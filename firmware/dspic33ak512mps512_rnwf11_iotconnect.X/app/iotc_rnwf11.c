#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "iotc_rnwf11.h"
#include "../mcc_generated_files/uart/uart2.h"
#include "../bsp/systick.h"
#include "../rnwf11/rnwf_interface.h"
#include "../rnwf11/rnwf_wifi_service.h"
#include "../rnwf11/rnwf_net_service.h"
#include "../rnwf11/rnwf_mqtt_service.h"
#include "../Libraries/iotc-c-lib/core/include/iotcl.h"
#include "../Libraries/iotc-c-lib/core/include/iotcl_telemetry.h"

#define WIFI_CONNECT_TIMEOUT_MS 20000UL
#define MQTT_CONNECT_TIMEOUT_MS 15000UL

static volatile bool s_wifi_up = false;
static volatile bool s_wifi_failed = false;
static volatile bool s_mqtt_up = false;
static volatile bool s_mqtt_failed = false;

static void WifiEventCallback(RNWF_WIFI_EVENT_t event, uint8_t *msg)
{
    (void)msg;
    switch (event)
    {
    case RNWF_DHCP_DONE:
        s_wifi_up = true;
        break;
    case RNWF_DISCONNECTED:
    case RNWF_CONNECT_FAILED:
        s_wifi_failed = true;
        break;
    default:
        break;
    }
}

static RNWF_RESULT_t MqttEventCallback(RNWF_MQTT_EVENT_t event, uint8_t *msg)
{
    (void)msg;
    if (event == RNWF_MQTT_CONNECTED)
    {
        s_mqtt_up = true;
    }
    else if (event == RNWF_MQTT_DISCONNECTED)
    {
        // The module sends this (as "+MQTTCONN:0") right after a rejected
        // connect attempt too, not just after a connection that later
        // drops - treat it as an immediate failure signal during connect
        // rather than silently waiting out the full timeout with no idea
        // why.
        s_mqtt_up = false;
        s_mqtt_failed = true;
    }
    return RNWF_PASS;
}

bool IOTC_RNWF11_ConnectWifi(const device_config_t *cfg)
{
    UART2_Initialize();
    RNWF_IF_Init();

    s_wifi_up = false;
    s_wifi_failed = false;

    RNWF_WIFI_SrvCtrl(RNWF_WIFI_SET_CALLBACK, (void *)&WifiEventCallback);

    RNWF_WIFI_PARAM_t param = {
        .mode = RNWF_WIFI_MODE_STA,
        .ssid = cfg->wifi_ssid,
        .passphrase = cfg->wifi_password,
        .security = RNWF_WPA2_MIXED, // matches the RNWF11 reference firmware's default; change if your AP needs WPA3
        .autoconnect = 0,
    };
    RNWF_WIFI_SrvCtrl(RNWF_SET_WIFI_PARAMS, &param);
    RNWF_WIFI_SrvCtrl(RNWF_STA_CONNECT, NULL);

    uint32_t start = SYSTICK_GetMillis();
    while (!s_wifi_up && !s_wifi_failed)
    {
        // The RNWF11 driver has no internal task/poll loop of its own for
        // async events beyond what RNWF_CMD_RSP_Send() drains inline on
        // each command; RNWF_EVENT_Handler() below services the async
        // event queue (+WSTA.../+MQTT... lines) between our AT commands.
        RNWF_EVENT_Handler();
        if ((uint32_t)(SYSTICK_GetMillis() - start) > WIFI_CONNECT_TIMEOUT_MS)
        {
            printf("WiFi connect timed out\r\n");
            return false;
        }
    }
    return s_wifi_up;
}

