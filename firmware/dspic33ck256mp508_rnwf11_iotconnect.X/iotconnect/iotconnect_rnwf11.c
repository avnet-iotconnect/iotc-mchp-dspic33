#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xc.h>
#include "clock.h"
#include <libpic30.h> /* needs FCY from clock.h */
#include "debug_console.h"
#include "uart2.h"
#include "iotconnect_rnwf11.h"
#include "iotconnect_rnwf11_config.h"
#include "iotcl.h"
#include "iotcl_telemetry.h"

#define IOTC_RNWF11_RESPONSE_SIZE 384U
#define IOTC_RNWF11_COMMAND_TIMEOUT 5000000UL
#define IOTC_RNWF11_PROBE_TIMEOUT 200000UL
#define IOTC_RNWF11_TIME_WAIT_MS 15000UL
#define IOTC_RNWF11_MIN_NTP_SECONDS 3000000000UL
#define IOTC_RNWF11_WIFI_WAIT_MS 20000UL

static bool iotcConnected;
static bool iotcModulePresent;
static volatile uint32_t telemetryMilliseconds;
static volatile uint32_t retryMilliseconds;
static IOTC_RNWF11_Telemetry_t telemetry;
static char lastResponse[IOTC_RNWF11_RESPONSE_SIZE];
static bool lastOverrun;
static bool mqttLinkUp;
static volatile bool moduleTimeValid;
/* Assume up: the module stays associated across dsPIC resets and only
 * re-announces its IP on a fresh association. */
static bool netUp = true;

static bool IOTC_RNWF11_IsConfigured(void)
{
    return (IOTC_WIFI_SSID[0] != '\0') &&
           (IOTC_MQTT_BROKER_HOST[0] != '\0') &&
           (IOTC_MQTT_CLIENT_ID[0] != '\0') &&
           (IOTC_MQTT_TELEMETRY_TOPIC[0] != '\0');
}

static void IOTC_RNWF11_UART2_Initialize(void)
{
    UART2_Initialize(IOTC_RNWF11_BAUD);
}

/* The module echoes every character, so keep draining while transmitting. */
static void IOTC_RNWF11_Collect(uint16_t *length)
{
    while (UART2_IsReceiveDataAvailable())
    {
        char c = (char)UART2_Read();
        if (*length < (IOTC_RNWF11_RESPONSE_SIZE - 1U))
        {
            lastResponse[*length] = c;
            (*length)++;
            lastResponse[*length] = '\0';
        }
    }
}

/* The module echoes every character; discard it so long commands still fit. */
static void IOTC_RNWF11_DiscardEcho(void)
{
    while (UART2_IsReceiveDataAvailable())
    {
        (void)UART2_Read();
    }
}

static void IOTC_RNWF11_Write(const char *text)
{
    while (*text != '\0')
    {
        UART2_Write((uint8_t)*text++);
        while (!UART2_IsTransmitComplete())
        {
            IOTC_RNWF11_DiscardEcho();
        }
        IOTC_RNWF11_DiscardEcho();
    }
}

static void IOTC_RNWF11_PollEvents(void);

static bool IOTC_RNWF11_CommandWithTimeout(const char *command, uint32_t timeout)
{
    uint16_t length = 0;

    /* Print anything pending rather than discarding a failure notification. */
    IOTC_RNWF11_PollEvents();

    lastResponse[0] = '\0';
    UART2_ReceiveFlush();
    lastOverrun = false;

    IOTC_RNWF11_Write(command);

    while (timeout-- != 0U)
    {
        IOTC_RNWF11_Collect(&length);
        if (UART2_IsOverrun())
        {
            lastOverrun = true;
            UART2_ClearOverrun();
        }
        if (strstr(lastResponse, "ERROR") != NULL)
        {
            return false;
        }
        if (strstr(lastResponse, "OK") != NULL)
        {
            return true;
        }
    }
    return false;
}

static bool IOTC_RNWF11_Command(const char *command)
{
    return IOTC_RNWF11_CommandWithTimeout(command, IOTC_RNWF11_COMMAND_TIMEOUT);
}

