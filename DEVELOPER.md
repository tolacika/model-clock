# Developer Guide — Model Train Layout Clock (ESP32‑S3 DevKitC‑1)

This developer guide documents internals, architecture, and implementation details of the Model Train Layout Clock project. It is intended for contributors and maintainers who will extend, debug or port this project.

---

## Project overview

This firmware implements a **fast-time model clock** where a hardware GPTimer increments a `unix_ts` global counter in *model-seconds*. A configurable timescale maps real time to model time (for example 1 real second → 2 model seconds for timescale 2). The design emphasizes deterministic timekeeping (minimal ISR), separation of concerns via tasks pinned to the second core for UI and peripherals, and a lightweight event-driven API for components to react to model tick events.

Key design goals:

* Precise model-second ticks produced by hardware timer.
* Minimal ISR workload (increment + queue-from-ISR) to ensure low jitter.
* Subscriber-friendly model tick dispatch via a consumer task that posts events to a custom event loop.
* Responsive, debounced button handling and double-buffered LCD rendering.
* Non-volatile persistence of essential state across reboots.

---

## High-level architecture

1. **GPTimer ISR** (IRAM, Core 0) — increments `unix_ts` by 1 (model second), pushes the tick value to `tick_queue` using `xQueueSendFromISR`.
2. **Tick consumer task** (pinned to Core 1) — blocks on `tick_queue`, receives tick values and posts `EVENT_MODEL_TICK` to the `CUSTOM_EVENTS` event loop.
3. **Subscribers** — e.g., `lcd_driver`, `led_driver`, or layout relay tasks subscribe to `EVENT_MODEL_TICK` and update UI/outputs.
4. **Buttons** — ISR-driven falling-edge interrupts enqueue GPIO numbers to `gpio_evt_queue`; a `button_task` reads the queue, does active-low reading and posts `EVENT_BUTTON_PRESS`.
5. **Event loop** — custom FreeRTOS event loop created via `esp_event_loop_create`, used as the single in-process event bus.
6. **State machine** — subscribes to button events and certain timer events to present UI and control state transitions.
7. **Storage (NVS)** — `storage_load()` restores times and timescale, and `storage_save()` persists current values periodically.

The design ensures the ISR is tiny and that any heavy lifting (string formatting, logging, LCD/LED updates) happens outside ISR context.

---

## Core components and files

* `main.c` — system init, subscribe simple tick logger, heartbeat loop with periodic storage save.
* `event_handler.h/c` — custom event loop wrapper (`events_init`, `events_post`, `events_subscribe`). Use this abstraction instead of calling `esp_event_*` everywhere.
* `timer.h/c` — GPTimer init, timescale control, `unix_ts` volatile global, tick queue and tick consumer task.
* `lcd_driver.h/c` — I2C initialization, double-buffered 20×4 LCD rendering, multiple screen layouts, and LCD update task.
* `button_driver.h/c` — button pin config, ISR, debounce via tick compare, and button dispatch task.
* `led_driver.h/c` — discrete GPIOs and NeoPixel handling via `led_strip` RMT driver.
* `state_machine.h/c` — UI state machine (clock/menu/edit), mapping buttons to actions and applying changes.
* `storage.h/c` — NVS read/write helpers and save/load logic.

---

## Timing and timescale design

### Key constants

* `TIMER_RES_HZ = 1'000'000` (1 MHz) — selected so alarm\_count = TIMER\_RES\_HZ / timescale yields an alarm every model second.
* `DEFAULT_TIMESCALE = 2`, `MAX_TIMESCALE = 60`.

### How timescale works

`timer_set_timescale(new_scale)` computes `alarm_count = TIMER_RES_HZ / new_scale` and sets the GPTimer alarm with that count and `auto_reload_on_alarm = true`.

* If `timescale = 1`, `alarm_count = 1'000'000` → ISR triggers once per real second → 1 real sec = 1 model sec.
* If `timescale = 60`, `alarm_count = 1'000'000 / 60 ≈ 16666` → ISR triggers every \~0.016666s => increments `unix_ts` once per model second, so 1 real sec -> 60 model secs.

This design uses the timer's resolution to trigger the ISR at the appropriate cadence; the ISR increments a model-second counter (not real time) for deterministic simulation.

