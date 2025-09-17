#!/usr/bin/env bash
set -euo pipefail

source ../../venv/bin/activate

# --- Config ---
GEN_KEYS="../../tools/generate_keys.py"
KEYS_DIR="./output"
OBJCOPY="../../nrf-sdk/gcc-arm-none-eabi-6-2017-q2-update/bin/arm-none-eabi-objcopy"
ELF_FILE="_build/nrf51822_xxac-dcdc_s130_patched.elf"

# --- Generate keys ---
echo "🔑 Generating keys..."
python "$GEN_KEYS"

# --- Get latest device ID ---
KEYFILE=$(ls -1t "$KEYS_DIR"/*_keyfile | head -1)
if [ -z "$KEYFILE" ]; then
  echo "❌ No keyfile generated"
  exit 1
fi
DEVICE_ID=$(basename "$KEYFILE" | cut -d_ -f1)
echo "📡 Device ID: $DEVICE_ID"

make clean
# --- Build ---
TARGET="stflash-nrf51822_xxac-dcdc-patched"
echo "📦 Building + flashing firmware for $DEVICE_ID..."

make "$TARGET" ADV_KEYS_FILE="$KEYFILE"

# --- Convert ELF → HEX ---
HEX_FILE="./output/${DEVICE_ID}_nrf51822.hex"
echo "📄 Converting ELF to HEX..."
"$OBJCOPY" -O ihex "$ELF_FILE" "$HEX_FILE"

echo "✅ Done! Firmware ready: $HEX_FILE"

cp _build/nrf51822_xxac-dcdc_s130_patched.bin output/nrf51822_"$DEVICE_ID".bin
echo "✅ Binary ready: output/nrf51822_$DEVICE_ID.bin"

echo $(head -1 "./output/${DEVICE_ID}.keys") > "./output/${DEVICE_ID}.txt"

aws s3 cp "./output/${DEVICE_ID}.txt" s3://air-tags
aws s3 cp "$HEX_FILE" s3://air-tags

