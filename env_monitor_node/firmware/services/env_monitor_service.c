/* Environmental monitor policy: smoothing, state updates, commands, and LED timing. */
#include "env_monitor_service.h"

#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "env_monitor_port.h"
#include "env_state.h"
#include "logger.h"
#include "osal.h"

static const char *TAG = "env_service";
static osal_mutex_t snapshot_mutex;
static env_snapshot_t latest_snapshot;
static bool alert_override_active;
static bool has_previous_state;
static env_state_t previous_state;
static uint16_t adc_history[APP_ADC_AVERAGE_COUNT];
static uint32_t adc_history_count;
static uint32_t adc_history_index;

/** Adds a sample to the ring and averages only entries that have been initialized. */
static uint16_t env_sensor_average(uint16_t sample)
{
    uint32_t sum = 0U;
    uint32_t index;

    adc_history[adc_history_index] = sample;
    adc_history_index = (adc_history_index + 1U) % APP_ADC_AVERAGE_COUNT;
    if (adc_history_count < APP_ADC_AVERAGE_COUNT)
    {
        adc_history_count++;
    }

    for (index = 0U; index < adc_history_count; index++)
    {
        sum += adc_history[index];
    }

    return (uint16_t)(sum / adc_history_count);
}

/** Converts raw ADC codes into explicitly synthetic demonstration metrics. */
static void env_sensor_convert(uint16_t adc_raw, float *temp_c, uint16_t *esi)
{
    *esi = (uint16_t)(((uint32_t)adc_raw * APP_ESI_MAX) / APP_ADC_RAW_MAX);
    *temp_c = APP_TEMP_BASE_C + (((float)adc_raw * APP_TEMP_DELTA_C) / (float)APP_ADC_RAW_MAX);
}

/** Creates and publishes a coherent snapshot, then reports any state transition. */
static bool env_snapshot_update(uint16_t adc_raw, bool button_is_pressed, uint32_t uptime_ms)
{
    env_snapshot_t snapshot = {0};
    bool state_changed;
    bool had_previous_state;
    env_state_t old_state = STATE_NORMAL;
    char state_line[80];

    if (!osal_mutex_lock(snapshot_mutex, 50U))
    {
        return false;
    }

    snapshot.adc_raw = adc_raw;
    snapshot.button_is_pressed = button_is_pressed;
    snapshot.uptime_ms = uptime_ms;
    env_sensor_convert(adc_raw, &snapshot.temp_c, &snapshot.esi);
    if (button_is_pressed || alert_override_active)
    {
        snapshot.temp_c = APP_TEMP_CRITICAL_SPIKE_C;
        snapshot.esi = APP_ESI_CRITICAL_SPIKE;
    }
    snapshot.state = evaluate_env_state(snapshot.temp_c, snapshot.esi,
                                        button_is_pressed || alert_override_active);

    had_previous_state = has_previous_state;
    state_changed = !had_previous_state || (snapshot.state != previous_state);
    if (had_previous_state)
    {
        old_state = previous_state;
    }

    latest_snapshot = snapshot;
    previous_state = snapshot.state;
    has_previous_state = true;
    osal_mutex_unlock(snapshot_mutex);

    if (state_changed)
    {
        (void)snprintf(state_line, sizeof(state_line), "[STATE_CHANGE] %s -> %s\n",
                       had_previous_state ? env_state_name(old_state) : "UNKNOWN",
                       env_state_name(snapshot.state));
        env_platform_uart_write(state_line);
    }

    return true;
}

/** Copies the latest shared snapshot while holding its mutex for a bounded time. */
static bool env_snapshot_read(env_snapshot_t *snapshot)
{
    if ((snapshot == NULL) || !osal_mutex_lock(snapshot_mutex, 50U))
    {
        return false;
    }

    *snapshot = latest_snapshot;
    osal_mutex_unlock(snapshot_mutex);
    return true;
}

