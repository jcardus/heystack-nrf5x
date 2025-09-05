#!/usr/bin/env bash
set -euo pipefail

# --- Config ---
GEN_KEYS="../../tools/generate_keys.py"
KEYS_DIR="./output"
OBJCOPY="../../nrf-sdk/gcc-arm-none-eabi-6-2017-q2-update/bin/arm-none-eabi-objcopy"
ELF_FILE="_build/nrf52832_xxaa-dcdc_s132_patched.elf"

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
TARGET="stflash-nrf52832_xxaa-dcdc-patched"
echo "📦 Building + flashing firmware for $DEVICE_ID..."

make "$TARGET" ADV_KEYS_FILE="$KEYFILE"

# --- Convert ELF → HEX ---
HEX_FILE="$DEVICE_ID.hex"
echo "📄 Converting ELF to HEX..."
"$OBJCOPY" -O ihex "$ELF_FILE" "$HEX_FILE"

echo "✅ Done! Firmware ready: $HEX_FILE"
