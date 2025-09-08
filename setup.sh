#!/usr/bin/env bash
set -e

mkdir nrf-sdk || echo ">>> nrf-sdk directory already exists, skipping creation."

# Detect platform and download appropriate toolchain
if [[ "$OSTYPE" == "darwin"* ]]; then
    TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-mac.tar.bz2"
    TOOLCHAIN_FILE="gcc-arm-none-eabi-6-2017-q2-update-mac.tar.bz2"
else
    TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2"
    TOOLCHAIN_FILE="gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2"
fi

echo ">>> Downloading ARM toolchain..."
wget -q "$TOOLCHAIN_URL"

echo ">>> Extracting toolchain to nrf-sdk directory..."
tar -xjf "$TOOLCHAIN_FILE" -C nrf-sdk

echo ">>> Downloading nRF5 SDK 12.3.0..."
wget -q https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v12.x.x/nRF5_SDK_12.3.0_d7731ad.zip

echo ">>> Extracting nRF5 SDK to nrf-sdk directory..."
unzip -q nRF5_SDK_12.3.0_d7731ad.zip -d nrf-sdk

echo ">>> Downloading nRF5 SDK 15.3.0..."
wget -q https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_15.3.0_59ac345.zip

echo ">>> Extracting nRF5 SDK 15.3.0 to nrf-sdk directory..."
unzip -q nRF5_SDK_15.3.0_59ac345.zip -d nrf-sdk

sed -i.bak 's|GNU_INSTALL_ROOT ?= .*|GNU_INSTALL_ROOT ?= ../../nrf-sdk/gcc-arm-none-eabi-6-2017-q2-update/bin/|' \
nrf-sdk/nRF5_SDK_15.3.0_59ac345/components/toolchain/gcc/Makefile.posix
rm -f Makefile.bak 


echo ">>> Installing Nordic nRF Command Line Tools (v10.24.2)..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo ">>> On macOS, please install nRF Command Line Tools manually from:"
    echo "    https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download"
else
    wget -q https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-24-2/nrf-command-line-tools_10.24.2_amd64.deb
    sudo apt-get update
    sudo apt-get install -y ./nrf-command-line-tools_10.24.2_amd64.deb
fi

python -m venv venv
source venv/bin/activate
pip install -r requirements.txt

make all