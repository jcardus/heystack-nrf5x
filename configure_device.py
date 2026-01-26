#!/usr/bin/env python3
"""
HeyStack BLE Configuration Tool

Connects to a HeyStack device during its 30-second configuration window
and writes public keys over BLE.

Usage:
    python configure_device.py keys.bin
    python configure_device.py --scan
    python configure_device.py --address XX:XX:XX:XX:XX:XX keys.bin

Requirements:
    pip install bleak
"""

import argparse
import asyncio
import struct
import sys
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
    from bleak.exc import BleakError
except ImportError:
    print("Error: bleak library not installed.")
    print("Install with: pip install bleak")
    sys.exit(1)

# Service and characteristic UUIDs
CONFIG_SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"
KEY_WRITE_CHAR_UUID = "0000ff01-0000-1000-8000-00805f9b34fb"
KEY_COUNT_CHAR_UUID = "0000ff02-0000-1000-8000-00805f9b34fb"

DEVICE_NAME = "HeyStack-Config"
KEY_LENGTH = 28


async def scan_for_devices(timeout: float = 10) -> list:
    """Scan for HeyStack devices in configuration mode."""
    print(f"Scanning for '{DEVICE_NAME}' devices ({timeout}s)...")

    devices = []
    detected = []

    def detection_callback(device, advertisement_data):
        if device.name and DEVICE_NAME in device.name:
            devices.append((device, advertisement_data))
        else:
            if device.address not in [d.address for d, _ in detected]:
                detected.append((device, advertisement_data))
                print(device)

    scanner = BleakScanner(detection_callback=detection_callback)
    await scanner.start()
    await asyncio.sleep(1000)
    await scanner.stop()

    return devices


async def connect_and_configure(address: str, keys: list[bytes]) -> bool:
    """Connect to device and write keys."""
    print(f"Connecting to {address}...")

    try:
        async with BleakClient(address, timeout=20.0) as client:
            if not client.is_connected:
                print("Failed to connect")
                return False

            print("Connected!")

            # List services for debugging
            services = client.services
            config_service = None
            for service in services:
                if service.uuid.lower() == CONFIG_SERVICE_UUID:
                    config_service = service
                    break

            if not config_service:
                print(f"Error: Configuration service {CONFIG_SERVICE_UUID} not found")
                print("Available services:")
                for service in services:
                    print(f"  - {service.uuid}")
                return False

            print(f"Found configuration service")

            # Read initial key count
            try:
                key_count_data = await client.read_gatt_char(KEY_COUNT_CHAR_UUID)
                initial_count = struct.unpack("<H", key_count_data)[0]
                print(f"Current key count: {initial_count}")
            except Exception as e:
                print(f"Warning: Could not read key count: {e}")
                initial_count = 0

            # Write each key
            success_count = 0
            for i, key in enumerate(keys):
                if len(key) != KEY_LENGTH:
                    print(f"Warning: Key {i} has invalid length {len(key)}, skipping")
                    continue

                try:
                    await client.write_gatt_char(KEY_WRITE_CHAR_UUID, key, response=True)
                    success_count += 1
                    print(f"Wrote key {i + 1}/{len(keys)}")
                except Exception as e:
                    print(f"Error writing key {i}: {e}")

            # Read final key count
            try:
                key_count_data = await client.read_gatt_char(KEY_COUNT_CHAR_UUID)
                final_count = struct.unpack("<H", key_count_data)[0]
                print(f"Final key count: {final_count}")
            except Exception as e:
                print(f"Warning: Could not read final key count: {e}")

            print(f"Successfully wrote {success_count}/{len(keys)} keys")
            return success_count > 0

    except BleakError as e:
        print(f"BLE error: {e}")
        return False
    except asyncio.TimeoutError:
        print("Connection timed out")
        return False


def load_keys_from_file(filepath: Path) -> list[bytes]:
    """Load keys from a binary file (28 bytes per key)."""
    if not filepath.exists():
        print(f"Error: File not found: {filepath}")
        sys.exit(1)

    data = filepath.read_bytes()

    if len(data) % KEY_LENGTH != 0:
        print(f"Warning: File size ({len(data)}) is not a multiple of {KEY_LENGTH}")

    keys = []
    for i in range(0, len(data), KEY_LENGTH):
        key = data[i:i + KEY_LENGTH]
        if len(key) == KEY_LENGTH:
            # Skip placeholder keys
            if key != b"OFFLINEFINDINGPUBLICKEYHERE!" and key != b"ENDOFKEYSENDOFKEYSENDOFKEYS!":
                keys.append(key)

    return keys


def load_keys_from_text_file(filepath: Path) -> list[bytes]:
    """Load keys from a text file (one hex-encoded key per line)."""
    if not filepath.exists():
        print(f"Error: File not found: {filepath}")
        sys.exit(1)

    keys = []
    with open(filepath, 'r') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            try:
                key = bytes.fromhex(line)
                if len(key) == KEY_LENGTH:
                    keys.append(key)
                else:
                    print(f"Warning: Line {line_num} has invalid length {len(key)}, skipping")
            except ValueError as e:
                print(f"Warning: Line {line_num} is not valid hex: {e}")

    return keys


async def main():
    parser = argparse.ArgumentParser(
        description="Configure HeyStack device with public keys over BLE"
    )
    parser.add_argument(
        "keyfile",
        nargs="?",
        type=Path,
        help="Path to key file (.bin for binary, .txt for hex-encoded)"
    )
    parser.add_argument(
        "--scan",
        action="store_true",
        help="Scan for devices and exit"
    )
    parser.add_argument(
        "--address", "-a",
        type=str,
        help="Device address (skip scanning)"
    )
    parser.add_argument(
        "--timeout", "-t",
        type=float,
        default=100.0,
        help="Scan timeout in seconds (default: 10)"
    )

    args = parser.parse_args()

    # Scan mode
    if args.scan:
        devices = await scan_for_devices(args.timeout)
        if devices:
            print(f"\nFound {len(devices)} device(s):")
            for device, adv_data in devices:
                print(f"  {device.name}: {device.address}")
                if adv_data.rssi:
                    print(f"    RSSI: {adv_data.rssi} dBm")
        else:
            print("No HeyStack devices found in configuration mode")
        return

    # Configuration mode requires a key file
    if not args.keyfile:
        parser.error("keyfile is required (unless using --scan)")

    # Load keys
    if args.keyfile.suffix.lower() == ".txt":
        keys = load_keys_from_text_file(args.keyfile)
    else:
        keys = load_keys_from_file(args.keyfile)

    if not keys:
        print("No valid keys found in file")
        sys.exit(1)

    print(f"Loaded {len(keys)} keys from {args.keyfile}")

    # Get device address
    address = args.address
    if not address:
        devices = await scan_for_devices(args.timeout)
        if not devices:
            print("No HeyStack devices found. Make sure the device is powered on")
            print("and in configuration mode (first 30 seconds after boot).")
            sys.exit(1)

        if len(devices) > 1:
            print(f"\nFound {len(devices)} devices:")
            for i, (device, _) in enumerate(devices):
                print(f"  [{i}] {device.name}: {device.address}")

            try:
                choice = int(input("Select device number: "))
                address = devices[choice][0].address
            except (ValueError, IndexError):
                print("Invalid selection")
                sys.exit(1)
        else:
            address = devices[0][0].address
            print(f"Found device: {devices[0][0].name} ({address})")

    # Connect and configure
    success = await connect_and_configure(address, keys)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    asyncio.run(main())