**Note**: rounding of `TIMER_RES_HZ / timescale` means slight quantization errors can occur in high timescales; if extreme precision is needed, consider using fractional accumulation in software or higher timer resolution.

---

## Interrupts, ISR and IRAM rules

* ISR marked `IRAM_ATTR` and kept small: only increments `unix_ts`, pushes `ts` to `tick_queue` with `xQueueSendFromISR`, and yields via `portYIELD_FROM_ISR` if a higher-priority task was woken.
* **Strict rule**: Do not call `ESP_LOGx`, `malloc`, `esp_event_post`, or functions that can block from ISR. Keep ISR deterministic.
* Ensure callback pointers and used variables are safe from races. `unix_ts` is `volatile uint32_t` to avoid compiler reordering — consumers should copy the value out quickly.

---

## Event loop and message flow

A single custom event loop `CUSTOM_EVENTS` is created with a task pinned to core 0 (`task_core_id = 0`). Use these helper functions:

* `events_post(event_id, event_data, size)` — post to the loop.
* `events_subscribe(event_id, handler, arg)` — register handler.

Important event IDs (see `event_handler.h`):

* `EVENT_MODEL_TICK` — payload: `uint32_t` tick value (model unix second). Posted by `tick_consumer_task`.
* `EVENT_BUTTON_PRESS` — `uint8_t` button index.
* `EVENT_TIMER_RESUME`, `EVENT_TIMER_PAUSE`, `EVENT_TIMER_SCALE` — control timer operations.
* `EVENT_TIMER_STATE_CHANGE` — posted by `timer` when resumed/paused; useful for LED/UI updates.
* `EVENT_LCD_UPDATE` — triggers an immediate re-render of the LCD (`lcd_driver` subscribes).

**Ordering/Latency**

Event loop uses an internal queue (`queue_size = 10` by default) — if many events are posted concurrently the queue can fill. Keep payloads small and use dedicated queues for high-frequency events (which is why ticks are queued via `tick_queue` and then posted to the event loop by the consumer task).

---

## Tasking and core affinity

Current tasking decisions:

* `custom_evt_loop` — created with `.task_core_id = 0` (event handling on core 0). Keep handlers lightweight to avoid starving core 0. If heavy handlers are used, consider pinning the event loop to core 1 or offloading work to tasks.
* `gptimer_isr` — runs on default interrupt core (IRAM).
* `tick_consumer_task` — pinned to core 1 for offloading model tick processing.
* `lcd_update_task` — pinned to core 1 (graphics, string formatting, I2C).
* `button_task`, `status_led_task` — currently not pinned (tskNO\_AFFINITY) or pinned to core 1; ensure they do not block critical tasks.

**Recommendations**

* Keep CPU-sensitive timekeeping on core 0 and UI/peripheral workload on core 1.
* If you change the event loop core, audit all handlers to ensure they are suitable for that core's load.

---

## LCD driver details

File: `lcd_driver.c`

### Summary

* Double-buffered text buffer: `lcd_buffer[2][80]` (20×4). One buffer is drawn into (`draw`), the other is active and used for sending to hardware. Buffers are swapped before I2C write.
* Rendering occurs in `lcd_render_cycle()` which chooses the active screen (`screen_clock`, `screen_settings`, `screen_editing`, etc.) and fills the draw buffer.
* `lcd_render()` compares double-buffers and avoids re-sending identical frames to reduce I2C traffic.
* `i2c_send_4bit_data()` implements the PCF8574-style 4-bit LCD protocol over I2C using the backpack's data lines.
* A dedicated `lcd_update_task()` sleeps for `1000 / LCD_FPS` ms or wakes on `xTaskNotifyGive` to render immediately when `EVENT_LCD_UPDATE` is posted.

### Key behavior notes

* `lcd_init_cycle()` performs the specific sequence to initialize an HD44780-compatible controller in 4-bit mode via I2C backpack.
* When changing the screen layout, always call `events_post(EVENT_LCD_UPDATE, NULL, 0)` so the LCD task renders promptly.
* `lcd_toggle_backlight()` updates the backlight bit via `i2c_master_transmit` directly.

### Race conditions

