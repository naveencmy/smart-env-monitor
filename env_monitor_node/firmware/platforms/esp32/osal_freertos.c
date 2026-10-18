/* ESP-IDF FreeRTOS implementation of the platform-neutral OSAL contract. */
#include "osal.h"
#include "esp32_osal.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

void osal_delay_ms(uint32_t duration_ms)
{
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
}

uint32_t osal_uptime_ms(void)
{
    int64_t uptime_us = esp_timer_get_time();
    uint64_t uptime_ms = (uint64_t)uptime_us / 1000U;
    return (uptime_ms > UINT32_MAX) ? UINT32_MAX : (uint32_t)uptime_ms;
}

osal_task_t osal_task_create(osal_task_fn_t task_entry,
                             const char *task_name,
                             size_t stack_bytes,
                             void *argument,
                             uint8_t priority)
{
    TaskHandle_t task_handle = NULL;
    BaseType_t result = xTaskCreate(task_entry, task_name,
                                    (configSTACK_DEPTH_TYPE)stack_bytes,
                                    argument, (UBaseType_t)priority, &task_handle);
    return (result == pdPASS) ? (osal_task_t)task_handle : NULL;
}

osal_mutex_t osal_mutex_create(void)
{
    return (osal_mutex_t)xSemaphoreCreateMutex();
}

bool osal_mutex_lock(osal_mutex_t mutex, uint32_t timeout_ms)
{
    if (mutex == NULL)
    {
        return false;
    }

    return xSemaphoreTake((SemaphoreHandle_t)mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void osal_mutex_unlock(osal_mutex_t mutex)
{
    if (mutex != NULL)
    {
        (void)xSemaphoreGive((SemaphoreHandle_t)mutex);
    }
}

void esp32_osal_register(void)
{
}
