make stflash-nrf51822_xxac-dcdc HAS_DEBUG=1
pkill openocd
openocd -f openocd.cfg \
      -c "init" \
      -c "rtt setup 0x20000000 0x10000 \"SEGGER RTT\"" \
      -c "reset halt" \
      -c "rtt start" \
      -c "rtt server start 9090 0" &
sleep 1
nc -v localhost 9090