/* Labelled so failures are traceable without printing Wi-Fi credentials. */
static bool IOTC_RNWF11_Step(const char *label, const char *command)
{
    if (IOTC_RNWF11_Command(command))
    {
        return true;
    }
    DEBUG_Printf("IOTC: step %s failed%s, resp=[%s]\r\n",
                 label, lastOverrun ? " (rx overrun)" : "",
                 (lastResponse[0] != '\0') ? lastResponse : "<timeout>");
    return false;
}

/* Sends a query and prints whatever comes back, ignoring OK/ERROR. */
static void IOTC_RNWF11_Query(const char *command)
{
    uint16_t length = 0;

    lastResponse[0] = '\0';
    UART2_ReceiveFlush();
    IOTC_RNWF11_Write(command);

    for (uint16_t slice = 0; slice < 2000U; slice++)
    {
        __delay_us(250);
        IOTC_RNWF11_Collect(&length);
    }
    DEBUG_Printf("IOTC: query -> [%s]\r\n", lastResponse);
}

static void IOTC_RNWF11_Diagnose(void)
{
    static const char *const queries[] = {
        "AT+TIME\r\n",     /* confirm SNTP supplied a valid clock */
        "AT+TLSC=1\r\n",   /* did the certificate names actually stick */
        "AT+MQTTC\r\n",    /* host, port, client id, TLS selection */
    };

    DEBUG_Printf("IOTC: --- diagnostics ---\r\n");
    for (uint16_t i = 0; i < (sizeof(queries) / sizeof(queries[0])); i++)
    {
        IOTC_RNWF11_Query(queries[i]);
    }
    DEBUG_Printf("IOTC: --- end ---\r\n");
}

/* Expected to fail once the station is associated, so do not log it. */
static void IOTC_RNWF11_StepQuiet(const char *command)
{
    (void)IOTC_RNWF11_Command(command);
}

/* Best effort: the module may already keep time, so a failure is not fatal. */
static void IOTC_RNWF11_StepOptional(const char *label, const char *command)
{
    (void)IOTC_RNWF11_Step(label, command);
}

static bool IOTC_RNWF11_WaitForValidTime(void)
{
    moduleTimeValid = false;
    for (uint32_t elapsed = 0; elapsed < IOTC_RNWF11_TIME_WAIT_MS; elapsed++)
    {
        (void)IOTC_RNWF11_Command("AT+TIME\r\n");
        IOTC_RNWF11_PollEvents();
        if (moduleTimeValid)
        {
            return true;
        }
        __delay_ms(1);
    }
    DEBUG_Printf("IOTC: step TIME-sync failed, resp=[%s]\r\n",
                 (lastResponse[0] != '\0') ? lastResponse : "<timeout>");
    return false;
}

static bool IOTC_RNWF11_WaitForNetwork(void)
{
    netUp = false;
    IOTC_RNWF11_StepQuiet("AT+WSTA=1\r\n");

    for (uint32_t elapsed = 0; elapsed < IOTC_RNWF11_WIFI_WAIT_MS; elapsed++)
    {
        IOTC_RNWF11_PollEvents();
        if (netUp)
        {
            return true;
        }
        __delay_ms(1);
    }
    DEBUG_Printf("IOTC: step WiFi-IP failed\r\n");
    return false;
}

