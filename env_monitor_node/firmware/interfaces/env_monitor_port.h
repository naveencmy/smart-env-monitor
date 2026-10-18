/* Vendor-neutral environmental monitor platform operations contract. */
#ifndef ENV_MONITOR_PORT_H
#define ENV_MONITOR_PORT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes ADC1, carrier GPIO, and UART0 hardware interfaces.
 * @return True if all required platform interfaces initialize successfully.
 */
bool env_platform_init(void);

/**
 * @brief Reads one raw ADC1 sample.
 * @param[out] adc_raw Destination for a 12-bit ADC code from 0 through 4095.
 * @return True if the sample is read successfully; false on I/O failure or null output.
 */
bool env_platform_read(uint16_t *adc_raw);

/**
 * @brief Reads the active-low carrier button without waiting.
 * @param[out] button_is_pressed Destination for the current logical button state.
 * @return True if the button was read; false for an invalid destination.
 */
bool env_platform_button_read(bool *button_is_pressed);

/**
 * @brief Drives the carrier LED using its declared board active level.
 * @param[in] is_on True requests the LED-on level; false requests LED-off.
 */
void env_platform_led_set(bool is_on);

/**
 * @brief Reads any UART command bytes available at the time of the call.
 * @param[out] buffer Writable destination for received bytes.
 * @param[in] capacity Maximum number of bytes writable into buffer.
 * @return Number of bytes received, from zero through capacity.
 */
uint32_t env_platform_uart_read(uint8_t *buffer, uint32_t capacity);

/**
 * @brief Sends a NUL-terminated diagnostic or telemetry line over UART0.
 * @param[in] line Text to write; borrowed only for the duration of this call.
 */
void env_platform_uart_write(const char *line);

/**
 * @brief Starts NimBLE advertising with the product device name.
 * @return True if NVS, NimBLE host initialization, and host task startup succeed.
 */
bool env_platform_ble_start(void);

#endif /* ENV_MONITOR_PORT_H */
