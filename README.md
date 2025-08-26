### Model Train Layout Clock (ESP32-S3 DevKitC-1)

This project implements a **fast-time simulation clock** for a model railway layout, using the ESP32-S3 DevKitC-1 as the controller.

* **Core features**

  * **Configurable timescale**: Real time is accelerated (1:1 up to 1:60), so one real second can represent up to one simulated minute.
  * **High-priority hardware timer**: A GPTimer ISR maintains a global “model time” counter (`unix_ts`), ticking in precise *model seconds*.
  * **Task separation across CPU cores**:

    * Core 0 -> timekeeping (ISR + clock logic).
    * Core 1 -> peripherals (LCD, NeoPixel, LEDs, relays).
  * **Inputs**: 8 push buttons (common GND, internal pull-ups, interrupt driven).
  * **Outputs**: 3 discrete status LEDs, one on-board NeoPixel, and a 20×4 LCD (I2C backpack).
  * **Thread-safe tick events**: Each model-second tick can notify display/relay driver tasks to update layout components.

* **Hardware mapping**

  * LCD I2C - GPIO8 (SDA), GPIO9 (SCL).
  * Buttons - GPIO4, 5, 6, 7, 10, 11, 12, 13.
  * LEDs - GPIO35, 36, 37.
  * NeoPixel - built-in on GPIO48.
  * Avoided pins: USB (GPIO19/20), UART0 (GPIO43/44), flash/PSRAM pins.

* **Debugging**

  * Serial monitor (`idf.py monitor`) shows model time ticks.
  * ISR does minimal work (increments counter), logging is done in tasks for safety.

---

In short: **a dual-core, ISR-driven fast-time clock for model railways**, providing precise tick events to synchronize signals, relays, and display feedback across your train layout.