static bool IOTC_RNWF11_Configure(void)
{
    char command[256];

    /* Order follows RNWF11 Developer's Guide, Appendix A.6 (AWS via AT). */
    if (!IOTC_RNWF11_Step("AT", "AT\r\n")) return false;

    snprintf(command, sizeof(command), "AT+TLSC=1,1,\"%s\"\r\n", IOTC_RNWF11_CA_NAME);
    if (!IOTC_RNWF11_Step("TLSC1-ca", command)) return false;
    if (IOTC_RNWF11_CERT_NAME[0] != '\0')
    {
        snprintf(command, sizeof(command), "AT+TLSC=1,2,\"%s\"\r\n", IOTC_RNWF11_CERT_NAME);
        if (!IOTC_RNWF11_Step("TLSC2-cert", command)) return false;
    }
    if (IOTC_RNWF11_KEY_NAME[0] != '\0')
    {
        snprintf(command, sizeof(command), "AT+TLSC=1,3,\"%s\"\r\n", IOTC_RNWF11_KEY_NAME);
        if (!IOTC_RNWF11_Step("TLSC3-key", command)) return false;
    }
    snprintf(command, sizeof(command), "AT+TLSC=1,5,\"%s\"\r\n", IOTC_MQTT_BROKER_HOST);
    if (!IOTC_RNWF11_Step("TLSC5-servername", command)) return false;
    IOTC_RNWF11_StepOptional("TLSC8-verify", "AT+TLSC=1,8,1\r\n");

    /* These are rejected while the station is already associated, which is fine. */
    snprintf(command, sizeof(command), "AT+WSTAC=1,\"%s\"\r\n", IOTC_WIFI_SSID);
    IOTC_RNWF11_StepQuiet(command);
    snprintf(command, sizeof(command), "AT+WSTAC=2,%u\r\n", IOTC_WIFI_SECURITY);
    IOTC_RNWF11_StepQuiet(command);
    snprintf(command, sizeof(command), "AT+WSTAC=3,\"%s\"\r\n", IOTC_WIFI_PASSWORD);
    IOTC_RNWF11_StepQuiet(command);
    IOTC_RNWF11_StepQuiet("AT+WSTAC=4,0\r\n");
    if (!IOTC_RNWF11_WaitForNetwork()) return false;

    /* AWS TLS certificate validation requires a valid module clock. */
    /* Existing SNTP configuration may reject updates with status 0.6. */
    IOTC_RNWF11_StepQuiet("AT+SNTPC=3,\"pool.ntp.org\"\r\n");
    IOTC_RNWF11_StepQuiet("AT+SNTPC=2,1\r\n");
    IOTC_RNWF11_StepQuiet("AT+SNTPC=1\r\n");
    if (!IOTC_RNWF11_WaitForValidTime()) return false;

    snprintf(command, sizeof(command), "AT+MQTTC=1,\"%s\"\r\n", IOTC_MQTT_BROKER_HOST);
    if (!IOTC_RNWF11_Step("MQTTC1-host", command)) return false;
    snprintf(command, sizeof(command), "AT+MQTTC=2,%u\r\n", IOTC_MQTT_BROKER_PORT);
    if (!IOTC_RNWF11_Step("MQTTC2-port", command)) return false;
    snprintf(command, sizeof(command), "AT+MQTTC=3,\"%s\"\r\n", IOTC_MQTT_CLIENT_ID);
    if (!IOTC_RNWF11_Step("MQTTC3-clientid", command)) return false;
    /* Match the working RNWF11 flow: use the MQTT protocol version it sets. */
    if (!IOTC_RNWF11_Step("MQTTC8-protocol", "AT+MQTTC=8,3\r\n")) return false;
    /*
     * Always written, even when empty: the module keeps the previous value
     * across resets, and AWS rejects a CONNECT that carries a username.
     */
    snprintf(command, sizeof(command), "AT+MQTTC=4,\"%s\"\r\n", IOTC_MQTT_USERNAME);
    IOTC_RNWF11_StepOptional("MQTTC4-user", command);
    IOTC_RNWF11_StepOptional("MQTTC5-pass", "AT+MQTTC=5,\"\"\r\n");
    if (!IOTC_RNWF11_Step("MQTTC7-tls", "AT+MQTTC=7,1\r\n")) return false;
    if (!IOTC_RNWF11_Step("MQTTC6-keepalive", "AT+MQTTC=6,60\r\n")) return false;
    if (!IOTC_RNWF11_Step("MQTTCONN", "AT+MQTTCONN=1\r\n"))
    {
        return false;
    }

    /* The broker result arrives asynchronously; watch for it rather than guess. */
    for (uint16_t slice = 0; slice < 10000U; slice++)
    {
        __delay_us(500);
        IOTC_RNWF11_PollEvents();
        if (mqttLinkUp)
        {
            break;
        }
    }
    return mqttLinkUp;
}

