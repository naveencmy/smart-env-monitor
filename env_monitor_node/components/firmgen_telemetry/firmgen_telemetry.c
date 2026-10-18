/*
 * firmgen_telemetry.c — single-file Firmgen live RTOS telemetry component.
 *
 * Once at startup, emits a "boot" line reporting why the PREVIOUS boot ended
 * (crash/reset information):
 *
 *   {"t":"boot","v":1,"reason":"panic","code":14,"crash":true}
 *
 * ALL of this happens only in the DEBUG profile. In the PRODUCTION profile
 * (CONFIG_FG_TELEMETRY_PROFILE_PRODUCTION) the component emits nothing at all —
 * the task, samplers, boot event and emitters are compiled out entirely, so a
 * shipping device puts no diagnostic traffic on its serial line.
 *
 * In debug it spawns one FreeRTOS task that samples system metrics every 500 ms
 * and emits a JSON v1 line to stdout (UART0):
 *
 *   {"t":"rtos","v":1,"cpu":<n>,"tasks":<n>,
 *    "heap":{"free":<b>,"min":<b>,"total":<b>},
 *    "stack_peak":<n>,"uptime":<s>
 *    [,"psram":{"present":<bool>,"free":<b>,"total":<b>}]
 *    [,"health":{"temp_c":<f>}]
 *    [,"top_tasks":[{"name":<s>,"cpu":<n>,"stack_min":<words>}, ...]]
 *   }
 *
 * PSRAM section appears only on chips with SPIRAM enabled.
 * Health section appears only on chips with a temperature sensor.
 * top_tasks requires CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS; "cpu" there is each
 * task's cumulative share of runtime since boot, not an interval delta.
 * Compatible with ESP32, S2, S3, C3, C6, H2, P4 on IDF v5.x.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_log.h"
#include "firmgen_telemetry.h"

/* ── Tunables ─────────────────────────────────────────────────────────────── */

#define FG_VERSION      1u
/* Must cover the deep printf→newlib-buffered-flush→VFS→UART path. 3072 was marginal (~67% peak)
 * and the sibling proto-monitor emitter crashed at 3072 on larger batches — bumped for headroom. */
#define FG_STACK_BYTES  6144u
#define FG_STACK_WORDS  (FG_STACK_BYTES / sizeof(StackType_t))
#define FG_PRIORITY     1u

/* Debug/production telemetry macros (Kconfig FG_TELEMETRY_PROFILE).
 *
 * Production emits NOTHING: the sampler, the JSON emitters, the boot event and the
 * telemetry task itself are all compiled out below, so a shipping build carries zero
 * UART traffic, no extra task/stack, and no sampling cost. Debug is the only profile
 * that streams. This is a hard on/off, not a reduced rate — a production device must
 * not put diagnostic chatter on its serial line at all.
 *
 * The only thing firmgen_telemetry_start() still does in production is apply
 * FG_TELEMETRY_LOG_LEVEL, which governs the application's own ESP_LOGx output rather
 * than telemetry, and costs one call. */
#if CONFIG_FG_TELEMETRY_PROFILE_PRODUCTION
#define FG_TELEMETRY_ENABLED 0
#else
#define FG_TELEMETRY_ENABLED 1
#define FG_INTERVAL_MS      500u
#define FG_TOP_TASKS_ENABLE 1
#define FG_TOP_TASKS_MAX    3u
#endif

#if FG_TELEMETRY_ENABLED

/* ── Internal metric types ────────────────────────────────────────────────── */

typedef struct {
    uint32_t cpu_pct;
    uint32_t tasks;
    size_t   free_heap;
    size_t   min_heap;
    size_t   total_heap;
    uint32_t stack_pct;
    uint64_t uptime_s;
} fg_core_t;

typedef struct {
    bool   present;
    size_t free;
    size_t total;
} fg_psram_t;

typedef struct {
    bool  valid;
    float temp_c;
} fg_health_t;

typedef struct {
    char     name[configMAX_TASK_NAME_LEN];
    uint32_t cpu_pct;      /* share of total runtime since boot (cumulative, not interval delta) */
    uint32_t stack_min;    /* high-water mark, in words, lowest free the task has ever seen */
} fg_task_t;

/* ── CPU delta sampling ───────────────────────────────────────────────────── */

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS

static uint32_t s_prev_idle  = 0;
static uint32_t s_prev_total = 0;

/* Samples system-wide CPU busy % (interval delta) and, when top_max > 0, the
 * top_max busiest non-idle tasks by cumulative runtime share. One
 * uxTaskGetSystemState() call serves both so a production build that passes
 * top_max=0 pays no extra cost over the original CPU-only sampler. */
