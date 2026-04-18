### Compile the Firmware

```
make all # Compile all the supported devices and place them in the release folder
make merge_nrf52810_xxaa-dcdc APPLE_KEY='"\x01\x01...\x20"' GOOGLE_KEY='"\x48x0b\x52\x6e\x20\x30"' HAS_BATTERY=1
```

