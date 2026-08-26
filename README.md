# dsPIC33AK512MPS512 + RNWF11 /IOTCONNECT Quickstart

A baremetal C quickstart connecting the Microchip **dsPIC33AK512MPS512** (on the
**dsPIC33 Curiosity Platform Development Board**) to
[Avnet /IOTCONNECT](https://www.iotconnect.io/) using /IOTCONNECT's
[C SDK](https://github.com/avnet-iotconnect/iotc-c-lib), over a Microchip
**RNWF11 UART to Cloud Add-on Board**. No RTOS, no MQTT/TLS stack on the MCU -
the RNWF11 owns the WiFi/MQTT/TLS connection itself, using a certificate and
key stored on its own filesystem, and the dsPIC33 just talks to it over UART
with AT commands. The demo sends one random-number telemetry value to
/IOTCONNECT every 10 seconds.

<!-- TODO: photo of the assembled setup (Curiosity board + RNWF11 mounted on mikroBUS A) -->
![Assembled hardware](media/hardware-overview.png)

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Download the Quickstart Files](#2-download-the-quickstart-files)
3. [Import the Device Template](#3-import-the-device-template)
4. [Generate and Upload the Device Certificate](#4-generate-and-upload-the-device-certificate)
5. [Create the Device in /IOTCONNECT](#5-create-the-device-in-iotconnect)
6. [Mount the RNWF11 on the Curiosity Board](#6-mount-the-rnwf11-on-the-curiosity-board)
7. [Flash the Firmware](#7-flash-the-firmware)
8. [Provision WiFi and /IOTCONNECT Config](#8-provision-wifi-and-iotconnect-config)
9. [Running the Demo](#9-running-the-demo)
10. [Resources](#10-resources)

The steps below are in the order you actually need to do them: the device
certificate has to exist before you can create the device in /IOTCONNECT, the
RNWF11 has to be provisioned with that certificate before it's mounted on
the Curiosity board, and the firmware has to already be flashed and running
before the final config-provisioning step can talk to it.

## 1. Prerequisites

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
5. An [/IOTCONNECT](https://www.iotconnect.io/) account

## 2. Download the Quickstart Files

You don't need to clone this repository - download just the files this
quickstart uses. Pick the block matching your provisioning script choice
(Python or PowerShell) from [Prerequisites](#1-prerequisites):

### Linux/macOS (bash), Python scripts:

```bash
mkdir dspic33-rnwf11-quickstart && cd dspic33-rnwf11-quickstart
RAW=https://raw.githubusercontent.com/avnet-iotconnect/iotc-mchp-dspic33/main
curl -fsSLO "$RAW/templates/dspic33-rnwf11-quickstart-template.json"
mkdir bin && curl -fsSL -o bin/dspic33ak512mps512_rnwf11_iotconnect.hex "$RAW/bin/dspic33ak512mps512_rnwf11_iotconnect.hex"
mkdir tools && cd tools
curl -fsSLO "$RAW/tools/provision_rnwf11_cert.py"
curl -fsSLO "$RAW/tools/provision_device_config.py"
curl -fsSLO "$RAW/tools/requirements.txt"
pip install -r requirements.txt 2>/dev/null || pip install --break-system-packages -r requirements.txt
cd ..
```

> [!NOTE]
> On newer Debian/Ubuntu systems, plain `pip install` may refuse with an
> "externally-managed-environment" error (PEP 668) - the `--break-system-packages`
> fallback above handles that. If you'd rather not touch the system Python at
> all, use a virtual environment instead: `python3 -m venv venv && source venv/bin/activate && pip install -r requirements.txt`.

### Windows (PowerShell), PowerShell scripts:

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

The rest of this README refers to files by these same relative paths
(`templates/...`, `bin/...`, `tools/...`), whether you downloaded them
individually or cloned the repo.

## 3. Import the Device Template

<!-- TODO: screenshot of importing templates/dspic33-rnwf11-quickstart-template.json,
     matching the style of the UI-ONBOARD.md walkthrough in iotc-python-lite-sdk-demos -->

In the /IOTCONNECT console, go to **Device &rarr; Templates** and import
[`templates/dspic33-rnwf11-quickstart-template.json`](templates/dspic33-rnwf11-quickstart-template.json).
You'll select this template in a couple of sections from now, when creating
the device.

## 4. Generate and Upload the Device Certificate

The RNWF11 board has its own USB-C port and power-select jumper
(`PC3V3` / `HOST3V3`), independent of the Curiosity board - this step uses it
standalone, **not** mounted on the Curiosity board yet.

<!-- TODO: close-up photo of the RNWF11's power jumper, labeled with both positions -->
![RNWF11 power jumper](media/rnwf11-jumper.png)

Move the jumper to **PC3V3** and plug the RNWF11's USB-C port directly into
your PC.

**Before running the command below**, find the serial port name it just
enumerated as:
- **Linux**: run `ls /dev/serial/by-id/` (or `dmesg | tail` right after
  plugging it in) - look for the RNWF11's MCP2200 USB-to-UART bridge, e.g.
  `/dev/ttyACM0`.
- **macOS**: run `ls /dev/cu.*` - look for something like `/dev/cu.usbmodemXXXX`.
- **Windows**: open Device Manager &rarr; **Ports (COM & LPT)** - look for
  "MCP2200 USB Serial Port Emulator" and note its `COMx` number.

You'll also need a root/CA certificate matching your IoTConnect account's
backend (for AWS-backed accounts,
[Amazon Root CA 1](https://www.amazontrust.com/repository/AmazonRootCA1.pem)
is the common choice) - download it and note its path.

Then, in the command below, replace `MYDEVICENAME` with the port you just
found, and `amazon-root-ca-1.pem` with your CA cert's path. `--duid` is a
Unique ID of your own choosing for this device - you'll reuse it later, both
when creating the device in IoTConnect and when provisioning WiFi/IoTConnect
config, so pick something memorable (`my-device-01` here is just an example).
From the `dspic33-rnwf11-quickstart` directory you downloaded the files into:

**Linux/macOS:**
```bash
cd tools
python3 provision_rnwf11_cert.py --port MYDEVICENAME --duid my-device-01 \
    --ca-cert-path amazon-root-ca-1.pem
cd ..
```

**Windows (PowerShell):**
```powershell
Set-Location tools
.\provision_rnwf11_cert.ps1 -Port MYDEVICENAME -Duid my-device-01 `
    -CaCertPath amazon-root-ca-1.pem
Set-Location ..
```

This generates a self-signed device certificate, prints it to the terminal,
and uploads the CA cert, device cert, and device key to the RNWF11's own
filesystem via `AT+FS`. Keep the terminal output around - you'll paste the
printed certificate into the IoTConnect console in the next step.

## 5. Create the Device in /IOTCONNECT

<!-- TODO: screenshot of creating a device with "Use my certificate", pasting
     in the certificate printed by provision_rnwf11_cert.py -->

Go to **Device &rarr; Devices** and create a new device using the template
you imported earlier, with **"Use my certificate"** as the authentication
type, the same Unique ID (DUID) you passed to `provision_rnwf11_cert.py`
earlier, and the certificate that script printed.

## 6. Mount the RNWF11 on the Curiosity Board

Move the RNWF11's power jumper back to **HOST3V3**, then mount it on the
Curiosity board's **mikroBUS A** socket specifically - this firmware is wired
for that socket (UART2 on RP72/RP73, mapped via Peripheral Pin Select in
`mcc_generated_files/system/src/pins.c`).

<!-- TODO: photo of the RNWF11 mounted on mikroBUS A -->
![RNWF11 mounted on mikroBUS A](media/rnwf11-mounted.png)

## 7. Flash the Firmware

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

## 8. Provision WiFi and /IOTCONNECT Config

With the firmware running and waiting, push WiFi and IoTConnect config to it
over the same debug console UART.

**Before running the command below**, find the Curiosity board's debug
console port (its onboard PKOB4/MCP2221A USB-to-UART bridge - a different
port than the RNWF11's, from the earlier step):
- **Linux**: run `ls /dev/serial/by-id/` (or `dmesg | tail` right after
  plugging the board in) - look for something like `/dev/ttyACM1`.
- **macOS**: run `ls /dev/cu.*` - look for something like `/dev/cu.usbmodemXXXX`.
- **Windows**: open Device Manager &rarr; **Ports (COM & LPT)** and note its
  `COMx` number.

Then, in the command below, replace `MYDEVICENAME` with that port. Your CPID
and Environment are under **Settings &rarr; Key Value** in the IoTConnect
console. `--duid`/`-Duid` must match the DUID you used earlier when
generating the certificate and creating the device. The cert/key name
options must match what `provision_rnwf11_cert.py`/`.ps1` printed when you
ran it earlier (the defaults shown here match its defaults). From the
`dspic33-rnwf11-quickstart` directory you downloaded the files into:

**Linux/macOS:**
```bash
cd tools
python3 provision_device_config.py --port MYDEVICENAME \
    --wifi-ssid "MyNetwork" --wifi-password "MyPassword" \
    --cpid <your CPID> --env <your Environment> --duid my-device-01 \
    --ca-name root-ca --cert-name device-cert --key-name device-key
cd ..
```

**Windows (PowerShell):**
```powershell
Set-Location tools
.\provision_device_config.ps1 -Port MYDEVICENAME `
    -WifiSsid "MyNetwork" -WifiPassword "MyPassword" `
    -Cpid <your CPID> -Env <your Environment> -Duid my-device-01 `
    -CaName root-ca -CertName device-cert -KeyName device-key
Set-Location ..
```

This script resolves your device's actual MQTT broker host, username, and
telemetry topic via /IOTCONNECT's discovery/identity API (using
[`iotconnect-sdk-lite`](https://pypi.org/project/iotconnect-sdk-lite/)'s
public `DeviceRestApi`, the same mechanism the other quickstarts in this
family use - this only succeeds once the device already exists in
/IOTCONNECT, which is why the device has to be created before this step),
then writes everything into the dsPIC33's on-chip flash over its console
UART.

> [!NOTE]
> The broker host/topic are resolved once, here, rather than re-resolved by
> the firmware on every boot - see [developer.md](developer.md#2-project-architecture)
> for why. If /IOTCONNECT ever reassigns your device to a different broker,
> re-run this script.

## 9. Running the Demo

Once provisioning completes, the device connects to WiFi, then to /IOTCONNECT
over MQTT via the RNWF11, and publishes `{"random": <0-100>}` every 10 seconds.
Watch it arrive on the device's **Live Data** tab in the /IOTCONNECT console.

<!-- TODO: screenshot of the console debug log showing a successful connect + publish -->
<!-- TODO: screenshot of the /IOTCONNECT Live Data tab showing incoming "random" values -->

## 10. Resources

- [developer.md](developer.md) - building from source, project architecture, and how to modify the firmware
- [dsPIC33A Curiosity OOB Demo](https://github.com/microchip-pic-avr-examples/dspic33a-curiosity-oob) - the base this project's clock/pin configuration borrows from
- [RNWF11 UART to Cloud Add-on Board User's Guide](https://ww1.microchip.com/downloads/aemDocuments/documents/WSG/ProductDocuments/UserGuides/RNWF11-UART-to-Cloud-Add-on-Board-User-Guide-DS50003638.pdf)
- [RNWF11 Application Developer's Guide](https://onlinedocs.microchip.com/oxy/GUID-209426F5-2F78-4B3F-80A0-AD79A119381E) (AT command reference)
- [iotc-c-lib](https://github.com/avnet-iotconnect/iotc-c-lib) - /IOTCONNECT's C SDK
- [iotc-python-lite-sdk-demos](https://github.com/avnet-iotconnect/iotc-python-lite-sdk-demos) - the same minimal-quickstart pattern for other boards
- [iotc-mchp-sama7d65-rnwf11](https://github.com/avnet-iotconnect/iotc-mchp-sama7d65-rnwf11) - an earlier, unconventional use of the RNWF11 (host-side TLS instead of on-module)

## Licensing

This repository is MIT licensed - see [LICENSE.md](LICENSE.md) - **except**
`firmware/dspic33ak512mps512_rnwf11_iotconnect.X/rnwf11/`, which is ported
from Microchip's reference firmware and remains under Microchip's SLA001
license (each file retains its original Microchip license header).
