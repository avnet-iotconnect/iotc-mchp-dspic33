# dsPIC33AK512MPS512 + RNWF11 IoTConnect Quickstart

A baremetal C quickstart connecting the Microchip **dsPIC33AK512MPS512** (on the
**dsPIC33 Curiosity Platform Development Board**) to
[Avnet IoTConnect](https://www.iotconnect.io/) using IoTConnect's
[C SDK](https://github.com/avnet-iotconnect/iotc-c-lib), over a Microchip
**RNWF11 UART to Cloud Add-on Board**. No RTOS, no MQTT/TLS stack on the MCU -
the RNWF11 owns the WiFi/MQTT/TLS connection itself, using a certificate and
key stored on its own filesystem, and the dsPIC33 just talks to it over UART
with AT commands. The demo sends one random-number telemetry value to
IoTConnect every 10 seconds.

<!-- TODO: photo of the assembled setup (Curiosity board + RNWF11 mounted on mikroBUS A) -->
![Assembled hardware](media/hardware-overview.png)

## Table of Contents

1. [Introduction](#1-introduction)
2. [Prerequisites](#2-prerequisites)
3. [Hardware Setup](#3-hardware-setup)
4. [Provisioning](#4-provisioning)
5. [Build and Flash the Firmware](#5-build-and-flash-the-firmware)
6. [Running the Demo](#6-running-the-demo)
7. [How It Works](#7-how-it-works)
8. [Resources](#8-resources)

## 1. Introduction

This project builds on Microchip's own
[dsPIC33A Curiosity out-of-box demo](https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob)
for this exact board (the `dspic33ak512mps512_dim` variant) - if you haven't
run that demo yet, do that first to confirm your board and toolchain are
working before coming back here.

> [!NOTE]
> This is a from-scratch, trimmed-down MPLAB X project, not a fork of the OOB
> demo project - it reuses that demo's known-good clock configuration but
> drops the touch/CAN/RGB-LED sample peripherals, since this quickstart only
> needs UART.

## 2. Prerequisites

### Hardware

1. [dsPIC33 Curiosity Platform Development Board](https://www.microchip.com/en-us/development-tool/ev74h48a) with the dsPIC33AK512MPS512 General Purpose DIM installed
2. [RNWF11 UART to Cloud Add-on Board (EV12H55A)](https://www.microchip.com/en-us/development-tool/ev12h55a)
3. Two USB cables: one for the Curiosity board's onboard debugger/console, one for the RNWF11's own USB-C port (used only during provisioning)
4. A 2.4 GHz WiFi network and its credentials

### Software

1. [MPLAB X IDE](https://www.microchip.com/mplabx) 6.25 or later, with the XC-DSC compiler 3.21 or later and the `dsPIC33AK-MP_DFP` device pack (same toolchain as the OOB demo)
2. Python 3.9+ with `openssl` on your `PATH`, for the provisioning scripts in [`tools/`](tools/)
3. A serial terminal (PuTTY, Tera Term, MPLAB Data Visualizer's terminal, etc.) to watch the device's console output
4. An [IoTConnect](https://www.iotconnect.io/) account

## 3. Hardware Setup

The RNWF11 add-on board is mikroBUS-compliant, and the Curiosity board breaks
out two mikroBUS sockets. This firmware is wired for **mikroBUS A**
specifically (UART2 on RP72/RP73, mapped via Peripheral Pin Select in
`mcc_generated_files/system/src/pins.c`) - mount the RNWF11 there.

<!-- TODO: photo of the RNWF11 mounted on mikroBUS A -->
![RNWF11 mounted on mikroBUS A](media/rnwf11-mounted.png)

> [!IMPORTANT]
> The RNWF11 board has its own USB-C port and power-select jumper
> (`PC3V3` / `HOST3V3`), independent of the Curiosity board. You'll use both
> positions during provisioning - see [Section 4](#4-provisioning).

<!-- TODO: close-up photo of the RNWF11's power jumper, labeled with both positions -->
![RNWF11 power jumper](media/rnwf11-jumper.png)

## 4. Provisioning

Provisioning is a one-time (per device) two-step process. Both scripts live
in [`tools/`](tools/):

```bash
cd tools
pip install -r requirements.txt
```

### 4.1 Create the device in IoTConnect

<!-- TODO: screenshots of importing templates/dspic33-rnwf11-quickstart-template.json
     and creating a device with "Use my certificate", matching the style of
     the UI-ONBOARD.md walkthrough in iotc-python-lite-sdk-demos -->

1. In the IoTConnect console, go to **Device &rarr; Templates** and import [`templates/dspic33-rnwf11-quickstart-template.json`](templates/dspic33-rnwf11-quickstart-template.json).
2. Go to **Device &rarr; Devices** and create a new device using that template, with **"Use my certificate"** as the authentication type and a Unique ID (DUID) of your choosing - you'll use this DUID as `--duid` in both provisioning scripts below. You don't have the certificate yet; you'll paste it in after step 4.2 generates it.

### 4.2 Upload the cert/key to the RNWF11

Move the RNWF11's power jumper to **PC3V3** and plug its USB-C port directly
into your PC (not mounted on the Curiosity board yet).

```bash
python provision_rnwf11_cert.py --port COM6 --duid my-device-01 \
    --ca-cert-path amazon-root-ca-1.pem
```

Replace `COM6` with the RNWF11's serial port, and `--ca-cert-path` with a
root/CA certificate matching your IoTConnect account's backend (for AWS-backed
accounts, [Amazon Root CA 1](https://www.amazontrust.com/repository/AmazonRootCA1.pem)
is the common choice).

This generates a self-signed device certificate, prints it to the terminal,
and uploads the CA cert, device cert, and device key to the RNWF11's own
filesystem via `AT+FS`. Paste the printed certificate into the IoTConnect
console to finish creating the device from step 4.1.

Once it finishes, move the jumper back to **HOST3V3** and mount the RNWF11 on
the Curiosity board's mikroBUS A.

### 4.3 Push WiFi + IoTConnect config to the dsPIC33

Flash and power up the firmware first (see [Section 5](#5-build-and-flash-the-firmware))
- it will print `Not provisioned yet.` on its debug console and wait. Then:

```bash
python provision_device_config.py --port COM5 \
    --wifi-ssid "MyNetwork" --wifi-password "MyPassword" \
    --cpid <your CPID> --env <your Environment> --duid my-device-01 \
    --ca-name root-ca --cert-name device-cert --key-name device-key
```

Replace `COM5` with the Curiosity board's debug console port. Your CPID and
Environment are under **Settings &rarr; Key Value** in the IoTConnect console.
The `--ca-name`/`--cert-name`/`--key-name` values must match what
`provision_rnwf11_cert.py` printed at the end of step 4.2 (the defaults shown
here match its defaults).

This script resolves your device's actual MQTT broker host, username, and
telemetry topic via IoTConnect's discovery/identity API (using
[`iotconnect-sdk-lite`](https://pypi.org/project/iotconnect-sdk-lite/)'s
public `DeviceRestApi`, the same mechanism the other quickstarts in this
family use), then writes everything into the dsPIC33's on-chip flash over its
console UART.

> [!NOTE]
> The broker host/topic are resolved once, here, rather than re-resolved by
> the firmware on every boot - see [Section 7](#7-how-it-works) for why. If
> IoTConnect ever reassigns your device to a different broker, re-run this
> script.

## 5. Build and Flash the Firmware

1. Open [`firmware/dspic33ak512mps512_rnwf11_iotconnect.X`](firmware/dspic33ak512mps512_rnwf11_iotconnect.X) in MPLAB X.
2. Clean and build the project.
3. Program the board via the onboard PKOB4 debugger (Make and Program Device).
4. Open a serial terminal on the debug console port (UART1, 115200 8-N-1) to watch the boot log.

<!-- TODO: screenshot of a successful build in MPLAB X -->

## 6. Running the Demo

Once provisioned (Section 4) and flashed (Section 5), the device connects to
WiFi, then to IoTConnect over MQTT via the RNWF11, and publishes
`{"random": <0-100>}` every 10 seconds. Watch it arrive on the device's
**Live Data** tab in the IoTConnect console.

<!-- TODO: screenshot of the console debug log showing a successful connect + publish -->
<!-- TODO: screenshot of the IoTConnect Live Data tab showing incoming "random" values -->

## 7. How It Works

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

- **`firmware/dspic33ak512mps512_rnwf11_iotconnect.X/rnwf11/`** - Microchip's
  own RNWF11 AT-command service layer, ported from their
  [AVR128DB48/SAME54 reference firmware](https://github.com/MicrochipTech/AzureDemo_RNWF)
  (that code turned out to be almost entirely portable C, sitting behind a
  thin UART interface struct - only the hardware shim needed replacing).
- **`app/iotc_rnwf11.c`** - glue between that driver and
  [`Libraries/iotc-c-lib`](https://github.com/avnet-iotconnect/iotc-c-lib):
  builds telemetry JSON with iotc-c-lib's `iotcl_telemetry_*` functions (used
  standalone - iotc-c-lib doesn't bundle an MQTT/TLS client, by design), then
  publishes it via `AT+MQTTPUB` through the RNWF11 driver.
- **`app/device_config.c`** / **`bsp/nvm_flash.c`** - WiFi/IoTConnect config
  is stored in the dsPIC33's own on-chip flash (word-programmed per the data
  sheet's Flash Program Memory chapter), not on the RNWF11 - the RNWF11's
  filesystem only supports certificate/key files (`AT+FS` `FILETYPE`
  1=CERT/2=PRIKEY), not arbitrary config.
- **Why resolve the broker at provisioning time, not on every boot?** The
  RNWF11's own MQTT/TLS client handles the actual connection, but it has no
  HTTPS/discovery capability of its own - only WiFi, sockets, TLS, and MQTT.
  Doing IoTConnect's discovery+identity HTTPS round-trip on-device would mean
  building an HTTP client on top of the RNWF11's raw TCP+TLS socket AT
  commands. `tools/provision_device_config.py` does that resolution once, on
  the PC, and bakes the result into the device's flash instead.

## 8. Resources

- [dsPIC33A Curiosity OOB Demo](https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob) - the base this project's clock/pin configuration borrows from
- [RNWF11 UART to Cloud Add-on Board User's Guide](https://ww1.microchip.com/downloads/aemDocuments/documents/WSG/ProductDocuments/UserGuides/RNWF11-UART-to-Cloud-Add-on-Board-User-Guide-DS50003638.pdf)
- [RNWF11 Application Developer's Guide](https://onlinedocs.microchip.com/oxy/GUID-209426F5-2F78-4B3F-80A0-AD79A119381E) (AT command reference)
- [iotc-c-lib](https://github.com/avnet-iotconnect/iotc-c-lib) - IoTConnect's C SDK
- [iotc-python-lite-sdk-demos](https://github.com/avnet-iotconnect/iotc-python-lite-sdk-demos) - the same minimal-quickstart pattern for other boards
- [iotc-mchp-sama7d65-rnwf11](https://github.com/avnet-iotconnect/iotc-mchp-sama7d65-rnwf11) - an earlier, unconventional use of the RNWF11 (host-side TLS instead of on-module)

## Licensing

This repository is MIT licensed - see [LICENSE.md](LICENSE.md) - **except**
`firmware/dspic33ak512mps512_rnwf11_iotconnect.X/rnwf11/`, which is ported
from Microchip's reference firmware and remains under Microchip's SLA001
license (each file retains its original Microchip license header).
