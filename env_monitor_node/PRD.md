# Product Requirements Document (PRD): Smart Environmental Monitor Node (Track 6)

## 1. System Overview
An ESP32-based Smart Environmental Monitor built with CraftifAI FirmGen and ESP-IDF v5.5. The firmware samples ambient temperature, humidity, and air quality index (AQI / EMI via ADC1), evaluates multi-tier safety thresholds, drives physical LED indicators on GPIO 2, broadcasts BLE telemetry beacons, streams structured JSON over UART0, and supports closed-loop serial commands and Live Intelligence runtime telemetry.

## 2. Frozen Requirement Set

| Req ID | Type | Title | Description | Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- |
| **REQ-01** | Explicit | Periodic Sensor Sampling | Sample ADC1_CH6 (GPIO 34) and environmental sensors every 1000ms. Support GPIO 0 (BOOT button) hardware override for live critical hazard testing. | UART logs `[SENSOR] ADC_Raw: XXX | Temp_Est: XX.X C | ESI: XXX` every 1000ms (+/- 50ms). |
| **REQ-02** | Explicit | Multi-Tier Threshold Engine | Classify telemetry into `NORMAL` (Temp < 30C, ESI < 150), `WARNING` (Temp 30-35C or ESI 150-250), or `CRITICAL` (Temp > 35C, ESI > 250, or GPIO 0 LOW). | Immediate `[STATE_CHANGE]` UART log on state transition; 100% pass on boundary tests. |
| **REQ-03** | Explicit | Physical Actuator LED Control | Drive GPIO 2 LED: `NORMAL` = 1900ms heartbeat, `WARNING` = 400ms blink, `CRITICAL` = 100ms rapid alarm strobe. | Physical LED frequency updates within 200ms of threshold breach. |
| **REQ-04** | Explicit | BLE Beacon & UART JSON Feed | Broadcast `CRAFTIFAI_ENV_01` BLE payload and emit single-line JSON over UART0 at 115200 baud every 1000ms. | Valid JSON parsed on UART and BLE advertisement visible on scanner. |
| **REQ-05** | Explicit | Closed-Loop Serial Override | Accept `ALERT_ON`, `ALERT_OFF`, and `STATUS` commands over UART0 to dynamically trigger emergency alerts. | Sending `ALERT_ON` triggers immediate critical alert state. |
| **REQ-06** | Bonus | Live Intelligence Telemetry | Expose RTOS task state, memory metrics, and sensor state struct for runtime inspection. | Telemetry frame emitted every cycle with stack/heap diagnostics. |
