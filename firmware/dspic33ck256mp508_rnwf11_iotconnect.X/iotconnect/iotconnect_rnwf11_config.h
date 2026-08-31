#ifndef IOTCONNECT_RNWF11_CONFIG_H
#define IOTCONNECT_RNWF11_CONFIG_H

/* Set to 1 after completing the IoTConnect settings below. */
#define IOTC_RNWF11_ENABLE 1

/* RNWF11 uses 230400, 8-N-1 on the existing UART1 external interface. */
#define IOTC_RNWF11_BAUD 230400UL

/*
 * UART1 is mapped by hal/port_config.c to RP78 (RX, RD14) and RP77 (TX, RD13).
 * Wire RNWF11 TX to RD14 and RNWF11 RX to RD13. Enabling IoTConnect disables
 * X2C Scope because both protocols cannot share UART1.
 */

/* Wi-Fi credentials. */
#define IOTC_WIFI_SSID ""
#define IOTC_WIFI_PASSWORD ""
#define IOTC_WIFI_SECURITY 2U

/* IoTConnect MQTT endpoint values resolved during device provisioning. */
#define IOTC_MQTT_BROKER_HOST ""
#define IOTC_MQTT_BROKER_PORT 8883U
#define IOTC_MQTT_CLIENT_ID ""
#define IOTC_MQTT_USERNAME ""
#define IOTC_MQTT_TELEMETRY_TOPIC ""

/* RNWF11 filesystem names for the CA certificate, device certificate, and key. */
#define IOTC_RNWF11_CA_NAME "ca.pem"
#define IOTC_RNWF11_CERT_NAME "device.crt"
#define IOTC_RNWF11_KEY_NAME "device.key"

#define IOTC_TELEMETRY_PERIOD_MS 10000UL

#endif