bool IOTC_RNWF11_ConnectMqtt(const device_config_t *cfg)
{
    // Point TLS config slot 1 at the CA/cert/key already uploaded to the
    // RNWF11's own filesystem (tools/provision_rnwf11_cert.py) - see
    // rnwf_net_service.h's RNWF_NET_TLS_CA_CERT.. index enum.
    // DOMAIN_NAME (id 6) is deliberately left NULL/unset: it has a much
    // shorter max length than SERVER_NAME (id 5) and AWS IoT's full
    // endpoint hostname doesn't fit, which the module rejects with
    // "ERROR:0.4,Invalid Parameter" - SERVER_NAME alone is sufficient for
    // the TLS SNI hostname.
    const char *tls_cfg_list[6] = {
        [RNWF_NET_TLS_CA_CERT] = cfg->rnwf_ca_name,
        [RNWF_NET_TLS_CERT_NAME] = cfg->rnwf_cert_name,
        [RNWF_NET_TLS_KEY_NAME] = cfg->rnwf_key_name,
        [RNWF_NET_TLS_KEY_PWD] = NULL,
        [RNWF_NET_TLS_SERVER_NAME] = cfg->mqtt_broker_host,
        [RNWF_NET_TLS_DOMAIN_NAME] = NULL,
    };
    RNWF_NET_SOCK_SrvCtrl(RNWF_NET_TLS_CONFIG_1, tls_cfg_list);

    // Deliberately NOT using RNWF_MQTT_SrvCtrl(RNWF_MQTT_CONFIG, ...) here -
    // with RNWF11_SERVICE defined, that wrapper unconditionally also sends
    // Azure-specific AT+MQTTC=10,1 (server-select=Azure) and an
    // AT+AZUREC=1,... Device Twin model ID command, neither of which
    // applies to an IoTConnect connection. This reimplements just the
    // generic subset of what that case does, using the same public
    // RNWF_CMD_SEND_OK_WAIT() macro (rnwf11/rnwf_interface.h) the vendor
    // code itself uses.
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_PROTO_VER, 3);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_TLS_CONF, RNWF_NET_TLS_CONFIG_1);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_BROKER_URL, cfg->mqtt_broker_host);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_BROKER_PORT, cfg->mqtt_broker_port);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_CLIENT_ID, cfg->iotc_duid);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_USERNAME, cfg->mqtt_username);
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_PASSWORD, "");
    RNWF_CMD_SEND_OK_WAIT(NULL, NULL, RNWF_MQTT_SET_KEEPALIVE, 60);

    s_mqtt_up = false;
    s_mqtt_failed = false;
    RNWF_MQTT_SrvCtrl(RNWF_MQTT_SET_CALLBACK, (void *)&MqttEventCallback);
    RNWF_MQTT_SrvCtrl(RNWF_MQTT_CONNECT, NULL);

    uint32_t start = SYSTICK_GetMillis();
    while (!s_mqtt_up && !s_mqtt_failed)
    {
        RNWF_EVENT_Handler();
        if ((uint32_t)(SYSTICK_GetMillis() - start) > MQTT_CONNECT_TIMEOUT_MS)
        {
            printf("MQTT connect timed out\r\n");
            return false;
        }
    }
    if (s_mqtt_failed)
    {
        printf("MQTT connect rejected by broker\r\n");
    }
    return s_mqtt_up;
}

#define MQTT_PUB_ESCAPED_MSG_MAX 256U

// AT+MQTTPUB wraps the message in "..." with no escaping of its own
// (RNWF_MQTT_CMD_PUBLISH in rnwf_mqtt_service.h) - any '"' inside the JSON
// payload itself prematurely closes that quoted argument and the module
// replies "ERROR:0.5,Incorrect Number of Parameters" instead of publishing.
static bool EscapeJsonForAtCommand(const char *json, char *out, size_t out_size)
{
    size_t o = 0;
    for (size_t i = 0; json[i] != '\0'; i++)
    {
        char c = json[i];
        bool needs_escape = (c == '"' || c == '\\');
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

bool IOTC_RNWF11_SendTelemetry(const device_config_t *cfg)
{
    IotclMessageHandle msg = iotcl_telemetry_create();
    if (msg == NULL)
    {
        printf("iotcl_telemetry_create failed\r\n");
        return false;
    }

    // Simple dummy telemetry, matching the other iotc quickstarts in this
    // family (see the iotc-python-lite-sdk-demos repo's app.py) - no real
    // sensor on this board is read here.
    iotcl_telemetry_set_number(msg, "random", (double)(rand() % 101));

    char *json = iotcl_telemetry_create_serialized_string(msg, false);
    iotcl_telemetry_destroy(msg);
    if (json == NULL)
    {
        printf("iotcl_telemetry_create_serialized_string failed\r\n");
        return false;
    }

    char escaped[MQTT_PUB_ESCAPED_MSG_MAX];
    if (!EscapeJsonForAtCommand(json, escaped, sizeof(escaped)))
    {
        printf("Telemetry JSON too long to send\r\n");
        iotcl_telemetry_destroy_serialized_string(json);
        return false;
    }

    RNWF_MQTT_FRAME_t frame = {
        .isNew = NEW_MSG,
        .qos = MQTT_QOS1,
        .isRetain = NO_RETAIN,
        .topic = cfg->mqtt_pub_topic,
        .message = escaped,
    };
    RNWF_RESULT_t result = RNWF_MQTT_SrvCtrl(RNWF_MQTT_PUBLISH, &frame);

    printf("Published: %s\r\n", json);
    iotcl_telemetry_destroy_serialized_string(json);

    return result == RNWF_PASS;
}

void IOTC_RNWF11_Poll(void)
{
    RNWF_EVENT_Handler();
}
