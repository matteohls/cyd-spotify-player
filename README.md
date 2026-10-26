# CYD Web Terminal — ESP-IDF, no PlatformIO

Connect to Wi-Fi AP **CYD-TERMINAL** (password **terminal123**) and open **http://192.168.4.1**.

Commands: help, info, uptime, heap, wifi, partitions, version, echo TEXT, clear, reboot.

The web page uploads a compatible ESP32-C5 application .bin to the inactive OTA slot. It does not execute arbitrary Linux/desktop binaries. Build artifact is published by GitHub Actions as `cyd-web-terminal-firmware`.

This version's interaction is browser-based. It does not implement an LCD text console. Confirm the exact board revision and flash size before flashing. OTA requires this firmware's compatible ESP32-C5 app image, not a full flash image.
