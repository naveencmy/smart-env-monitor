/**
 * firmgen_telemetry.h — Firmgen live RTOS telemetry component
 *
 * Emits, once at startup, a "boot" line reporting why the PREVIOUS boot
 * ended (crash/reset information — panic, watchdog trip, brownout, etc.),
 * then a JSON "rtos" line over UART stdout every interval that the Firmgen
 * backend can parse and forward to the Live Intelligence Panel.
 *
 * Minimum usage (call once from app_main, after scheduler starts):
 *   #include "firmgen_telemetry.h"
 *   firmgen_telemetry_start();
 *
 * Debug vs. production (Kconfig FG_TELEMETRY_PROFILE, "Firmgen Telemetry" menu):
 *   Debug      — telemetry ON: 500 ms interval, full detail (incl. per-task
 *                top_tasks breakdown) plus the boot/reset event.
 *   Production — telemetry OFF: emits NOTHING. The task, samplers, emitters and
 *                boot event are compiled out, so there is no serial traffic, no
 *                extra task/stack and no sampling cost. It also overrides
 *                FG_PROTO_MONITOR, which shares the same serial stream.
 *                firmgen_telemetry_start() then only applies the log level below.
 *   FG_TELEMETRY_LOG_LEVEL (same menu) additionally sets the runtime ESP_LOG
 *   verbosity via esp_log_level_set() when telemetry starts, independently of
 *   CONFIG_LOG_DEFAULT_LEVEL, so a profile switch can carry a log-level change
 *   without rebuilding every translation unit.
 *
 * CPU usage (optional):
 *   Enable CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y in menuconfig
 *   (Component config → FreeRTOS → Enable FreeRTOS trace facility +
 *    Enable FreeRTOS to collect run time stats).
 *   When enabled, the "cpu" field reports real busy-%, and the debug profile's
 *   "top_tasks" is populated. Without it, "cpu" is always 0 and top_tasks is
 *   omitted — all other metrics are always live.
 *
 * Task count:
 *   Requires CONFIG_FREERTOS_USE_TRACE_FACILITY=y (auto-set when runtime
 *   stats are enabled). Without it, "tasks" reports 0.
 *
 * Works on: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2,
 *            ESP32-P4 — any target supported by ESP-IDF v5.x.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start the background telemetry task.
 * Idempotent — safe to call more than once; only the first call has effect.
 * Must be called after the FreeRTOS scheduler is running (i.e. from app_main
 * or any task, NOT from a constructor or early-init hook).
 */
void firmgen_telemetry_start(void);

#ifdef __cplusplus
}
#endif
