# Model Train Layout Clock (ESP32-S3 DevKitC-1)

**Short description**

A compact, fast-time simulation clock for model railway layouts running on an ESP32‑S3 DevKitC‑1. Uses a hardware GPTimer ISR to keep precise *model-seconds*, a configurable timescale (up to 1:60), a small button-driven UI on a 20×4 I2C LCD, LEDs/NeoPixel for status, and NVS persistence.

---

## Core features

* **Configurable timescale:** 1:1 up to 1:60 (default 1:2).
* **Accurate tick source:** GPTimer ISR increments a global `unix_ts` (model Unix seconds). ISR does minimal work; tick values are queued to tasks.
* **Dual‑core separation:** Core 0 handles timekeeping/ISR; Core 1 runs peripherals (LCD, NeoPixel, LEDs, button task, tick consumer).
* **Inputs / Outputs:** 8 push buttons (active low, internal pull-ups), 3 discrete LEDs, 1 built‑in NeoPixel, 20×4 I2C LCD (PCF8574 backpack typical).
* **Event driven:** Custom ESP event loop used for `EVENT_MODEL_TICK`, button events, timer control, and LCD updates.
* **Persistence:** Model time, real time and timescale saved to NVS.

---

## Important defaults

* `DEFAULT_UNIX_TS` = `2025-01-01 00:00:00` (used when no saved model time).
* `DEFAULT_TIMESCALE` = `2` (1:2 by default).
* `MAX_TIMESCALE` = `60`.
* `TIMER_RES_HZ` = `1000000` (1 MHz timer resolution).

---

## Hardware (default pin mapping)

* **I2C (LCD):** SDA = GPIO8, SCL = GPIO9, I2C address `0x27` (changeable in `lcd_driver.h`).
* **Buttons (8):** GPIO4,5,6,7,10,11,12,13 (common GND, falling-edge interrupt).
* **Status LEDs:** GPIO35 (green), GPIO36 (amber), GPIO37 (red).
* **NeoPixel:** GPIO48 (led\_strip RMT driver).

> Avoid using USB/UART/flash-related pins reserved by the board.

---

## Quick build & flash

```bash
# set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# build
idf.py build

# flash (replace PORT)
idf.py -p /dev/ttyUSB0 flash

# monitor logs
idf.py -p /dev/ttyUSB0 monitor
```

---

## Quick start

1. Wire hardware according to the pin mapping above.
2. Flash firmware and open the serial monitor.
3. On first boot the device loads defaults and shows a splash/clock screen. Press **MENU** to open the menu; **START/STOP** toggles run/pause.

---

## Where to look in source

* `timer.*` — GPTimer, `unix_ts`, timescale control.
* `lcd_driver.*` — I2C LCD double-buffered renderer and screens.
* `button_driver.*` — ISR + debounce + button task.
* `led_driver.*` — discrete LEDs + NeoPixel handling.
* `state_machine.*` — UI/menu/edit logic.
* `storage.*` — NVS read/write of persisted values.

---

## License

MIT

---

If you want this short README saved to a file in the repo (or exported to PDF), or if you want to tweak wording or the defaults shown, tell me which change and I will update it.
