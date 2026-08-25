/**
 * @file device_config.h
 * @brief WiFi + IoTConnect connection settings, persisted in on-chip flash
 *        (see bsp/nvm_flash.c) and written by tools/provision_device_config.py
 *        over the UART1 debug console using a simple line-based protocol
 *        (see app/provisioning.c).
 */
#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define DEVICE_CONFIG_MAGIC 0x49435431UL // "ICT1"

typedef struct
{
    uint32_t magic;   // DEVICE_CONFIG_MAGIC if this page holds valid provisioned data
    uint32_t version;

    char wifi_ssid[33];
    char wifi_password[65];

    char iotc_cpid[17];
    char iotc_env[17];
    char iotc_duid[65];

    // Resolved once by tools/provision_device_config.py (IoTConnect DRA
    // discovery+identity) and baked in here, rather than the firmware doing
    // live HTTPS discovery itself - see the README for why.
    char mqtt_broker_host[128];
    uint16_t mqtt_broker_port;
    char mqtt_username[192]; // IoTConnect's device MQTT username, resolved at provisioning time; empty string if unused
    char mqtt_pub_topic[128]; // IoTConnect's per-device telemetry publish topic, also resolved at provisioning time - see the README for why this isn't computed on-device

    // Filenames of the CA/root, device cert, and device key already
    // uploaded to the RNWF11's own filesystem by
    // tools/provision_rnwf11_cert.py (AT+FS), so this firmware never
    // handles private key material directly.
    char rnwf_ca_name[33];
    char rnwf_cert_name[33];
    char rnwf_key_name[33];

    uint32_t crc32; // over every field above, see device_config.c
} device_config_t;

/**
 * @brief Loads the config from flash into *out.
 * @return true if a validly-provisioned config was found (magic + CRC32 match).
 *         false if the device has not been provisioned yet (or the flash
 *         page was erased/corrupt) - the caller should fall back to
 *         provisioning mode rather than trust *out.
 */
bool DEVICE_CONFIG_Load(device_config_t *out);

/**
 * @brief Erases the config flash page and writes *cfg (with a freshly
 *        computed CRC32 and DEVICE_CONFIG_MAGIC).
 * @return true on success.
 */
bool DEVICE_CONFIG_Save(device_config_t *cfg);

#endif // DEVICE_CONFIG_H
