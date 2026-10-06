# esp8266-rtos

A Wi-Fi scanner for the ESP8266 (ESP-12F module), written against Espressif's FreeRTOS-based **ESP8266 RTOS SDK** in plain C. It doesn't use Arduino.

Every 10 seconds a FreeRTOS task scans for nearby networks and prints each one over serial:

```
SSID: MyNetwork | RSSI: -54 dBm | Channel: 6 | Auth: WPA2
SSID: Cafe_Guest | RSSI: -81 dBm | Channel: 11 | Auth: OPEN
--------------------------------------------
```

It only lists network names and signal details. It doesn't connect to any network or store credentials.

## Hardware

- ESP-12F module (ESP8266, 4 MB flash), PlatformIO board `esp12e`
- USB-to-serial adapter on COM5 at 115200 baud (the module has no USB of its own)
- GPIO0 and RESET buttons for entering flash mode

## Build and flash

Requires [PlatformIO](https://platformio.org/). The board has no auto-reset circuit, so put it in flash mode by hand:

1. Hold **GPIO0**.
2. Press and release **RESET**.
3. Release **GPIO0**.
4. Run `pio run -t upload`.
5. Press **RESET** to start the new firmware.

Then open the serial monitor:

```bash
pio device monitor
```

## Things worth knowing about this SDK

PlatformIO's `esp8266-rtos-sdk` framework is Espressif's **older** RTOS SDK, not the ESP-IDF-style one, and most online examples don't match it:

- The entry point is `user_init()`, not `app_main()`, and every project must define `user_rf_cal_sector_set()` or it won't link.
- Wi-Fi scanning works through a callback (`wifi_station_scan()`), and the results arrive as a linked list.
- **UART0 starts at 74880 baud.** The SDK's startup code hard-codes it, so a monitor at 115200 shows garbage that looks like a crash loop. The firmware calls `UART_SetBaudrate(UART0, BIT_RATE_115200)` first thing.
- The firmware uses about 279 KB of flash. A linker-map breakdown showed over 99% of that is Espressif's precompiled libraries (lwIP, WPA, PHY, 802.11), not application code. Two attempts to shrink it (removing float printf, switching to `iprintf`) didn't reduce the size.

[CLAUDE.md](CLAUDE.md) has the full notes.