static uint32_t sample_tasks(fg_task_t *top, uint32_t top_max, uint32_t *out_top_n)
{
    if (out_top_n) *out_top_n = 0u;

    uint32_t n   = uxTaskGetNumberOfTasks();
    TaskStatus_t *buf = pvPortMalloc(n * sizeof(TaskStatus_t));
    if (!buf) return 0u;

    uint32_t total_ticks = 0u;
    uint32_t found = uxTaskGetSystemState(buf, n, &total_ticks);

    /* Sum idle task counters (IDF names: "IDLE", "IDLE0", "IDLE1"). */
    uint32_t idle_ticks = 0u;
    for (uint32_t i = 0u; i < found; i++) {
        const char *nm = buf[i].pcTaskName;
        if (nm[0]=='I' && nm[1]=='D' && nm[2]=='L' && nm[3]=='E') {
            idle_ticks += buf[i].ulRunTimeCounter;
        }
    }

    if (top && top_max > 0u && total_ticks > 0u) {
        uint32_t top_n = 0u;
        for (uint32_t i = 0u; i < found; i++) {
            const char *nm = buf[i].pcTaskName;
            if (nm[0]=='I' && nm[1]=='D' && nm[2]=='L' && nm[3]=='E') continue;

            uint32_t pct = (uint32_t)(((uint64_t)buf[i].ulRunTimeCounter * 100u) / total_ticks);

            /* Insertion sort into the fixed-size top array, descending by pct. */
            uint32_t insert_at = top_n;
            for (uint32_t j = 0u; j < top_n; j++) {
                if (pct > top[j].cpu_pct) { insert_at = j; break; }
            }
            if (insert_at >= top_max) continue; /* doesn't rank */

            uint32_t shift_end = (top_n < top_max) ? top_n : top_max - 1u;
            for (uint32_t j = shift_end; j > insert_at; j--) {
                top[j] = top[j - 1u];
            }
            strncpy(top[insert_at].name, nm, sizeof(top[insert_at].name) - 1u);
            top[insert_at].name[sizeof(top[insert_at].name) - 1u] = '\0';
            top[insert_at].cpu_pct   = pct;
            top[insert_at].stack_min = (uint32_t)buf[i].usStackHighWaterMark;
            if (top_n < top_max) top_n++;
        }
        if (out_top_n) *out_top_n = top_n;
    }

    vPortFree(buf);

    uint32_t d_total = total_ticks - s_prev_total;
    uint32_t d_idle  = idle_ticks  - s_prev_idle;
    s_prev_total     = total_ticks;
    s_prev_idle      = idle_ticks;

    if (d_total == 0u) return 0u;
    uint32_t idle_pct = (d_idle * 100u) / d_total;
    return (idle_pct > 100u) ? 0u : (100u - idle_pct);
}

#else
static uint32_t sample_tasks(fg_task_t *top, uint32_t top_max, uint32_t *out_top_n)
{
    (void)top; (void)top_max;
    if (out_top_n) *out_top_n = 0u;
    return 0u;
}
#endif

/* ── Core metrics ─────────────────────────────────────────────────────────── */

static void core_sample(fg_core_t *out, UBaseType_t hwm, fg_task_t *top, uint32_t top_max, uint32_t *out_top_n)
{
    out->total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    out->free_heap  = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    out->min_heap   = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    out->cpu_pct    = sample_tasks(top, top_max, out_top_n);
    out->uptime_s   = (uint64_t)(esp_timer_get_time() / 1000000ULL);

    uint32_t used  = (FG_STACK_WORDS > (uint32_t)hwm) ? (FG_STACK_WORDS - (uint32_t)hwm) : 0u;
    out->stack_pct = (FG_STACK_WORDS > 0u) ? ((used * 100u) / FG_STACK_WORDS) : 0u;
    if (out->stack_pct > 100u) out->stack_pct = 100u;

#if configUSE_TRACE_FACILITY
    out->tasks = (uint32_t)uxTaskGetNumberOfTasks();
#else
    out->tasks = 0u;
#endif
}

/* ── PSRAM metrics ────────────────────────────────────────────────────────── */

static void psram_sample(fg_psram_t *out)
{
#if CONFIG_SPIRAM
    out->present = true;
    out->free    = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    out->total   = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
#else
    out->present = false;
    out->free    = 0;
    out->total   = 0;
#endif
}

/* ── Temperature sensor ───────────────────────────────────────────────────── */

/* Opt-in (Kconfig, default off). The __has_include guard means that even if the
 * option is enabled without esp_driver_tsens on the include path, this compiles out
 * gracefully instead of failing the build with "temperature_sensor.h: No such file". */
