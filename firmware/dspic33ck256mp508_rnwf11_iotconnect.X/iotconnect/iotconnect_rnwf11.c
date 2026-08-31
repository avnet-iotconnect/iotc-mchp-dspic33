#include <stdio.h>
#include <string.h>
#include <xc.h>
#include "clock.h"
#include "uart1.h"
#include "iotconnect_rnwf11.h"
#include "iotconnect_rnwf11_config.h"

#define IOTC_RNWF11_RESPONSE_SIZE 192U
#define IOTC_RNWF11_COMMAND_TIMEOUT 5000000UL
#define IOTC_RNWF11_PROBE_TIMEOUT 200000UL

static bool iotcConnected;
static bool iotcModulePresent;
static volatile uint32_t telemetryMilliseconds;
static IOTC_RNWF11_Telemetry_t telemetry;

static bool IOTC_RNWF11_IsConfigured(void)
{
    return (IOTC_WIFI_SSID[0] != '\0') &&
           (IOTC_MQTT_BROKER_HOST[0] != '\0') &&
           (IOTC_MQTT_CLIENT_ID[0] != '\0') &&
           (IOTC_MQTT_TELEMETRY_TOPIC[0] != '\0');
}

static void IOTC_RNWF11_UART1_Initialize(void)
{
    UART1_Initialize();
    UART1_SpeedModeHighSpeed();
    UART1_BaudRateDividerSet((uint16_t)((FCY / (4UL * IOTC_RNWF11_BAUD)) - 1UL));
    UART1_ModuleEnable();
}

static void IOTC_RNWF11_Write(const char *text)
{
    while (*text != '\0')
    {
        while (U1STAHbits.UTXBF)
        {
        }
        U1TXREG = (uint8_t)*text++;
    }
}

static bool IOTC_RNWF11_CommandWithTimeout(const char *command, uint32_t timeout)
{
    char response[IOTC_RNWF11_RESPONSE_SIZE];
    uint16_t length = 0;

    while (!U1STAHbits.URXBE)
    {
        (void)U1RXREG;
    }

    IOTC_RNWF11_Write(command);
    while (timeout-- != 0U)
    {
        if (!U1STAHbits.URXBE)
        {
            char character = (char)U1RXREG;
            if (length < (sizeof(response) - 1U))
            {
                response[length++] = character;
                response[length] = '\0';
            }
            if (strstr(response, "ERROR") != NULL)
            {
                return false;
            }
            if (strstr(response, "OK") != NULL)
            {
                return true;
            }
        }
    }
    return false;
}

static bool IOTC_RNWF11_Command(const char *command)
{
    return IOTC_RNWF11_CommandWithTimeout(command, IOTC_RNWF11_COMMAND_TIMEOUT);
}

static bool IOTC_RNWF11_Configure(void)
{
    char command[256];

    if (!IOTC_RNWF11_Command("AT\r\n")) return false;
    snprintf(command, sizeof(command), "AT+WSTAC=1,\"%s\"\r\n", IOTC_WIFI_SSID);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+WSTAC=2,%u\r\n", IOTC_WIFI_SECURITY);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+WSTAC=3,\"%s\"\r\n", IOTC_WIFI_PASSWORD);
    if (!IOTC_RNWF11_Command(command)) return false;
    if (!IOTC_RNWF11_Command("AT+WSTA=1\r\n")) return false;

    snprintf(command, sizeof(command), "AT+TLSC=1,1,\"%s\"\r\n", IOTC_RNWF11_CA_NAME);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+TLSC=1,2,\"%s\"\r\n", IOTC_RNWF11_CERT_NAME);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+TLSC=1,3,\"%s\"\r\n", IOTC_RNWF11_KEY_NAME);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+TLSC=1,5,\"%s\"\r\n", IOTC_MQTT_BROKER_HOST);
    if (!IOTC_RNWF11_Command(command)) return false;

    snprintf(command, sizeof(command), "AT+MQTTC=1,\"%s\"\r\n", IOTC_MQTT_BROKER_HOST);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+MQTTC=2,%u\r\n", IOTC_MQTT_BROKER_PORT);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+MQTTC=3,\"%s\"\r\n", IOTC_MQTT_CLIENT_ID);
    if (!IOTC_RNWF11_Command(command)) return false;
    snprintf(command, sizeof(command), "AT+MQTTC=4,\"%s\"\r\n", IOTC_MQTT_USERNAME);
    if (!IOTC_RNWF11_Command(command)) return false;
    if (!IOTC_RNWF11_Command("AT+MQTTC=5,\"\"\r\n")) return false;
    if (!IOTC_RNWF11_Command("AT+MQTTC=6,60\r\n")) return false;
    if (!IOTC_RNWF11_Command("AT+MQTTC=7,1\r\n")) return false;
    if (!IOTC_RNWF11_Command("AT+MQTTC=8,3\r\n")) return false;
    return IOTC_RNWF11_Command("AT+MQTTCONN=1\r\n");
}

static void IOTC_RNWF11_PublishMotorState(void)
{
    char command[384];
    snprintf(command, sizeof(command),
             "AT+MQTTPUB=1,1,0,\"%s\",\"{\\\"motorRunning\\\":%u,\\\"state\\\":%u,\\\"sector\\\":%u,\\\"requestedSpeedRpm\\\":%u,\\\"measuredSpeedRpm\\\":%u,\\\"requestedCurrent\\\":%d,\\\"measuredCurrent\\\":%d,\\\"dutyCycle\\\":%d,\\\"dcBusAdc\\\":%d}\"\r\n",
             IOTC_MQTT_TELEMETRY_TOPIC, telemetry.motorRunning, telemetry.state,
             telemetry.sector, telemetry.requestedSpeedRpm, telemetry.measuredSpeedRpm,
             telemetry.requestedCurrent, telemetry.measuredCurrent, telemetry.dutyCycle,
             telemetry.dcBusAdc);
    (void)IOTC_RNWF11_Command(command);
}

void IOTC_RNWF11_Initialize(void)
{
#if IOTC_RNWF11_ENABLE
    IOTC_RNWF11_UART1_Initialize();
    iotcModulePresent = IOTC_RNWF11_CommandWithTimeout("AT\r\n", IOTC_RNWF11_PROBE_TIMEOUT);
    iotcConnected = false;
#else
    iotcModulePresent = false;
    iotcConnected = false;
#endif
}

void IOTC_RNWF11_Tick1ms(void)
{
#if IOTC_RNWF11_ENABLE
    telemetryMilliseconds++;
#endif
}

void IOTC_RNWF11_SetTelemetry(const IOTC_RNWF11_Telemetry_t *sample)
{
    telemetry = *sample;
}

void IOTC_RNWF11_Task(void)
{
#if IOTC_RNWF11_ENABLE
    if (!iotcModulePresent || !IOTC_RNWF11_IsConfigured())
    {
        return;
    }
    if (!iotcConnected)
    {
        iotcConnected = IOTC_RNWF11_Configure();
        return;
    }
    if (telemetryMilliseconds >= IOTC_TELEMETRY_PERIOD_MS)
    {
        telemetryMilliseconds = 0;
        IOTC_RNWF11_PublishMotorState();
    }
#endif
}

bool IOTC_RNWF11_IsConnected(void)
{
    return iotcConnected;
}
