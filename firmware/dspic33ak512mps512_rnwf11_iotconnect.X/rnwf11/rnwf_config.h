/**
 * @file rnwf_config.h
 * @brief Stand-in for Microchip's rnwf_app.h, which rnwf_mqtt_service.c and
 *        rnwf_net_service.c #include under #ifdef RNWF11_SERVICE.
 *
 * rnwf_app.h in Microchip's reference firmware is ~150 lines of Azure-DPS
 * application config (WiFi credential placeholders, Azure telemetry JSON
 * formats, an LED state machine, etc.) - none of it is used by this
 * IoTConnect-based quickstart. Only two symbols from it are actually
 * referenced by the driver files we ported:
 *
 *  - AZURE_MODEL_ID: only used inside an Azure Device Provisioning Service
 *    message-format string in rnwf_mqtt_service.c that this project never
 *    calls (we don't use Azure DPS). Defined here only so the file compiles.
 *  - RNWF_TLS_ECC608_DEVTYPE: only referenced inside rnwf_net_service.c's
 *    RNWF_NET_TLS_ECC608_CONFIG_1/2 case, which is the code path for using
 *    the RNWF11's onboard ATECC608 secure element as the TLS identity. This
 *    project instead uses the plain RNWF_NET_TLS_CONFIG_1/2 path (a
 *    self-signed cert/key uploaded to the module's filesystem via AT+FS -
 *    see tools/provision_rnwf11_cert.py), so this code path is never
 *    exercised at runtime; the value just needs to exist to compile.
 */
#ifndef RNWF_CONFIG_H
#define RNWF_CONFIG_H

#define AZURE_MODEL_ID          "unused"
#define RNWF_TLS_ECC608_DEVTYPE 1 // 1=TrustNGo 2=TrustFlex - unused, see comment above

#endif // RNWF_CONFIG_H
