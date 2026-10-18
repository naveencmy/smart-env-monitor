/* Board facts and pure helpers for the ESP32-WROOM-32 carrier connections. */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

#define BOARD_LED_ACTIVE_LEVEL 1
#define BOARD_BUTTON_ACTIVE_LEVEL 0
#define BOARD_NAME "esp32-wroom-32 carrier"

/**
 * @brief Returns the configured board identity string.
 * @return A static board name that remains valid for the program lifetime.
 */
const char *board_name(void);

/**
 * @brief Converts a logical LED request into its carrier GPIO level.
 * @param[in] is_on True requests the user-confirmed active-high LED state.
 * @return Raw output level to write to the carrier LED GPIO.
 */
static inline uint32_t board_led_level(bool is_on)
{
    return is_on ? (uint32_t)BOARD_LED_ACTIVE_LEVEL
                 : (uint32_t)(!BOARD_LED_ACTIVE_LEVEL);
}

#endif /* BOARD_H */