/** Samples ADC1 once per second, smooths the samples, and emits the sensor record. */
static void env_sensor_task(void *argument)
{
    uint16_t adc_sample = 0U;
    bool button_is_pressed = false;
    char sensor_line[160];
    env_snapshot_t snapshot;
    (void)argument;

    for (;;)
    {
        if (env_platform_read(&adc_sample) &&
            env_platform_button_read(&button_is_pressed))
        {
            uint16_t averaged_adc = env_sensor_average(adc_sample);
            if (env_snapshot_update(averaged_adc, button_is_pressed, osal_uptime_ms()) &&
                env_snapshot_read(&snapshot))
            {
                (void)snprintf(sensor_line, sizeof(sensor_line),
                               "[SENSOR] ADC_Raw: %u | Temp_Est: %.1f C | ESI: %u | Btn_Trigger: %u\n",
                               (unsigned int)snapshot.adc_raw, (double)snapshot.temp_c,
                               (unsigned int)snapshot.esi,
                               snapshot.button_is_pressed ? 1U : 0U);
                env_platform_uart_write(sensor_line);
            }
        }
        else
        {
            ESP_LOGE(TAG, "ADC or button read failed");
        }
        osal_delay_ms(APP_SAMPLE_PERIOD_MS);
    }
}

/** Polls the physical button independently from the one-second sample task. */
static void env_button_task(void *argument)
{
    bool previous_button_state = false;
    bool is_initialized = false;
    bool button_is_pressed = false;
    (void)argument;

    for (;;)
    {
        if (env_platform_button_read(&button_is_pressed) &&
            (!is_initialized || (button_is_pressed != previous_button_state)))
        {
            env_snapshot_t snapshot;
            if (env_snapshot_read(&snapshot))
            {
                bool updated = env_snapshot_update(snapshot.adc_raw, button_is_pressed,
                                                   osal_uptime_ms());
                if (updated)
                {
                    (void)env_snapshot_read(&snapshot);
                }
            }
            previous_button_state = button_is_pressed;
            is_initialized = true;
        }
        osal_delay_ms(APP_BUTTON_POLL_PERIOD_MS);
    }
}

/** Applies a complete command and reports its status without logging input bytes. */
static void env_command_process(const char *command)
{
    env_snapshot_t snapshot;

    if (strcmp(command, "ALERT_ON") == 0)
    {
        if (osal_mutex_lock(snapshot_mutex, 50U))
        {
            alert_override_active = true;
            snapshot = latest_snapshot;
            osal_mutex_unlock(snapshot_mutex);
            if (env_snapshot_update(snapshot.adc_raw, snapshot.button_is_pressed,
                                    osal_uptime_ms()))
            {
                (void)env_snapshot_read(&snapshot);
            }
        }
        env_platform_uart_write("[CMD] External Closed-Loop Alert Activated\n");
    }
    else if (strcmp(command, "ALERT_OFF") == 0)
    {
        if (osal_mutex_lock(snapshot_mutex, 50U))
        {
            alert_override_active = false;
            snapshot = latest_snapshot;
            osal_mutex_unlock(snapshot_mutex);
            if (env_snapshot_update(snapshot.adc_raw, snapshot.button_is_pressed,
                                    osal_uptime_ms()))
            {
                (void)env_snapshot_read(&snapshot);
            }
        }
        env_platform_uart_write("[CMD] External Closed-Loop Alert Cleared\n");
    }
    else if (strcmp(command, "STATUS") == 0)
    {
        if (env_snapshot_read(&snapshot))
        {
            ESP_LOGI(TAG, "STATUS state=%s adc=%u temp=%.1f esi=%u uptime_ms=%lu",
                     env_state_name(snapshot.state), (unsigned int)snapshot.adc_raw,
                     (double)snapshot.temp_c, (unsigned int)snapshot.esi,
                     (unsigned long)snapshot.uptime_ms);
        }
    }
    else if (command[0] != '\0')
    {
        ESP_LOGW(TAG, "Ignoring unsupported UART command");
    }
}

/** Polls UART without waiting and processes bounded newline-terminated commands. */
static void env_command_task(void *argument)
{
    uint8_t receive_buffer[APP_UART_RX_CHUNK_SIZE];
    char command_buffer[APP_UART_LINE_BUFFER_SIZE];
    uint32_t command_length = 0U;
    uint32_t index;
    uint32_t received;
    (void)argument;

    memset(command_buffer, 0, sizeof(command_buffer));
    for (;;)
    {
        received = env_platform_uart_read(receive_buffer, (uint32_t)sizeof(receive_buffer));
        for (index = 0U; index < received; index++)
        {
            char byte = (char)receive_buffer[index];
            if ((byte == '\n') || (byte == '\r'))
            {
                command_buffer[command_length] = '\0';
                env_command_process(command_buffer);
                command_length = 0U;
            }
            else if (command_length < ((uint32_t)sizeof(command_buffer) - 1U))
            {
                command_buffer[command_length] = byte;
                command_length++;
            }
            else
            {
                command_length = 0U;
                command_buffer[0] = '\0';
                ESP_LOGW(TAG, "UART command exceeded line buffer");
            }
        }
        osal_delay_ms(10U);
    }
}