#define IOTC_RNWF11_ESCAPED_JSON_MAX 256U

/* AT+MQTTPUB wraps the message in "..." with no escaping of its own - any
 * '"' inside the JSON payload prematurely closes that quoted argument. Keys
 * stay abbreviated (see IOTC_RNWF11_PublishMotorState) since iotc-c-lib's
 * "d"-envelope already adds back some of the line-length budget that saved. */
static bool IOTC_RNWF11_EscapeJsonForAtCommand(const char *json, char *out, size_t out_size)
{
    size_t o = 0;
    for (size_t i = 0; json[i] != '\0'; i++)
    {
        char c = json[i];
        bool needs_escape = (c == '"') || (c == '\\');
        size_t needed = needs_escape ? 2U : 1U;
        if (o + needed >= out_size)
        {
            return false;
        }
        if (needs_escape)
        {
            out[o++] = '\\';
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return true;
}

static bool IOTC_RNWF11_PublishMotorState(void)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (msg == NULL)
    {
        DEBUG_Printf("IOTC: iotcl_telemetry_create failed\r\n");
        return false;
    }

    /* Keys are abbreviated: the full names overflow the module's AT line limit. */
    iotcl_telemetry_set_number(msg, "run", telemetry.motorRunning);
    iotcl_telemetry_set_number(msg, "st", telemetry.state);
    iotcl_telemetry_set_number(msg, "sec", telemetry.sector);
    iotcl_telemetry_set_number(msg, "rpm", telemetry.requestedSpeedRpm);
    iotcl_telemetry_set_number(msg, "spd", telemetry.measuredSpeedRpm);
    iotcl_telemetry_set_number(msg, "ic", telemetry.requestedCurrent);
    iotcl_telemetry_set_number(msg, "im", telemetry.measuredCurrent);
    iotcl_telemetry_set_number(msg, "duty", telemetry.dutyCycle);
    iotcl_telemetry_set_number(msg, "vdc", telemetry.dcBusAdc);

    char *json = iotcl_telemetry_create_serialized_string(msg, false);
    iotcl_telemetry_destroy(msg);
    if (json == NULL)
    {
        DEBUG_Printf("IOTC: iotcl_telemetry_create_serialized_string failed\r\n");
        return false;
    }

    char escaped[IOTC_RNWF11_ESCAPED_JSON_MAX];
    bool escapedOk = IOTC_RNWF11_EscapeJsonForAtCommand(json, escaped, sizeof(escaped));

    bool sent = false;
    if (!escapedOk)
    {
        DEBUG_Printf("IOTC: telemetry JSON too long to escape (%s)\r\n", json);
    }
    else
    {
        char command[384];
        int written = snprintf(command, sizeof(command), "AT+MQTTPUB=1,1,0,\"%s\",\"%s\"\r\n",
                                IOTC_MQTT_TELEMETRY_TOPIC, escaped);
        if ((written < 0) || ((size_t)written >= sizeof(command)))
        {
            DEBUG_Printf("IOTC: telemetry command too long to send\r\n");
        }
        else
        {
            sent = IOTC_RNWF11_Step("MQTTPUB", command);
        }
    }

    iotcl_telemetry_destroy_serialized_string(json);
    return sent;
}

void IOTC_RNWF11_Initialize(void)
{
#if IOTC_RNWF11_ENABLE
    IOTC_RNWF11_UART2_Initialize();
    iotcModulePresent = IOTC_RNWF11_CommandWithTimeout("AT\r\n", IOTC_RNWF11_PROBE_TIMEOUT);
    iotcConnected = false;

    IotclClientConfig iotcl_cfg;
    iotcl_init_client_config(&iotcl_cfg);
    iotcl_cfg.device.instance_type = IOTCL_DCT_CUSTOM; // broker/topic are resolved at provisioning time - see iotconnect_rnwf11_config.h
    iotcl_init(&iotcl_cfg);

#if !IOTC_RNWF11_RX_RPn
    DEBUG_Printf("IOTC: UART2 pins unset, set IOTC_RNWF11_RX_RPn/TX_RPnR\r\n");
#endif
    DEBUG_Printf("IOTC: module %s, config %s\r\n",
                 iotcModulePresent ? "detected" : "not responding",
                 IOTC_RNWF11_IsConfigured() ? "ok" : "incomplete");
#else
    iotcModulePresent = false;
    iotcConnected = false;
#endif
}

void IOTC_RNWF11_Tick1ms(void)
{
#if IOTC_RNWF11_ENABLE
    telemetryMilliseconds++;
    retryMilliseconds++;
#endif
}

void IOTC_RNWF11_SetTelemetry(const IOTC_RNWF11_Telemetry_t *sample)
{
    telemetry = *sample;
}

/*
 * The module reports real connection state asynchronously; command replies only
 * say whether the command itself was accepted.
 */
static void IOTC_RNWF11_PollEvents(void)
{
    static char line[128];
    static uint16_t len;

    while (UART2_IsReceiveDataAvailable())
    {
        char c = (char)UART2_Read();
        if ((c == '\r') || (c == '\n'))
        {
            if (len > 0U)
            {
                line[len] = '\0';
                DEBUG_Printf("IOTC: event %s\r\n", line);
                if (strstr(line, "+MQTTCONN:1") != NULL)
                {
                    mqttLinkUp = true;
                }
                else if (strstr(line, "+MQTTCONN:0") != NULL)
                {
                    mqttLinkUp = false;
                }
                else if (strstr(line, "+WSTAAIP:1") != NULL)
                {
                    netUp = true;
                }
                else if (strstr(line, "+WSTALU:0") != NULL)
                {
                    netUp = false;
                    mqttLinkUp = false;
                }
                else if (strstr(line, "+TIME:") != NULL)
                {
                    char *time_value = strstr(line, "+TIME:");
                    if (strtoul(time_value + 6, NULL, 10) >= IOTC_RNWF11_MIN_NTP_SECONDS)
                    {
                        moduleTimeValid = true;
                    }
                }
                else if (strstr(line, "20.2") != NULL)
                {
                    /* The module reports this when STA is already connected. */
                    netUp = true;
                }
                len = 0;
            }
        }
        else if (len < (sizeof(line) - 1U))
        {
            line[len++] = c;
        }
    }
}

void IOTC_RNWF11_Task(void)
{
#if IOTC_RNWF11_ENABLE
    if (!iotcModulePresent || !IOTC_RNWF11_IsConfigured())
    {
        return;
    }
    IOTC_RNWF11_PollEvents();
    if (!iotcConnected)
    {
        if (!netUp)
        {
            return;
        }
        if (retryMilliseconds < IOTC_RETRY_PERIOD_MS)
        {
            return;
        }
        retryMilliseconds = 0;
        iotcConnected = IOTC_RNWF11_Configure();
        DEBUG_Printf("IOTC: provisioning %s\r\n", iotcConnected ? "complete" : "failed");
#if IOTC_DIAG_QUERIES
        IOTC_RNWF11_Diagnose();
#endif
        /* MQTTCONN only means accepted; the broker link is reported later. */
        mqttLinkUp = false;
        telemetryMilliseconds = 0;
        return;
    }
    if (telemetryMilliseconds >= IOTC_TELEMETRY_PERIOD_MS)
    {
        telemetryMilliseconds = 0;
        if (!mqttLinkUp)
        {
            DEBUG_Printf("IOTC: broker not connected, retrying\r\n");
            iotcConnected = false;
            retryMilliseconds = 0;
            return;
        }
        bool sent = IOTC_RNWF11_PublishMotorState();
        DEBUG_Printf("IOTC: publish %s state=%u rpm=%u duty=%d\r\n",
                     sent ? "ok" : "FAILED",
                     telemetry.state, telemetry.measuredSpeedRpm, telemetry.dutyCycle);
    }
#endif
}

bool IOTC_RNWF11_IsConnected(void)
{
    return iotcConnected;
}