* `lcd_buffer_index_draw` / `lcd_buffer_index_active` swapping must remain single-threaded. Rendering and buffer writes are done in the LCD task; other tasks should not access the `lcd_buffer` directly.

---

## Button driver details

File: `button_driver.c`

### Summary

* Buttons use falling-edge interrupt (`GPIO_INTR_NEGEDGE`) and internal pull-ups.
* An ISR (`gpio_isr_handler`) checks debounce by comparing `xTaskGetTickCountFromISR()` with `last_isr_tick[button_index]` to ensure spacing >= `BUTTON_DEBOUNCE_MS`.
* When debounce allows, ISR pushes the GPIO number into `gpio_evt_queue` through `xQueueSendFromISR`.
* `button_task` receives the GPIO number, then reads `gpio_get_level(io_num)` (active-low) and maps to a `button_num` index and posts `EVENT_BUTTON_PRESS` with the `uint8_t` index.

### Debounce rationale

* Debounce in ISR uses tick comparison instead of software timers or delayed work. This keeps detection fast and requires no extra heap. The downside: system tick resolution bounds debounce granularity.

### Points to consider

* If your buttons are jittery or cause missed presses, increase `BUTTON_DEBOUNCE_MS` or add hardware RC debouncing.
* The ISR installs using `ESP_INTR_FLAG_LEVEL3`. Adjust priority if you need different interrupt behavior.

---

## LED / NeoPixel details

File: `led_driver.c`

* Discrete LEDs are updated by `status_led_task` which polls `target_led_states[]` and applies changes via `gpio_set_level`.
* NeoPixel uses the `led_strip` RMT driver (`led_strip_new_rmt_device`). `max_leds = 1` by default. Use `led_strip_set_pixel` + `led_strip_refresh`.
* `led_event_handler` listens to `EVENT_TIMER_STATE_CHANGE` to update `target_*` values.

Notes:

* NeoPixel RMT timing details are handled by the driver; avoid toggling the NeoPixel from an ISR.
* The `status_led_task` runs with priority 8; reduce if it steals CPU from higher priority tasks.

---

## Storage (NVS) behavior

File: `storage.c`

* Uses NVS namespace `storage` storing `u32` values for `model_ts`, `real_ts`, `timescale`.
* `storage_init()` ensures NVS is initialized/erased if needed.
* `storage_load()` reads values into local struct, posts `EVENT_TIMER_SCALE`, sets `unix_ts` and calls `settimeofday()` using the saved `real_ts`.
* `storage_save()` writes current `unix_ts`, `time(NULL)` and current timescale.

Notes & caveats:

* `nvs_set_u32()` and `nvs_get_u32()` are used — watch for `ESP_ERR_NVS_NOT_FOUND` and fallback to defaults.
* If upgrading structures to larger sizes, migrate carefully; consider adding a version key to NVS to allow migrations.

---

## State machine and UI flow

File: `state_machine.c`

* Main UI states: `STATE_INIT`, `STATE_CLOCK`, `STATE_MENU`, `STATE_EDIT`, `STATE_LCD_TEST`, `STATE_RESTART`.
* On `EVENT_EXIT_INIT_STATE` the machine leaves `STATE_INIT` into `STATE_CLOCK`.
* `BUTTON_*` events cause transitions:

  * `BUTTON_START_STOP` toggles timer run/pause (via `EVENT_TIMER_PAUSE` / `EVENT_TIMER_RESUME`).
  * `BUTTON_MENU` toggles between `STATE_CLOCK` and `STATE_MENU`/`STATE_EDIT` depending on context.
  * `BUTTON_OK` in menu starts editing or test screens.
* After any state change, it posts `EVENT_LCD_UPDATE` to trigger immediate redraw.

Editing

* Edit modes: edit real time (uses `settimeofday`), edit model time (sets `unix_ts`), edit timescale (posts `EVENT_TIMER_SCALE`).
* Edit cursor maps to components of the datetime (year, month, day, hour, minute, second).

Testing & validation

* Use serial logs to follow state transitions (`ESP_LOGI` messages in `state_event_handler`).

---

## Build, test, and debug workflow

### Build

* Standard ESP-IDF build: `idf.py build`.
* Use `idf.py menuconfig` to set SDK, partition table, and logging levels.

### Flash & monitor