#if CONFIG_SOC_TEMP_SENSOR_SUPPORTED && CONFIG_FG_TELEMETRY_TEMP_SENSOR && __has_include("driver/temperature_sensor.h")
#include "driver/temperature_sensor.h"
static temperature_sensor_handle_t s_temp     = NULL;
static bool                        s_temp_init = false;

static bool read_temp(float *out_c)
{
    if (!s_temp_init) {
        s_temp_init = true;
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        if (temperature_sensor_install(&cfg, &s_temp) != ESP_OK) { s_temp = NULL; }
        else { temperature_sensor_enable(s_temp); }
    }
    if (!s_temp) return false;
    return temperature_sensor_get_celsius(s_temp, out_c) == ESP_OK;
}
#else
static bool read_temp(float *out_c) { (void)out_c; return false; }
#endif

static void health_sample(fg_health_t *out)
{
    out->valid  = read_temp(&out->temp_c);
    if (!out->valid) out->temp_c = 0.0f;
}

/* ── Reset / crash reason ─────────────────────────────────────────────────── */

/* Maps esp_reset_reason_t to a stable string the backend parser matches on
 * (see rtos_telemetry::schema — keep in sync with the Rust-side is_crash list).
 * is_crash=true for reasons that mean the previous boot did NOT shut down
 * cleanly (panic, watchdog trip, brownout, unexplained CPU lockup). */
static const char *reset_reason_str(esp_reset_reason_t r, bool *is_crash)
{
    *is_crash = false;
    switch (r) {
        case ESP_RST_POWERON:   return "poweron";
        case ESP_RST_EXT:       return "ext_pin";
        case ESP_RST_SW:        return "sw_reset";
        case ESP_RST_DEEPSLEEP: return "deepsleep_wake";
        case ESP_RST_SDIO:      return "sdio";
        case ESP_RST_USB:       return "usb";
        case ESP_RST_JTAG:      return "jtag";
        case ESP_RST_EFUSE:     *is_crash = true; return "efuse_error";
        case ESP_RST_PANIC:     *is_crash = true; return "panic";
        case ESP_RST_INT_WDT:   *is_crash = true; return "interrupt_watchdog";
        case ESP_RST_TASK_WDT:  *is_crash = true; return "task_watchdog";
        case ESP_RST_WDT:       *is_crash = true; return "other_watchdog";
        case ESP_RST_BROWNOUT:  *is_crash = true; return "brownout";
#ifdef ESP_RST_PWR_GLITCH
        case ESP_RST_PWR_GLITCH: *is_crash = true; return "power_glitch";
#endif
#ifdef ESP_RST_CPU_LOCKUP
        case ESP_RST_CPU_LOCKUP: *is_crash = true; return "cpu_lockup";
#endif
        case ESP_RST_UNKNOWN:
        default:                return "unknown";
    }
}

/* Emitted once, the first time the telemetry task runs after boot — reports why
 * the *previous* boot ended, filling the "crash/reset information" gap: the
 * steady-state "rtos" line only ever describes the current, running boot. */
static void emit_boot_event(void)
{
    bool is_crash = false;
    esp_reset_reason_t reason = esp_reset_reason();
    const char *reason_str = reset_reason_str(reason, &is_crash);

    char line[128];
    int n = snprintf(line, sizeof(line),
        "{\"t\":\"boot\",\"v\":%u,\"reason\":\"%s\",\"code\":%d,\"crash\":%s}\n",
        (unsigned)FG_VERSION, reason_str, (int)reason, is_crash ? "true" : "false");
    if (n < 0) return;
    size_t len = ((size_t)n < sizeof(line)) ? (size_t)n : sizeof(line) - 1u;
    fwrite(line, 1, len, stdout);
    fflush(stdout);
}

/* ── JSON emitter ─────────────────────────────────────────────────────────── */

