# esp8266-rtos

PlatformIO ESP8266 ESP-12F project using the `esp8266-rtos-sdk` framework (FreeRTOS-based, IDF-style — NOT Arduino).

## Hardware

- Board: ESP-12F module (ESP8266, 4MB flash), PlatformIO board id `esp12e`.
- Upload/monitor: COM5 @ 115200 baud (USB-to-serial adapter; module has no onboard USB).
- GPIO0 is the boot-mode strap pin: pulled low = bootloader/flash mode, floating/high = normal run mode.

## Flash procedure

The board has no auto-reset/auto-program circuit, so entering bootloader mode is manual:

1. Hold the **GPIO0** button down.
2. Press and release **RESET** (while still holding GPIO0).
3. Release **GPIO0** — the module is now in flashing mode.
4. Run the upload (`pio run -t upload`).
5. After upload completes, press **RESET** once to boot the new firmware normally.

## Known ESP8266 RTOS SDK Gotchas

1. **PlatformIO's `espressif8266` platform's `esp8266-rtos-sdk` framework is NOT ESP-IDF-style**, despite the name. It's Espressif's older RTOS SDK (FreeRTOS kernel + legacy non-OS-style WiFi API), the same generation as the NONOS SDK. There is no `app_main()`, no `esp_wifi_scan_start()`/`esp_wifi_scan_get_ap_records()`, no `esp_event_loop.h`, and no `nvs_flash.h`. Confirmed by reading the installed package at `~/.platformio/packages/framework-esp8266-rtos-sdk/include/espressif/` and the platform's own bundled example at `~/.platformio/platforms/espressif8266/examples/esp8266-rtos-sdk-blink/src/main.c`.
2. **Entry point is `void user_init(void)`**, not `app_main()`. Include `esp_common.h` (pulls in FreeRTOS + the legacy WiFi/system API), not `esp_wifi.h`/`esp_system.h` IDF headers directly.
3. **`uint32 user_rf_cal_sector_set(void)` is mandatory** in every project — the SDK calls it to pick which flash sector holds RF calibration data. Omitting it is a link error. Copy the standard `switch` on `system_get_flash_size_map()` from the platform's blink example.
4. **WiFi scanning is callback-based**, not blocking/polling: `wifi_station_scan(struct scan_config *config, scan_done_cb_t cb)` — pass `NULL` for the default config (all channels, not hidden) and a callback of type `void (*)(void *arg, STATUS status)`. The callback receives a `struct bss_info *` linked list (walk it via `bss->next.stqe_next`), not an array — there is no "get scan results" function to call after the fact.
5. Station mode is set with `wifi_set_opmode(STATION_MODE)` (legacy enum), not `esp_wifi_set_mode(WIFI_MODE_STA)`.
6. Auth mode enum is `AUTH_MODE` with values `AUTH_OPEN`, `AUTH_WEP`, `AUTH_WPA_PSK`, `AUTH_WPA2_PSK`, `AUTH_WPA_WPA2_PSK`, `AUTH_MAX` — not the IDF `wifi_auth_mode_t`/`WIFI_AUTH_*` names.
7. `vTaskDelay()` intervals should be divided by `portTICK_RATE_MS` (this SDK's name for the tick-to-ms constant), not `portTICK_PERIOD_MS`.
8. **UART0 comes up at 74880 baud, not 115200, and stays there unless you change it.** The SDK's own startup calls `uart_init_new()` (in `driver_lib/driver/uart.c`), which hardcodes `BIT_RATE_74880`. There is no exposed API to pass a different baud into that init call — the older `uart_init(void)` that took a config struct is `#if 0`'d out in this SDK version. Symptom: `pio device monitor` (or any terminal opened at 115200, matching `platformio.ini`'s `monitor_speed`) shows continuous garbage — looks exactly like a crash/reboot loop, but the firmware is running fine, just at the wrong baud. Fix: call `UART_SetBaudrate(UART0, BIT_RATE_115200)` (from `uart.h`) at the top of `user_init()`, before any `printf`.
9. **Flash usage (~279KB/376KB app partition) is ~99% vendor SDK, not application code — and it isn't meaningfully prunable.** Confirmed by generating a real linker map (`-Wl,-Map,...`) and attributing every byte in the `.text`/`.irom0.text`/`.data`/`.rodata` sections to its source archive member. Our own `src/main.o` accounts for well under 1KB; the rest is `libphy.a` (~47KB, RF calibration), `liblwip.a` (~46KB, full TCP/IP stack — TCP, DHCP client *and server*, DNS, IPv6/nd6/mld6), `libpp.a` (~45KB, packet processing/MAC), `libnet80211.a` (~44KB, 802.11 mgmt — including AP-mode/hostap code), `libmain.a` (~29KB, WiFi core glue, dominated by a single 16.5KB `user_interface.o`), `libwpa.a` (~19KB, WPA supplicant *and authenticator* crypto handshake), plus `libcirom.a`/`libcrypto.a`/`libfreertos.a` (~39KB combined: libc printf/dtoa, SHA1/MD5/AES, FreeRTOS kernel).
   - None of this is separable at the application level: these are precompiled binary blobs (no source shipped via PlatformIO), and enabling `wifi_set_opmode(STATION_MODE)` + calling `wifi_station_scan()` unconditionally pulls in the full lwIP stack (DHCP server included, despite never enabling SoftAP) and the full WPA authenticator+supplicant code (despite never associating to an AP) — this SDK generation doesn't cleanly separate "scan only" from "full station with IP+WPA" at the object-file level.
   - Tried and empirically ruled out: (a) stripping the framework's forced `-u _printf_float -u _scanf_float` link flags via an `extra_scripts` pre-script — zero byte change, because this toolchain's `printf` always resolves to the float-capable `_vfprintf_r`/`_dtoa_r` path regardless of format specifiers used, and something in the SDK itself (not our code) already pulls that path in. (b) Switching our own `printf()` calls to the integer-only `iprintf()` — this made the binary *larger* (+6.9KB), because the SDK's internal float-capable `printf` path is still linked in by something else internally, so `iprintf` just added a second, redundant print implementation alongside it rather than replacing it.
   - Practical implication: don't expect code-level cleanup in `main.c` to move the flash-usage needle. If flash budget ever becomes a real constraint, the actual levers are architectural (a different SDK/framework generation, or a smaller flash-partition-vs-OTA tradeoff), not application code changes.