/** Applies the state-selected LED cadence with a 20 ms scheduling quantum. */
static void led_indicator_task(void *argument)
{
    uint32_t phase_elapsed_ms = 0U;
    uint32_t on_period_ms;
    uint32_t off_period_ms;
    bool led_is_on = false;
    bool has_previous_led_state = false;
    env_state_t previous_led_state = STATE_NORMAL;
    env_snapshot_t snapshot;
    (void)argument;

    for (;;)
    {
        if (env_snapshot_read(&snapshot))
        {
            if (!has_previous_led_state || (snapshot.state != previous_led_state))
            {
                phase_elapsed_ms = 0U;
                led_is_on = true;
                previous_led_state = snapshot.state;
                has_previous_led_state = true;
            }

            if (snapshot.state == STATE_NORMAL)
            {
                on_period_ms = APP_LED_NORMAL_ON_MS;
                off_period_ms = APP_LED_NORMAL_OFF_MS;
            }
            else if (snapshot.state == STATE_WARNING)
            {
                on_period_ms = APP_LED_WARNING_ON_MS;
                off_period_ms = APP_LED_WARNING_OFF_MS;
            }
            else
            {
                on_period_ms = APP_LED_CRITICAL_ON_MS;
                off_period_ms = APP_LED_CRITICAL_OFF_MS;
            }

            phase_elapsed_ms += APP_LED_UPDATE_PERIOD_MS;
            if (led_is_on && (phase_elapsed_ms >= on_period_ms))
            {
                led_is_on = false;
                phase_elapsed_ms = 0U;
            }
            else if (!led_is_on && (phase_elapsed_ms >= off_period_ms))
            {
                led_is_on = true;
                phase_elapsed_ms = 0U;
            }
            env_platform_led_set(led_is_on);
        }
        osal_delay_ms(APP_LED_UPDATE_PERIOD_MS);
    }
}

/** Writes one valid JSON object from a coherent snapshot every sample period. */
static void env_telemetry_task(void *argument)
{
    char json_line[192];
    env_snapshot_t snapshot;
    (void)argument;

    for (;;)
    {
        if (env_snapshot_read(&snapshot))
        {
            (void)snprintf(json_line, sizeof(json_line),
                           "{\"node\":\"%s\",\"adc\":%u,\"temp_c\":%.1f,\"esi\":%u,\"state\":\"%s\",\"uptime_ms\":%lu}\n",
                           APP_DEVICE_NAME, (unsigned int)snapshot.adc_raw,
                           (double)snapshot.temp_c, (unsigned int)snapshot.esi,
                           env_state_name(snapshot.state), (unsigned long)snapshot.uptime_ms);
            env_platform_uart_write(json_line);
        }
        osal_delay_ms(APP_SAMPLE_PERIOD_MS);
    }
}

bool env_monitor_service_start(void)
{
    snapshot_mutex = osal_mutex_create();
    if ((snapshot_mutex == NULL) || !env_platform_init())
    {
        ESP_LOGE(TAG, "Monitor platform initialization failed");
        return false;
    }

    if (!env_snapshot_update(0U, false, osal_uptime_ms()))
    {
        ESP_LOGE(TAG, "Initial monitor snapshot could not be created");
        return false;
    }
    if (!env_platform_ble_start())
    {
        ESP_LOGE(TAG, "BLE advertising initialization failed");
    }

    if (osal_task_create(env_sensor_task, "env_sensor_task", 4096U, NULL, 5U) == NULL)
    {
        ESP_LOGE(TAG, "Sensor task creation failed");
        return false;
    }
    if (osal_task_create(env_button_task, "env_button_task", 3072U, NULL, 7U) == NULL)
    {
        ESP_LOGE(TAG, "Button task creation failed");
        return false;
    }
    if (osal_task_create(led_indicator_task, "led_indicator_task", 3072U, NULL, 6U) == NULL)
    {
        ESP_LOGE(TAG, "LED indicator task creation failed");
        return false;
    }
    if (osal_task_create(env_telemetry_task, "env_telemetry_task", 3072U, NULL, 4U) == NULL)
    {
        ESP_LOGE(TAG, "Telemetry task creation failed");
        return false;
    }
    if (osal_task_create(env_command_task, "env_command_task", 3072U, NULL, 4U) == NULL)
    {
        ESP_LOGE(TAG, "Command task creation failed");
        return false;
    }

    return true;
}
