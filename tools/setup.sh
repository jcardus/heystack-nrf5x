#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

brew install gcc-arm-embedded
brew install openocd
brew install nrfutil
brew install --cask nordic-nrf-command-line-tools
brew install wget

echo ">>> Downloading nRF5 SDK 12.3.0..."
wget -q -c https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v12.x.x/nRF5_SDK_12.3.0_d7731ad.zip

echo ">>> Extracting nRF5 SDK to nrf-sdk directory..."
unzip -q -o nRF5_SDK_12.3.0_d7731ad.zip -d ../nrf-sdk

echo ">>> Downloading nRF5 SDK 15.3.0..."
wget -q -c https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_15.3.0_59ac345.zip

echo ">>> Extracting nRF5 SDK 15.3.0 to nrf-sdk directory..."
unzip -q -o nRF5_SDK_15.3.0_59ac345.zip -d ../nrf-sdk

echo ">>> Configuring GCC toolchain in Makefile.posix..."
TOOLCHAIN_BIN="$(dirname "$(which arm-none-eabi-gcc 2>/dev/null || echo "/opt/homebrew/bin/arm-none-eabi-gcc")")/"
TOOLCHAIN_VERSION="$(arm-none-eabi-gcc -dumpversion 2>/dev/null || echo "15.3.1")"

for sdk_dir in ../nrf-sdk/nRF5_SDK_*; do
  makefile_posix="${sdk_dir}/components/toolchain/gcc/Makefile.posix"
  if [[ -f "${makefile_posix}" ]]; then
    cat <<EOF > "${makefile_posix}"
GNU_INSTALL_ROOT ?= ${TOOLCHAIN_BIN}
GNU_VERSION ?= ${TOOLCHAIN_VERSION}
GNU_PREFIX ?= arm-none-eabi
EOF
    echo "Configured ${makefile_posix} (GNU_INSTALL_ROOT=${TOOLCHAIN_BIN})"
  fi
done
