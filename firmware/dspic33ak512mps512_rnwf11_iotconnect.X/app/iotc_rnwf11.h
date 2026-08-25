/**
 * @file iotc_rnwf11.h
 * @brief Glue between the ported RNWF11 AT-command driver (rnwf11/) and
 *        IoTConnect: WiFi connect, TLS+MQTT configuration/connect, and
 *        sending one telemetry sample (random number) via iotc-c-lib's
 *        JSON builder.
 */
#ifndef IOTC_RNWF11_H
#define IOTC_RNWF11_H

#include <stdbool.h>
#include "device_config.h"

/**
 * @brief Brings up UART2 + the RNWF11 AT-command interface and connects to
 *        WiFi using cfg->wifi_ssid/wifi_password. Blocks until connected,
 *        failed, or IOTC_RNWF11_WIFI_TIMEOUT_MS elapses.
 */
bool IOTC_RNWF11_ConnectWifi(const device_config_t *cfg);

/**
 * @brief Points RNWF11 TLS config slot 1 at the cert/key cfg already has
 *        uploaded to the module's filesystem (see
 *        tools/provision_rnwf11_cert.py), configures the MQTT client with
 *        cfg's broker/credentials, and connects. Blocks until connected,
 *        failed, or IOTC_RNWF11_MQTT_TIMEOUT_MS elapses.
 */
bool IOTC_RNWF11_ConnectMqtt(const device_config_t *cfg);

/**
 * @brief Builds one telemetry sample ({"random": <0-100>}) with iotc-c-lib
 *        and publishes it to cfg->mqtt_pub_topic at QoS 1.
 */
bool IOTC_RNWF11_SendTelemetry(const device_config_t *cfg);

/**
 * @brief Services the RNWF11 driver's async AT-command event queue (link
 *        up/down, MQTT connect/disconnect, etc.). Call this frequently
 *        from the main loop - it is non-blocking.
 */
void IOTC_RNWF11_Poll(void);

#endif // IOTC_RNWF11_H
