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
3. [Download the Quickstart Files](#3-download-the-quickstart-files)
4. [Step 1: Import the Device Template](#4-step-1-import-the-device-template)
5. [Step 2: Generate and Upload the Device Certificate](#5-step-2-generate-and-upload-the-device-certificate)
6. [Step 3: Create the Device in IoTConnect](#6-step-3-create-the-device-in-iotconnect)
7. [Step 4: Mount the RNWF11 on the Curiosity Board](#7-step-4-mount-the-rnwf11-on-the-curiosity-board)
8. [Step 5: Flash the Firmware](#8-step-5-flash-the-firmware)
9. [Step 6: Provision WiFi and IoTConnect Config](#9-step-6-provision-wifi-and-iotconnect-config)
10. [Running the Demo](#10-running-the-demo)
11. [Resources](#11-resources)

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

The steps below are in the order you actually need to do them: the device
certificate has to exist before you can create the device in IoTConnect, the
RNWF11 has to be provisioned with that certificate *before* it's mounted on
the Curiosity board, and the firmware has to already be flashed and running
before the final config-provisioning step can talk to it.

## 2. Prerequisites

### Hardware

1. [dsPIC33 Curiosity Platform Development Board](https://www.microchip.com/en-us/development-tool/ev74h48a) with the dsPIC33AK512MPS512 General Purpose DIM installed
2. [RNWF11 UART to Cloud Add-on Board (EV12H55A)](https://www.microchip.com/en-us/development-tool/ev12h55a)
3. Two USB cables: one for the Curiosity board's onboard debugger/console, one for the RNWF11's own USB-C port (used only during provisioning)
4. A 2.4 GHz WiFi network and its credentials

### Software

1. [MPLAB IPE](https://www.microchip.com/mplab/mplab-integrated-programming-environment) (or MPLAB X IDE, which includes it) to flash the pre-built firmware image in [`bin/`](bin/) via the onboard PKOB4 debugger. You only need the full MPLAB X IDE if you're building from source - see [developer.md](developer.md).
2. `openssl` on your `PATH` (already present on most Linux/macOS systems; on Windows it's included with [Git for Windows](https://git-scm.com/downloads/win), among other sources)
3. Either Python 3.9+ **or** PowerShell 5.1+ (Windows ships this by default; PowerShell 7+ also works on Linux/macOS) to run the provisioning scripts - pick whichever you're more comfortable with, both do the same thing
4. A serial terminal (PuTTY, Tera Term, MPLAB Data Visualizer's terminal, etc.) to watch the device's console output
5. An [IoTConnect](https://www.iotconnect.io/) account

## 3. Download the Quickstart Files

You don't need to clone this repository - download just the files this
quickstart uses. Pick the block matching your provisioning script choice
(Python or PowerShell) from [Prerequisites](#2-prerequisites):

<details>
<summary><b>Linux/macOS (bash), Python scripts</b></summary>

```bash
mkdir dspic33-rnwf11-quickstart && cd dspic33-rnwf11-quickstart
RAW=https://raw.githubusercontent.com/avnet-iotconnect/iotc-mchp-dspic33/main
curl -fsSLO "$RAW/templates/dspic33-rnwf11-quickstart-template.json"
mkdir bin && curl -fsSL -o bin/dspic33ak512mps512_rnwf11_iotconnect.hex "$RAW/bin/dspic33ak512mps512_rnwf11_iotconnect.hex"
mkdir tools && cd tools
curl -fsSLO "$RAW/tools/provision_rnwf11_cert.py"
curl -fsSLO "$RAW/tools/provision_device_config.py"
curl -fsSLO "$RAW/tools/requirements.txt"
pip install -r requirements.txt
cd ..
```

</details>

<details>
<summary><b>Windows (PowerShell), PowerShell scripts</b></summary>

```powershell
New-Item -ItemType Directory dspic33-rnwf11-quickstart | Out-Null
Set-Location dspic33-rnwf11-quickstart
$Raw = "https://raw.githubusercontent.com/avnet-iotconnect/iotc-mchp-dspic33/main"
Invoke-WebRequest "$Raw/templates/dspic33-rnwf11-quickstart-template.json" -OutFile dspic33-rnwf11-quickstart-template.json
New-Item -ItemType Directory bin | Out-Null
Invoke-WebRequest "$Raw/bin/dspic33ak512mps512_rnwf11_iotconnect.hex" -OutFile bin/dspic33ak512mps512_rnwf11_iotconnect.hex
New-Item -ItemType Directory tools | Out-Null
Set-Location tools
Invoke-WebRequest "$Raw/tools/provision_rnwf11_cert.ps1" -OutFile provision_rnwf11_cert.ps1
Invoke-WebRequest "$Raw/tools/provision_device_config.ps1" -OutFile provision_device_config.ps1
Set-Location ..
```

</details>

The rest of this README refers to files by these same relative paths
(`templates/...`, `bin/...`, `tools/...`), whether you downloaded them
individually or cloned the repo.

## 4. Step 1: Import the Device Template

<!-- TODO: screenshot of importing templates/dspic33-rnwf11-quickstart-template.json,
     matching the style of the UI-ONBOARD.md walkthrough in iotc-python-lite-sdk-demos -->

In the IoTConnect console, go to **Device &rarr; Templates** and import
[`templates/dspic33-rnwf11-quickstart-template.json`](templates/dspic33-rnwf11-quickstart-template.json).
You'll select this template when creating the device in
[Step 3](#6-step-3-create-the-device-in-iotconnect).

## 5. Step 2: Generate and Upload the Device Certificate

The RNWF11 board has its own USB-C port and power-select jumper
(`PC3V3` / `HOST3V3`), independent of the Curiosity board - this step uses it
standalone, **not** mounted on the Curiosity board yet.

<!-- TODO: close-up photo of the RNWF11's power jumper, labeled with both positions -->
![RNWF11 power jumper](media/rnwf11-jumper.png)

Move the jumper to **PC3V3** and plug the RNWF11's USB-C port directly into
your PC. Then, from `tools/`:

**Linux/macOS:**
```bash
python provision_rnwf11_cert.py --port /dev/ttyACM0 --duid my-device-01 \
    --ca-cert-path amazon-root-ca-1.pem
```

**Windows (PowerShell):**
```powershell
.\provision_rnwf11_cert.ps1 -Port COM6 -Duid my-device-01 `
    -CaCertPath amazon-root-ca-1.pem
```

Replace `/dev/ttyACM0`/`COM6` with the RNWF11's serial port, and `--ca-cert-path`/`-CaCertPath` with a
root/CA certificate matching your IoTConnect account's backend (for AWS-backed
accounts, [Amazon Root CA 1](https://www.amazontrust.com/repository/AmazonRootCA1.pem)
is the common choice). `--duid` is a Unique ID of your choosing for this
device - you'll reuse it in Steps 3 and 6.

This generates a self-signed device certificate, prints it to the terminal,
and uploads the CA cert, device cert, and device key to the RNWF11's own
filesystem via `AT+FS`. Keep the terminal output around - you'll paste the
printed certificate into the IoTConnect console in the next step.

## 6. Step 3: Create the Device in IoTConnect

<!-- TODO: screenshot of creating a device with "Use my certificate", pasting
     in the certificate printed by provision_rnwf11_cert.py -->

Go to **Device &rarr; Devices** and create a new device using the template
from Step 1, with **"Use my certificate"** as the authentication type, the
same Unique ID (DUID) you passed to `provision_rnwf11_cert.py` in Step 2, and
the certificate that script printed.

## 7. Step 4: Mount the RNWF11 on the Curiosity Board

Move the RNWF11's power jumper back to **HOST3V3**, then mount it on the
Curiosity board's **mikroBUS A** socket specifically - this firmware is wired
for that socket (UART2 on RP72/RP73, mapped via Peripheral Pin Select in
`mcc_generated_files/system/src/pins.c`).

<!-- TODO: photo of the RNWF11 mounted on mikroBUS A -->
![RNWF11 mounted on mikroBUS A](media/rnwf11-mounted.png)

## 8. Step 5: Flash the Firmware

This quickstart ships a pre-built firmware image at
[`bin/dspic33ak512mps512_rnwf11_iotconnect.hex`](bin/) - you don't need
MPLAB X or to build anything to run the demo.

1. Open MPLAB IPE, select the dsPIC33AK512MPS512 device and the onboard PKOB4 tool.
2. Browse to `bin/dspic33ak512mps512_rnwf11_iotconnect.hex` and click **Program**.
3. Open a serial terminal on the debug console port (UART1, 115200 8-N-1) to watch the boot log.

<!-- TODO: screenshot of MPLAB IPE programming the board -->

The firmware will print `Not provisioned yet.` and wait - that's expected
until you complete the next step.

> [!TIP]
> Modifying the firmware, or just want to build it yourself from source?
> See [developer.md](developer.md).

## 9. Step 6: Provision WiFi and IoTConnect Config

With the firmware running and waiting (Step 5), push WiFi and IoTConnect
config to it over the same debug console UART:

**Linux/macOS:**
```bash
python provision_device_config.py --port /dev/ttyACM1 \
    --wifi-ssid "MyNetwork" --wifi-password "MyPassword" \
    --cpid <your CPID> --env <your Environment> --duid my-device-01 \
    --ca-name root-ca --cert-name device-cert --key-name device-key
```

**Windows (PowerShell):**
```powershell
.\provision_device_config.ps1 -Port COM5 `
    -WifiSsid "MyNetwork" -WifiPassword "MyPassword" `
    -Cpid <your CPID> -Env <your Environment> -Duid my-device-01 `
    -CaName root-ca -CertName device-cert -KeyName device-key
```

Replace `/dev/ttyACM1`/`COM5` with the Curiosity board's debug console port. Your CPID and
Environment are under **Settings &rarr; Key Value** in the IoTConnect console.
`--duid`/`-Duid` must match Steps 2 and 3. The cert/key name options must
match what `provision_rnwf11_cert.py`/`.ps1` printed at the end of Step 2
(the defaults shown here match its defaults).

This script resolves your device's actual MQTT broker host, username, and
telemetry topic via IoTConnect's discovery/identity API (using
[`iotconnect-sdk-lite`](https://pypi.org/project/iotconnect-sdk-lite/)'s
public `DeviceRestApi`, the same mechanism the other quickstarts in this
family use - this only succeeds once the device exists in IoTConnect, which
is why Step 3 has to come first), then writes everything into the dsPIC33's
on-chip flash over its console UART.

> [!NOTE]
> The broker host/topic are resolved once, here, rather than re-resolved by
> the firmware on every boot - see [developer.md](developer.md#2-project-architecture)
> for why. If IoTConnect ever reassigns your device to a different broker,
> re-run this script.

## 10. Running the Demo

Once Step 6 completes, the device connects to WiFi, then to IoTConnect over
MQTT via the RNWF11, and publishes `{"random": <0-100>}` every 10 seconds.
Watch it arrive on the device's **Live Data** tab in the IoTConnect console.

<!-- TODO: screenshot of the console debug log showing a successful connect + publish -->
<!-- TODO: screenshot of the IoTConnect Live Data tab showing incoming "random" values -->

## 11. Resources

- [developer.md](developer.md) - building from source, project architecture, and how to modify the firmware
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
