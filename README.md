# CraftifAI FirmGen — Track 6: Smart Environmental Monitor Node

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.5-blue.svg)](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/)
[![Hardware](https://img.shields.io/badge/MCU-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![BLE](https://img.shields.io/badge/Bluetooth-NimBLE-orange.svg)](https://mynewt.apache.org/)
[![License](https://img.shields.io/badge/License-MIT-purple.svg)](LICENSE)

A requirements-driven ESP32 bare-metal / FreeRTOS firmware node generated with **CraftifAI FirmGen** for **Track B (Problem Statement #6: Smart Environmental Monitor)**.

---

## 📌 Architecture & Pin Mapping

| Peripheral | Pin / Channel | Function | Mode |
| :--- | :--- | :--- | :--- |
| **Analog Sensor** | `ADC1_CH6` (`GPIO 34`) | Environmental Severity Index (ESI) & Temperature | Input (ADC OneShot) |
| **Emergency Button** | `GPIO 0` (`BOOT`) | Hardware Hazard Override / Injection | Input (Internal Pull-Up) |
| **Actuator LED** | `GPIO 2` | Visual Status & Hazard Strobe | Output (Push-Pull) |
| **Console / JSON** | `UART0` (115200 baud) | Live JSON Stream & Closed-Loop Command CLI | TX/RX |
| **BLE Radio** | NimBLE Peripheral | Advertising beacon `CRAFTIFAI_ENV_01` | Broadcast Telemetry |

---

## 🎯 Verification & Traceability

- **[PRD.md](PRD.md):** Complete frozen requirement specifications (REQ-01 to REQ-06).
- **[TRACEABILITY_REPORT.md](TRACEABILITY_REPORT.md):** Detailed traceability linking each requirement to source code, verification methods, and acceptance logs.
- **[UART_ACCEPTANCE_EVIDENCE.log](UART_ACCEPTANCE_EVIDENCE.log):** Live hardware capture showing periodic sampling, state transitions (`NORMAL` -> `WARNING` -> `CRITICAL`), JSON telemetry output, and closed-loop command response.

---

## 🚀 Building and Flashing

### Prerequisites
- ESP-IDF v5.5 installed and exported in your shell.

### Build Commands
```bash
cd env_monitor_node
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

---

## 🧪 Acceptance Evidence Summary
All 5 core requirements and bonus Live Intelligence diagnostics have passed verification:
- [x] **REQ-01:** Periodic Onboard ADC & Thermal Sampling (1000ms cadence)
- [x] **REQ-02:** Three-State Threshold Classification Engine (`NORMAL`, `WARNING`, `CRITICAL`)
- [x] **REQ-03:** Physical GPIO 2 LED timing patterns (`1900ms`, `400ms`, `100ms`)
- [x] **REQ-04:** NimBLE Beacon (`CRAFTIFAI_ENV_01`) & UART0 JSON Stream
- [x] **REQ-05:** Closed-Loop Command Receiver (`ALERT_ON` override)
