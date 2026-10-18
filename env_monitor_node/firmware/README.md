# env_monitor_node

ESP-IDF 5.5 environmental monitor demonstration for an ESP32-WROOM-32 carrier. Firmware samples ADC1 channel 6 (GPIO34), averages the last four samples, derives synthetic ESI and temperature values, monitors the active-low GPIO0 button, drives the active-high GPIO2 carrier LED, accepts UART0 commands at 115200 baud, emits JSON telemetry, and has a NimBLE advertising implementation for `CRAFTIFAI_ENV_01`.

The active-high GPIO2 LED polarity was selected by the user. The ESP32-WROOM-32 module itself does not include a BOOT button or LED; actual GPIO0/GPIO2 functionality depends on the carrier matching the PRD and was not physically confirmed.

## Measurement limits

GPIO34 has no external analog input connected under the zero-external-wiring requirement. The ADC reading is therefore not a real environmental or temperature sensor input. ESI (`raw * 500 / 4095`) and temperature (`24 + raw * 16 / 4095` °C) are synthetic demo mappings only. A held button or `ALERT_ON` sets the synthetic snapshot to 45.0 °C and ESI 420.

The PRD specifies 11 dB attenuation; this firmware uses ESP-IDF's `ADC_ATTEN_DB_12` enum, the nearest available setting. GPIO0 has the internal pull-up enabled. UART0 uses GPIO1 TX and GPIO3 RX, the standard console pins.

## Runtime behavior

- Sensor sampling and `[SENSOR]` lines are scheduled every 1000 ms; JSON telemetry is also scheduled every 1000 ms.
- `evaluate_env_state()` gives CRITICAL precedence for temperature above 35 °C, ESI above 300, or a held button; then WARNING for the stated inclusive bands; otherwise NORMAL.
- The button is polled every 20 ms. A transition sets the LED active immediately, then runs NORMAL 100/1900 ms, WARNING 400/400 ms, or CRITICAL 100/100 ms patterns.
- UART commands are newline-terminated `ALERT_ON`, `ALERT_OFF`, and `STATUS`. ALERT_ON latches the critical override until ALERT_OFF; a held button remains critical after clearing it.
- NimBLE advertises the device name. JSON and state reports are UART-only.

## Verification status and limitations

The ESP-IDF build tool reported success, but its reported build time was 0.1 seconds and it could not locate the configured ESP-IDF installation previously (`C:\Espressif\frameworks\esp-idf\tools\idf.py` was missing). The BLE registry dependency could not be inspected or confirmed in the project manifest, and the IDF examples/components directories were unavailable to the tools. Therefore this build result does not establish that the C sources, NimBLE dependency, or firmware image were actually compiled and linked. No host boundary tests were run. No ESP32 device was detected, so UART output, BLE visibility, button response, LED pattern timing, and physical operation remain unverified.

A real carrier is required to validate GPIO0 button and GPIO2 LED behavior. The selected LED polarity is an assumption based on the user's answer, not a schematic verification. Doxygen was not installed, so the HTML API report was not generated.

## Layer breakdown

- `firmware/app/`: application startup.
- `firmware/services/`: ADC smoothing, state/snapshot updates, button polling, command handling, LED cadence, JSON telemetry.
- `firmware/modules/`: portable threshold classifier.
- `firmware/interfaces/`: hardware-port contracts.
- `firmware/platforms/esp32/`: ESP-IDF ADC/GPIO/UART, OSAL and NimBLE adapters.
- `firmware/bsp/`: board pin and active-level declarations.
- `firmware/configs/app_config.h`: centralized hardware, sampling, timing, threshold, and buffer constants.
