# Contributing to Smart Environmental Monitor Node

Thank you for your interest in contributing to the **CraftifAI FirmGen Smart Environmental Monitor** project!

## Development Workflow

1. **FirmGen Architecture Guidelines**:
   - Firmware core modules reside in `env_monitor_node/firmware/`.
   - ESP32 hardware adaptation layers (BSP, OSAL, NimBLE, GPIO drivers) reside in `env_monitor_node/firmware/platforms/esp32/`.
   - Entry points and IDF registration reside in `env_monitor_node/main/`.

2. **Coding Standards**:
   - Adhere to C99 / GNU17 standard.
   - Use deterministic FreeRTOS task prioritization.
   - Ensure all public functions are declared in their respective header files with Doxygen-style docstrings.

3. **Requirement Traceability**:
   - Any new feature or bug fix must reference an associated requirement ID or document a new one in `PRD.md`.
   - Ensure `TRACEABILITY_REPORT.md` is kept synchronized.

4. **Testing & Verification**:
   - Run boundary unit tests for threshold calculations.
   - Verify UART JSON schema validity and NimBLE payload frames before opening a Pull Request.

## Pull Request Process

1. Fork the repository and create your branch from `main`.
2. Commit with conventional commit messages (e.g. `feat: add low-power sleep mode`).
3. Ensure clean compilation without warnings under `idf.py build`.
4. Submit the PR for review with acceptance logs attached.