static void emit_json(const fg_core_t *core, const fg_psram_t *psram, const fg_health_t *health,
                       const fg_task_t *top, uint32_t top_n)
{
    /* Assemble the whole line in a local buffer and write it with a SINGLE fwrite() so a
     * concurrent ESP_LOG can't splice into the middle of the JSON (each stdio call is
     * atomic under its own FILE lock). We deliberately do NOT hold flockfile across
     * multiple printf calls — that deadlocked the device at Wi-Fi connect (this task and
     * the proto-monitor task would hold the stdout FILE lock while the Wi-Fi driver
     * blocked on it, wedging the whole system with no panic — the task WDT is disabled).
     * See firmgen_proto_monitor.c for the full rationale. Bumped from 320 to 512 to fit
     * the optional top_tasks array (FG_TOP_TASKS_MAX entries) alongside psram/health. */
    char   line[512];
    size_t off = 0;
    int n = snprintf(line, sizeof(line),
           "{\"t\":\"rtos\",\"v\":%u,\"cpu\":%lu,\"tasks\":%lu,"
           "\"heap\":{\"free\":%lu,\"min\":%lu,\"total\":%lu},"
           "\"stack_peak\":%lu,\"uptime\":%llu",
           (unsigned)FG_VERSION,
           (unsigned long)core->cpu_pct,
           (unsigned long)core->tasks,
           (unsigned long)core->free_heap,
           (unsigned long)core->min_heap,
           (unsigned long)core->total_heap,
           (unsigned long)core->stack_pct,
           (unsigned long long)core->uptime_s);
    if (n < 0) return;
    off = ((size_t)n < sizeof(line)) ? (size_t)n : sizeof(line) - 1u;

    if (psram) {
        int m = snprintf(line + off, sizeof(line) - off,
               ",\"psram\":{\"present\":%s,\"free\":%lu,\"total\":%lu}",
               psram->present ? "true" : "false",
               (unsigned long)psram->free,
               (unsigned long)psram->total);
        if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
    }

    if (health && health->valid) {
        int m = snprintf(line + off, sizeof(line) - off,
               ",\"health\":{\"temp_c\":%.1f}", (double)health->temp_c);
        if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
    }

    if (top && top_n > 0u) {
        int m = snprintf(line + off, sizeof(line) - off, ",\"top_tasks\":[");
        if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
        for (uint32_t i = 0u; i < top_n; i++) {
            m = snprintf(line + off, sizeof(line) - off,
                   "%s{\"name\":\"%s\",\"cpu\":%lu,\"stack_min\":%lu}",
                   (i == 0u) ? "" : ",",
                   top[i].name,
                   (unsigned long)top[i].cpu_pct,
                   (unsigned long)top[i].stack_min);
            if (m > 0) off += ((size_t)m < sizeof(line) - off) ? (size_t)m : sizeof(line) - off - 1u;
        }
        if (off < sizeof(line) - 1u) line[off++] = ']';
    }

    if (off < sizeof(line) - 2u) { line[off++] = '}'; line[off++] = '\n'; }
    fwrite(line, 1, off, stdout);
    fflush(stdout);
}

/* ── Telemetry task ───────────────────────────────────────────────────────── */

static TaskHandle_t s_handle = NULL;

static void telemetry_task(void *arg)
{
    (void)arg;
    sample_tasks(NULL, 0u, NULL); /* prime delta counters on first sample */
    emit_boot_event();            /* report why the PREVIOUS boot ended, once */

    for (;;) {
        UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);

        fg_task_t top[FG_TOP_TASKS_MAX];
        uint32_t  top_n = 0u;

        fg_core_t   core;   core_sample(&core, hwm, top, FG_TOP_TASKS_ENABLE ? FG_TOP_TASKS_MAX : 0u, &top_n);
        fg_psram_t  psram;  psram_sample(&psram);
        fg_health_t health; health_sample(&health);

        emit_json(&core, &psram, &health, top, top_n);

        vTaskDelay(pdMS_TO_TICKS(FG_INTERVAL_MS));
    }
}

#endif /* FG_TELEMETRY_ENABLED */

/* ── Public API ───────────────────────────────────────────────────────────── */

void firmgen_telemetry_start(void)
{
    /* Configurable log level (Kconfig FG_TELEMETRY_LOG_LEVEL) — applied at runtime via
     * esp_log_level_set, independent of CONFIG_LOG_DEFAULT_LEVEL, so verbosity can be
     * changed without rebuilding every translation unit that calls ESP_LOGx. This governs
     * the APPLICATION's logging, not telemetry, so it applies in both profiles. */
    esp_log_level_set("*", (esp_log_level_t)CONFIG_FG_TELEMETRY_LOG_LEVEL);

#if FG_TELEMETRY_ENABLED
    if (s_handle != NULL) return; /* idempotent */

    BaseType_t ret = xTaskCreate(
        telemetry_task,
        "fg_telemetry",
        FG_STACK_BYTES,
        NULL,
        FG_PRIORITY,
        &s_handle
    );

    if (ret != pdPASS) {
        fprintf(stderr, "firmgen_telemetry: xTaskCreate failed\n");
    }
#else
    /* Production profile: nothing is sampled and nothing is emitted. Deliberately logged
     * at DEBUG level so it does not itself become serial noise on a shipping device. */
    ESP_LOGD("fg_telemetry",
             "telemetry disabled (production profile) — no data will be emitted");
#endif
}
