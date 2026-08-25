# SPDX-License-Identifier: MIT
# Copyright (C) 2026 Avnet
#
# One-time provisioning step 1: generates a self-signed device certificate/key
# (same openssl recipe as the other iotc quickstarts in this family - see
# common/starter-demo/quickstart.sh in avnet-iotconnect/iotc-python-lite-sdk-demos)
# and uploads it, plus a root/CA cert you supply, directly to the RNWF11 add-on
# board's own filesystem via AT+FS (XMODEM), so the module can use them for its
# own MQTT/TLS session without ever handing the private key to the host MCU.
#
# Run this with the RNWF11 board's own USB-C cable plugged straight into your PC
# (its power-select jumper on PC3V3) - NOT mounted on the Curiosity board. See
# the README for the jumper/USB details.

import argparse
import subprocess
import sys
import time
from io import BytesIO
from pathlib import Path

import serial
from xmodem import XMODEM

FILETYPE_CERT = 1
FILETYPE_PRIKEY = 2
XMODEM_CRC16 = 2

EC_CURVE = "prime256v1"
CERT_DAYS = 36500  # 100 years, matches the other iotc quickstarts' self-signed certs


def gencert(duid: str, out_dir: Path):
    key_path = out_dir / f"{duid}-pkey.pem"
    cert_path = out_dir / f"{duid}-cert.pem"
    subj = f"/C=US/ST=IL/L=Chicago/O=IoTConnect/CN={duid}"

    subprocess.run(["openssl", "ecparam", "-name", EC_CURVE, "-genkey", "-noout", "-out", str(key_path)], check=True)
    subprocess.run(
        ["openssl", "req", "-new", "-days", str(CERT_DAYS), "-nodes", "-x509",
         "-subj", subj, "-key", str(key_path), "-out", str(cert_path)],
        check=True,
    )
    print(f"Generated {cert_path} and {key_path}")
    print("\nPaste the certificate below into the IoTConnect console when creating this")
    print(f'device (Unique ID "{duid}", "Use my certificate"):\n')
    print(cert_path.read_text())
    return cert_path, key_path


class RnwfAtLink:
    """Thin AT-command + XMODEM-upload helper for talking to the RNWF11 directly over its own USB-serial port."""

    def __init__(self, ser: serial.Serial):
        self.ser = ser

    def send_at(self, cmd: str, timeout=5) -> str:
        self.ser.timeout = timeout
        self.ser.write((cmd + "\r\n").encode("ascii"))
        response = b""
        deadline = time.time() + timeout
        while time.time() < deadline:
            chunk = self.ser.read(256)
            if not chunk:
                continue
            response += chunk
            if b"OK\r\n" in response or b"ERROR" in response:
                break
        text = response.decode("ascii", errors="replace")
        if "OK" not in text:
            raise RuntimeError(f"AT command failed: {cmd!r} -> {text!r}")
        return text

    def upload_file(self, filetype: int, filename: str, data: bytes):
        print(f"Uploading {filename} ({len(data)} bytes)...")
        self.ser.timeout = 5
        self.ser.write(f'AT+FS=1,{filetype},{XMODEM_CRC16},"{filename}",{len(data)}\r\n'.encode("ascii"))

        # Wait for the '#' raw-mode prompt before starting the XMODEM handshake.
        deadline = time.time() + 5
        buf = b""
        while time.time() < deadline:
            buf += self.ser.read(64)
            if b"#" in buf:
                break
        else:
            raise RuntimeError(f"RNWF11 never entered raw mode for {filename}: {buf!r}")

        def getc(size, timeout=1):
            self.ser.timeout = timeout
            return self.ser.read(size) or None

        def putc(chunk, timeout=1):
            self.ser.timeout = timeout
            return self.ser.write(chunk)

        modem = XMODEM(getc, putc)
        if not modem.send(BytesIO(data)):
            raise RuntimeError(f"XMODEM transfer failed for {filename}")

        # The module prints a final "OK" once the transfer completes.
        self.ser.timeout = 10
        tail = self.ser.read(64)
        if b"OK" not in tail:
            raise RuntimeError(f"RNWF11 did not confirm {filename}: {tail!r}")
        print(f"{filename} uploaded OK")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="RNWF11's own USB-serial port (e.g. COM6 or /dev/ttyACM0) - NOT the Curiosity board's debug console")
    parser.add_argument("--baud", type=int, default=230400, help="RNWF11 AT-command baud rate (default: 230400)")
    parser.add_argument("--duid", required=True, help="This device's Unique ID - used as the cert's Common Name and output filename")
    parser.add_argument("--ca-cert-path", required=True, help="Path to a root/CA cert PEM file to also upload, for validating the broker's server certificate (e.g. Amazon Root CA 1 for an AWS-backed IoTConnect account - see the README)")
    parser.add_argument("--ca-name", default="root-ca", help="Filename to store the CA cert as on the RNWF11 (default: root-ca)")
    parser.add_argument("--cert-name", default="device-cert", help="Filename to store the device cert as on the RNWF11 (default: device-cert)")
    parser.add_argument("--key-name", default="device-key", help="Filename to store the device private key as on the RNWF11 (default: device-key)")
    parser.add_argument("--out-dir", default=".", help="Where to write the generated cert/key PEM files on this PC (default: current directory)")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    cert_path, key_path = gencert(args.duid, out_dir)

    ca_path = Path(args.ca_cert_path)
    if not ca_path.is_file():
        print(f"CA cert not found: {ca_path}")
        sys.exit(1)

    print(f"\nConnecting to RNWF11 at {args.port} ({args.baud} baud)...")
    with serial.Serial(args.port, args.baud, timeout=5) as ser:
        link = RnwfAtLink(ser)
        link.send_at("AT")  # sanity check the module responds before doing anything else

        link.upload_file(FILETYPE_CERT, args.ca_name, ca_path.read_bytes())
        link.upload_file(FILETYPE_CERT, args.cert_name, cert_path.read_bytes())
        link.upload_file(FILETYPE_PRIKEY, args.key_name, key_path.read_bytes())

    print("\nDone. Move the RNWF11's power jumper back to HOST3V3, mount it on the")
    print("Curiosity board's mikroBUS A, and run provision_device_config.py next.")
    print(f"Pass these filenames to it: --ca-name {args.ca_name} --cert-name {args.cert_name} --key-name {args.key_name}")


if __name__ == "__main__":
    main()
