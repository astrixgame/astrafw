# AstraFW

Custom ESP-IDF firmware for the **[Waveshare ESP32-C6 Touch AMOLED 2.06](https://www.waveshare.com/esp32-c6-touch-amoled-2.06.htm)** — a smartwatch-form-factor development board. This is not a generic framework or demo; it is a purpose-built firmware targeting every peripheral on the board with a clean, modular driver layer written in C on top of ESP-IDF 5.5+.

> 📄 Official hardware docs: [docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.06](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.06)

---

## Target hardware

| Component | Part | Notes |
|---|---|---|
| MCU | ESP32-C6 | RISC-V 32-bit, up to 160 MHz, 16 MB flash |
| Wireless | WiFi 6 · BT 5 · Zigbee 3.0 / Thread | Onboard antenna |
| Display | 2.06″ AMOLED 410×502 · 16.7M colors | CO5300 driver via QSPI |
| Touch | FT3168 | I2C, up to 400 kHz |
| PMIC | AXP2101 | LiPo charge/discharge, multi-rail output |
| RTC | PCF85063A | Battery-backed, countdown timer |
| IMU | QMI8658 | 3-axis accel + 3-axis gyro |
| Audio | ES8311 codec + ES7210 echo cancellation | I2S |
| Storage | 16 MB internal flash (SPIFFS + NVS) | |

---

## Project structure

```
main/
├── hardware.h              # All GPIO and bus pin assignments
├── err_check.h             # Lightweight error-check macros
├── main.c
├── display_driver/         # AMOLED panel, touch, brightness (LEDC PWM)
├── i2c_driver/             # Shared I2C master — one bus, get handle anywhere
├── i2s_driver/             # I2S audio output via ES8311
├── power_driver/           # AXP2101 — voltages, battery %, charge state, IRQ
├── rtc_driver/             # PCF85063A — time get/set + countdown timer
├── spiffs_driver/          # SPIFFS filesystem mount/unmount
└── nvs_driver/             # NVS flash wrapper — int32, uint8, string, blob
```

---

## Drivers

### `i2c_driver`
Owns the single I2C master bus. Every other I2C driver calls `i2c_get_handle()` — no handle passing, no init ordering boilerplate.

### `display_driver`
Drives the CO5300 AMOLED over QSPI and the FT3168 touch controller over I2C. Exposes pixel, line, fill, and flush primitives plus `display_brightness_set()` via LEDC.

### `power_driver`
Full AXP2101 driver. Reads battery voltage, VBUS, VSYS, battery percentage, and charge state. Registers a user callback fired on PMIC hardware interrupts (USB connect/disconnect, charge done, low battery, power key short/long press, over-voltage, temperature warning).

### `rtc_driver`
PCF85063A driver. Read/write time via `dtime_t`. Full access to the hardware **countdown timer**:

```c
// Countdown 30 s, raise the INT pin when done
rtc_start_timer(30, RTC_TIMER_CLK_1HZ, true);

// Poll from any task
if (rtc_is_timer_expired()) {
    rtc_clear_timer_flag();
    // handle expiry
}

rtc_stop_timer(); // disable early
```

Available clock sources:

| Constant | Rate | Max range |
|---|---|---|
| `RTC_TIMER_CLK_4096HZ` | 4096 Hz | ~62 ms |
| `RTC_TIMER_CLK_64HZ` | 64 Hz | ~4 s |
| `RTC_TIMER_CLK_1HZ` | 1 Hz | 255 s |
| `RTC_TIMER_CLK_1_60HZ` | 1/60 Hz | 255 min |

### `spiffs_driver`
Mounts the SPIFFS partition for read/write file access on internal flash.

### `nvs_driver`
Persistent key-value storage over ESP-IDF NVS flash. Configurable namespace via `STORAGE_NAMESPACE`. Supports `int32`, `uint8`, `string`, and raw `blob`.

---

## Build

Requires the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/) toolchain **≥ 5.5.0**.

```bash
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

Component dependencies in `main/idf_component.yml` are resolved automatically by the IDF Component Manager on first build — no manual installs needed.

---

## License

[Apache 2.0](LICENSE)