* `idf.py -p <PORT> flash monitor`.
* The serial monitor displays `ESP_LOG` messages. Watch for `Timer initialized` and `Timescale set to` messages.

### Dynamic debugging

* Use `esp_log_level_set("TAG", ESP_LOG_DEBUG)` (in code or via menuconfig) to increase verbosity for a specific module.
* Add `ESP_LOGI` in handlers to inspect event arrival and handler core/priority.

### Unit testing

* Hardware bindings make unit testing harder. Keep non-hardware logic (time conversions, format\_time, ts\_to\_tm/tm\_to\_ts) separate for host-side unit tests.
* Consider `esp-idf`'s Unity framework for embedded unit tests; test `tm_to_ts`, `format_time` and small state machine transitions where possible.

---

## Common issues & troubleshooting

**Timer never starts**

* Ensure `timer_resume()` is called (the system starts in paused state); `events_post(EVENT_TIMER_RESUME, NULL, 0)` will resume.
* Check `gptimer_enable` and `gptimer_start` return values.

**Model time increments faster/slower than expected**

* Verify `TIMER_RES_HZ` and `alarm_count` (computed in `timer_set_timescale`) are correct.
* Very high timescales suffer quantization; consider using a higher `TIMER_RES_HZ` if hardware supports it.

**LCD shows garbage**

* Wrong I2C address (`0x27` vs `0x3F`) or wiring.
* Some backpacks map DB pins differently; verify backpack type.
* Timing in `lcd_init_cycle` depends on small delays — try increasing `ets_delay_us` between commands for flaky displays.

**Button events lost or debounced incorrectly**

* Confirm ISR was installed successfully.
* Check `gpio_evt_queue` size and `xQueueSendFromISR` success.

**NVS errors**

* Run `nvs_flash_erase` if partition layout changed. Increase NVS partition size via `menuconfig` if necessary.

---

## Extensions & integration points

### Wi‑Fi & SNTP

* A `WiFi + SNTP` menu entry exists as a placeholder. Integrate ESP-IDF Wi‑Fi + SNTP and during `storage_load()` you can optionally sync real time via SNTP and set `unix_ts` accordingly.

### External control protocol (UART/MQTT)

* Add a task that subscribes to `EVENT_MODEL_TICK` and publishes a simple JSON state via UART or MQTT. Use an additional queue to avoid blocking event handlers.

### Multiple NeoPixels / RGB indicator per module

* Extend `led_driver` to support `max_leds > 1` and maintain a small ring buffer or state table for each LED.

### Relay outputs for layout

* Add a `relay_driver` module that subscribes to `EVENT_MODEL_TICK` and uses a schedule to toggle relays at model times.

---

## Coding style & contribution notes

* Use `ESP_ERROR_CHECK` for ESP-IDF API calls during initialization. In production code, prefer graceful error handling.
* Keep ISRs minimal and in IRAM when necessary.
* Use `events_post` and `events_subscribe` rather than calling `esp_event_*` directly across files.
* Document any new events in `event_handler.h` and ensure subscribers are registered during initialization.
* Keep global variables to a minimum. `unix_ts` is intentionally global (volatile) — wrap in accessor functions if needed.

---

## Appendix: constants, macros and useful snippets

### Important macros

* `DEFAULT_UNIX_TS` — default model timestamp (set in `timer.h`).
* `DEFAULT_REAL_TS` — default saved real timestamp (set in `timer.h`).
* `DEFAULT_TIMESCALE`, `MAX_TIMESCALE` in `timer.h`.
* `LCD_COLS`, `LCD_ROWS`, `LCD_ROW_OFFSET` in `lcd_driver.h`.

### Useful debugging snippet (print model time)

```c
char buf[21];
format_time(unix_ts, buf, sizeof(buf));
ESP_LOGI("debug", "model time: %s", buf);
```

### Queue-from-ISR pattern (already used)

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
uint32_t ts = unix_ts;
xQueueSendFromISR(tick_queue, &ts, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

---

If you want I can:

* Export this developer guide to `DEVELOPER.md` in the repository, or
* Split it into smaller focused docs (eg. `lcd_driver.md`, `timer_design.md`), or
* Add sequence diagrams (text ASCII or Mermaid) showing the tick flow and UI actions.

Which next step would you like?
