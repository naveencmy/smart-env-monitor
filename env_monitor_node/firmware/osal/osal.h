/* Portable RTOS services contract implemented by the ESP32 platform adapter. */
#ifndef OSAL_H
#define OSAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *osal_task_t;
typedef void *osal_mutex_t;
typedef void (*osal_task_fn_t)(void *argument);

/**
 * @brief Delays the calling task for a bounded duration.
 * @param[in] duration_ms Delay duration in milliseconds; zero returns immediately.
 */
void osal_delay_ms(uint32_t duration_ms);

/**
 * @brief Reads monotonic elapsed time since boot.
 * @return Monotonic uptime in milliseconds, saturated to uint32_t range.
 */
uint32_t osal_uptime_ms(void);

/**
 * @brief Creates a task with the requested stack and scheduling priority.
 * @param[in] task_entry Task function that runs until it deletes or blocks itself.
 * @param[in] task_name Static name used for RTOS diagnostics.
 * @param[in] stack_bytes Requested task stack capacity in bytes.
 * @param[in] argument Borrowed task argument passed to task_entry.
 * @param[in] priority RTOS task priority.
 * @return Opaque task handle, or NULL if creation fails.
 */
osal_task_t osal_task_create(osal_task_fn_t task_entry,
                             const char *task_name,
                             size_t stack_bytes,
                             void *argument,
                             uint8_t priority);

/**
 * @brief Creates an unlocked mutex for sharing state among tasks.
 * @return Opaque mutex handle, or NULL if allocation fails.
 */
osal_mutex_t osal_mutex_create(void);

/**
 * @brief Attempts to acquire a mutex within a finite timeout.
 * @param[in] mutex Mutex handle returned by osal_mutex_create.
 * @param[in] timeout_ms Maximum wait time in milliseconds.
 * @return True if acquired; false for timeout or invalid handle.
 */
bool osal_mutex_lock(osal_mutex_t mutex, uint32_t timeout_ms);

/**
 * @brief Releases a mutex previously acquired by the calling task.
 * @param[in] mutex Mutex handle returned by osal_mutex_create.
 */
void osal_mutex_unlock(osal_mutex_t mutex);

#endif /* OSAL_H */
