brew install gcc-arm-embedded
brew install openocd
brew install nrfutil
brew install --cask nordic-nrf-command-line-tools
brew install wget

echo ">>> Downloading nRF5 SDK 12.3.0..."
wget -q https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v12.x.x/nRF5_SDK_12.3.0_d7731ad.zip

echo ">>> Extracting nRF5 SDK to nrf-sdk directory..."
unzip -q nRF5_SDK_12.3.0_d7731ad.zip -d ../nrf-sdk
# GNU_INSTALL_ROOT := /opt/homebrew

echo ">>> Downloading nRF5 SDK 15.3.0..."
wget -q https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_15.3.0_59ac345.zip

echo ">>> Extracting nRF5 SDK 15.3.0 to nrf-sdk directory..."
unzip -q nRF5_SDK_15.3.0_59ac345.zip -d ../nrf-sdk
# GNU_INSTALL_ROOT ?= /opt/homebrew/bin/
