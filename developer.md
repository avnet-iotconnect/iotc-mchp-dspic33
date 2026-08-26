# Developer Guide

This document covers building the firmware from source, the project's
architecture, and how to modify it. If you just want to run the quickstart
as-is, see [README.md](README.md) instead - it uses the pre-built
[`bin/dspic33ak512mps512_rnwf11_iotconnect.hex`](bin/) and doesn't require
MPLAB X at all.

## Table of Contents

1. [Building From Source](#1-building-from-source)
2. [Project Architecture](#2-project-architecture)
3. [Modifying the Firmware](#3-modifying-the-firmware)
4. [Provisioning Protocol Reference](#4-provisioning-protocol-reference)

## 1. Building From Source

### Requirements

- [MPLAB X IDE](https://www.microchip.com/mplabx) 6.25 or later
- XC-DSC compiler 3.21 or later
- The `dsPIC33AK-MP_DFP` device pack (Device Family Pack) - install via MPLAB X's Pack Manager if you don't already have it from the OOB demo

### Build Steps

1. Clone this repo, then initialize the `iotc-c-lib` submodule:
   ```bash
   git submodule update --init --recursive
   ```
2. Open [`firmware/dspic33ak512mps512_rnwf11_iotconnect.X`](firmware/dspic33ak512mps512_rnwf11_iotconnect.X) in MPLAB X.
3. Clean and Build. The output `.hex` lands in `dist/default/production/`.
4. Program via the onboard PKOB4 debugger (Make and Program Device), or with a standalone `.hex` via MPLAB IPE.

> [!NOTE]
> This project pins `dsPIC33AK-MP_DFP` 1.4.260 in `nbproject/configurations.xml`.
> If your installed pack is a different version, MPLAB X will prompt to
> resolve it on first open - usually just accept the update. If you're on an
> older pack and hit build errors in `mcc_generated_files/system/src/config_bits.c`
> about unrecognized values for `FICD_NOBTSWP` or `FWDT_RCLKSEL`, that's a
> config-bit enum rename between DFP releases (`OFF`&rarr;`BTSWP_DISABLED`,
> `BFRC256`&rarr;`BFRC244`) - same underlying setting, just renamed labels.

The build in this repo's `bin/` folder was produced from this exact source
with MPLAB X 6.35 / XC-DSC 4.00 / `dsPIC33AK-MP_DFP` 1.4.260, and verified to
compile and link cleanly (102,480 / 524,284 bytes flash, 10,860 / 65,536
bytes RAM) with no warnings outside Microchip's own ported `rnwf11/` driver
files (see [Section 2](#2-project-architecture) for why those are largely
untouched).

## 2. Project Architecture

```
dsPIC33AK512MPS512                RNWF11                      IoTConnect
+----------------+   AT commands  +--------------+   MQTT/TLS  +------------+
| main.c         |--- UART2 ----->| WiFi + MQTT  |------------>| Your       |
| iotc-c-lib     |   (mikroBUS A) | + TLS client |             | Device     |
| (JSON builder  |                | cert/key in  |             |            |
|  only)         |                | its own      |             |            |
+----------------+                | filesystem   |             +------------+
                                   +--------------+
```

No RTOS, no MQTT/TLS stack on the MCU - the RNWF11 owns the WiFi/MQTT/TLS
connection itself over AT commands, using a certificate and key stored on its
own filesystem (not the dsPIC33's).

### Firmware layout

- **`main.c`** - boot sequence: load config from flash (or wait for
  provisioning), connect WiFi, connect MQTT, then loop publishing telemetry.
- **`mcc_generated_files/`** - MCC Melody-generated clock/pin/UART1/UART2/TMR1
  drivers, trimmed down from the OOB demo (ADC/CAN/PWM peripherals and their
  driver files were dropped - this project only needs UART and a timer tick).
  `system/src/pins.c` is where UART2 gets routed to mikroBUS A via Peripheral
  Pin Select (RP72&rarr;U2TX, RP73&rarr;U2RX) - see the comments there for the
  exact data sheet register references.
- **`rnwf11/`** - Microchip's own RNWF11 AT-command service layer, ported
  from their [AVR128DB48/SAME54 reference firmware](https://github.com/MicrochipTech/AzureDemo_RNWF).
  This code turned out to be almost entirely portable C, sitting behind a
  thin UART interface struct (`UART_INTERFACE`, matching the same
  abstraction the OOB demo already uses for UART1) - only a handful of lines
  needed changing, each marked `// ported:` in a comment. Two things worth
  knowing if you're modifying this layer:
  - `RNWF_MQTT_SrvCtrl(RNWF_MQTT_CONFIG, ...)` is **not** used by this
    project - with `RNWF11_SERVICE` defined (required for this device),
    that vendor code path unconditionally also sends Azure-specific AT
    commands (`AT+MQTTC=10,1` server-select and an `AT+AZUREC` Device Twin
    model ID). `app/iotc_rnwf11.c`'s `IOTC_RNWF11_ConnectMqtt()`
    reimplements just the generic subset of that case instead, using the
    same public `RNWF_CMD_SEND_OK_WAIT()` macro the vendor code itself uses.
  - `rnwf_ecc_service.c` (the RNWF11's onboard ATECC608 secure-element path)
    is compiled in but never exercised at runtime - this project uses the
    plain `RNWF_NET_TLS_CONFIG_1` path (cert/key uploaded to the module's
    filesystem, see `tools/provision_rnwf11_cert.py`/`.ps1`) rather than
    `RNWF_NET_TLS_ECC608_CONFIG_1`.
- **`app/`** - the actual application logic, all original code:
  - `device_config.c`/`.h` - the WiFi/IoTConnect config struct, persisted to
    flash with a CRC32.
  - `provisioning.c`/`.h` - the line-based `PROVISION`/`KEY=VALUE`/`END`
    protocol over UART1 (see [Section 4](#4-provisioning-protocol-reference)).
  - `iotc_rnwf11.c`/`.h` - glue between the RNWF11 driver and
    [`Libraries/iotc-c-lib`](https://github.com/avnet-iotconnect/iotc-c-lib):
    builds telemetry JSON with iotc-c-lib's `iotcl_telemetry_*` functions
    (used standalone - iotc-c-lib doesn't bundle an MQTT/TLS client by
    design), then publishes it via `AT+MQTTPUB` through the RNWF11 driver.
- **`bsp/nvm_flash.c`** - hand-written (not MCC-generated) word-program/
  page-erase flash driver, from the data sheet's Flash Program Memory
  chapter. WiFi/IoTConnect config lives in the dsPIC33's own flash, not the
  RNWF11's filesystem, because the RNWF11's `AT+FS` filesystem only supports
  certificate/key files (`FILETYPE` 1=CERT/2=PRIKEY), not arbitrary config.
- **`bsp/systick.c`** - a 1ms tick off TMR1, used by `timer/delay.c` (which
  the ported RNWF11 driver needs) and the telemetry publish interval.
  Deliberately not the OOB demo's full cooperative task scheduler
  (`bsp/task.c`) - this project doesn't need dynamic task registration.

### Why resolve the MQTT broker/topic at provisioning time, not on every boot?

The RNWF11's own MQTT/TLS client handles the actual connection, but it has no
HTTPS/discovery capability of its own - only WiFi, sockets, TLS, and MQTT AT
commands. Doing IoTConnect's discovery+identity HTTPS round-trip on-device
would mean building an HTTP client on top of the RNWF11's raw TCP+TLS socket
AT commands. `tools/provision_device_config.py` (or its PowerShell
equivalent, `.ps1`, which calls the same discovery/identity REST endpoints
directly since `iotconnect-sdk-lite` is Python-only) does that resolution
once, on the PC, and bakes the result into the device's flash instead. If
IoTConnect ever reassigns your device to a different broker, re-run that
script.

## 3. Modifying the Firmware

- **Sending different telemetry**: edit `IOTC_RNWF11_SendTelemetry()` in
  `app/iotc_rnwf11.c` - it's a handful of `iotcl_telemetry_set_*()` calls.
  Update `templates/dspic33-rnwf11-quickstart-template.json`'s `attributes`
  to match whatever you add.
- **Changing the publish interval**: `TELEMETRY_PERIOD_MS` in `main.c`.
- **Using mikroBUS B instead of A**: `RP28`/`RP97` are mikroBUS B's TX/RX
  (see the data sheet's PPS tables); you'd change the `RPOR`/`RPINR` lines in
  `pins.c`, the `TRISx` bit for the new TX pin, and rebuild `uart2.c`'s pin
  comments accordingly. UART2 itself doesn't change.
- **Subscribing to Command/OTA messages (C2D)**: not implemented in this
  quickstart (out of scope - see the README's Introduction) - `rnwf_mqtt_service.c`'s
  `RNWF_MQTT_SUBSCRIBE_QOS0/1/2` requests and iotc-c-lib's `iotcl_c2d.c`
  (already compiled in) are the pieces you'd wire together.

## 4. Provisioning Protocol Reference

`app/provisioning.c` implements a line-based protocol over UART1 (the debug
console), driven by `tools/provision_device_config.py`/`.ps1`:

```
PROVISION
WIFI_SSID=<value>
WIFI_PASSWORD=<value>
IOTC_CPID=<value>
IOTC_ENV=<value>
IOTC_DUID=<value>
MQTT_BROKER_HOST=<value>
MQTT_BROKER_PORT=<value>
MQTT_USERNAME=<value>
MQTT_PUB_TOPIC=<value>
RNWF_CA_NAME=<value>
RNWF_KEY_NAME=<value>
RNWF_CERT_NAME=<value>
END
```

The device replies `OK` once the config is validated and written to flash,
or `ERROR:<reason>` (malformed line, unknown key, or a flash write failure).
Lines can arrive in any order; unrecognized keys are treated as a protocol
error rather than silently ignored, so a typo in a key name fails loudly
instead of leaving a field at its default.
