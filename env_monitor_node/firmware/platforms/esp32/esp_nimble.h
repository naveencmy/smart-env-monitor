/* Minimal NimBLE advertising adapter header. */
#ifndef ESP_NIMBLE_H
#define ESP_NIMBLE_H

#include <stdbool.h>

/**
 * @brief Initializes NVS, starts NimBLE host processing, and advertises the configured name.
 * @param[in] device_name Null-terminated local BLE name retained by the BLE stack.
 * @return True if initialization succeeds and host startup is scheduled.
 */
bool esp_nimble_start(const char *device_name);

#endif /* ESP_NIMBLE_H */
