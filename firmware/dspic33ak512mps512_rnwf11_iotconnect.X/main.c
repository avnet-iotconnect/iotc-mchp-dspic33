/**
 * @file main.c
 * @brief dsPIC33AK512MPS512 + RNWF11 UART-to-Cloud Add-on Board: baremetal
 *        IoTConnect quickstart. Connects to WiFi and an IoTConnect MQTT
 *        broker through the RNWF11's own AT-command interface (no MQTT/TLS
 *        stack runs on this MCU - the module owns the connection, using a
 *        cert/key already stored in its own filesystem), then sends a
 *        random-number telemetry sample every TELEMETRY_PERIOD_MS.
 *
 * See the top-level README for the one-time provisioning steps
 * (tools/provision_rnwf11_cert.py, then tools/provision_device_config.py)
 * this firmware waits for on first boot.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mcc_generated_files/system/system.h"
#include "bsp/systick.h"
#include "timer/delay.h"
#include "app/device_config.h"
#include "app/provisioning.h"
#include "app/iotc_rnwf11.h"
#include "Libraries/iotc-c-lib/core/include/iotcl.h"

#define TELEMETRY_PERIOD_MS 10000UL
#define RECONNECT_DELAY_MS  5000U

static void WaitForProvisioning(device_config_t *cfg)
{
    printf("\r\nNot provisioned yet.\r\n");
    printf("On your PC: run tools/provision_rnwf11_cert.py once (RNWF11 connected\r\n");
    printf("directly to USB), then tools/provision_device_config.py against this\r\n");
    printf("board's console port. Waiting...\r\n");

    for (;;)
    {
        if (PROVISIONING_CheckForRequest())
        {
            if (PROVISIONING_RunOnce(cfg))
            {
                return;
            }
            printf("Provisioning failed - waiting for retry...\r\n");
        }
    }
}

int main(void)
{
    SYSTEM_Initialize();
    SYSTICK_Initialize();

    printf("\r\n\r\n=== dsPIC33AK512MPS512 + RNWF11 IoTConnect Quickstart ===\r\n");

    device_config_t cfg;
    if (!DEVICE_CONFIG_Load(&cfg))
    {
        WaitForProvisioning(&cfg);
    }
    else
    {
        printf("Loaded provisioned config for DUID \"%s\"\r\n", cfg.iotc_duid);
    }

    for (;;)
    {
        printf("Connecting to WiFi SSID \"%s\"...\r\n", cfg.wifi_ssid);
        if (!IOTC_RNWF11_ConnectWifi(&cfg))
        {
            printf("WiFi connect failed, retrying in %ums\r\n", RECONNECT_DELAY_MS);
            DELAY_milliseconds(RECONNECT_DELAY_MS);
            continue;
        }

        printf("WiFi connected. Connecting to IoTConnect MQTT broker \"%s\"...\r\n", cfg.mqtt_broker_host);
        if (!IOTC_RNWF11_ConnectMqtt(&cfg))
        {
            printf("MQTT connect failed, retrying in %ums\r\n", RECONNECT_DELAY_MS);
            DELAY_milliseconds(RECONNECT_DELAY_MS);
            continue;
        }

        printf("Connected. Sending telemetry every %lums.\r\n", TELEMETRY_PERIOD_MS);

        IotclClientConfig iotcl_cfg;
        iotcl_init_client_config(&iotcl_cfg);
        iotcl_cfg.device.instance_type = IOTCL_DCT_CUSTOM; // we resolve broker/topic ourselves at provisioning time - see device_config.h
        iotcl_init(&iotcl_cfg);

        uint32_t last_publish_ms = 0;
        bool connection_lost = false;
        while (!connection_lost)
        {
            IOTC_RNWF11_Poll();

            uint32_t now = SYSTICK_GetMillis();
            if ((uint32_t)(now - last_publish_ms) >= TELEMETRY_PERIOD_MS)
            {
                last_publish_ms = now;
                if (!IOTC_RNWF11_SendTelemetry(&cfg))
                {
                    printf("Publish failed - reconnecting\r\n");
                    connection_lost = true;
                }
            }

            if (PROVISIONING_CheckForRequest())
            {
                device_config_t new_cfg;
                if (PROVISIONING_RunOnce(&new_cfg))
                {
                    cfg = new_cfg;
                    connection_lost = true; // reconnect with the freshly provisioned settings
                }
            }
        }
    }
}
