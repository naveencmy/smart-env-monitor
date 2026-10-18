/* ESP-IDF adapter for ADC1, carrier GPIO, UART0, and BLE advertising startup. */
#include "env_monitor_port.h"

#include <limits.h>
#include <string.h>

#include "app_config.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_nimble.h"

static const char *TAG = "env_platform";
static adc_oneshot_unit_handle_t adc1_handle;

/** Selects ADC1 channel 6, the documented ESP32 mapping for GPIO34. */
static adc_channel_t env_adc_channel(void)
{
    return ADC_CHANNEL_6;
}

bool env_platform_init(void)
{
    adc_oneshot_unit_init_cfg_t adc_unit_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_chan_cfg_t adc_channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = (adc_bitwidth_t)APP_ADC_SAMPLE_BITWIDTH,
    };
    uart_config_t uart_config = {
        .baud_rate = APP_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t result;

    result = adc_oneshot_new_unit(&adc_unit_config, &adc1_handle);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC1 initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    result = adc_oneshot_config_channel(adc1_handle, env_adc_channel(), &adc_channel_config);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC1 channel setup failed: %s", esp_err_to_name(result));
        return false;
    }

    result = gpio_reset_pin((gpio_num_t)APP_LED_GPIO);
    if (result == ESP_OK)
    {
        result = gpio_set_direction((gpio_num_t)APP_LED_GPIO, GPIO_MODE_OUTPUT);
    }
    if (result == ESP_OK)
    {
        result = gpio_set_level((gpio_num_t)APP_LED_GPIO, board_led_level(false));
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "LED GPIO setup failed: %s", esp_err_to_name(result));
        return false;
    }

    result = gpio_reset_pin((gpio_num_t)APP_BUTTON_GPIO);
    if (result == ESP_OK)
    {
        result = gpio_set_direction((gpio_num_t)APP_BUTTON_GPIO, GPIO_MODE_INPUT);
    }
    if (result == ESP_OK)
    {
        result = gpio_set_pull_mode((gpio_num_t)APP_BUTTON_GPIO, GPIO_PULLUP_ONLY);
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "button GPIO setup failed: %s", esp_err_to_name(result));
        return false;
    }

    result = uart_driver_install((uart_port_t)APP_UART_PORT, APP_UART_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if ((result != ESP_OK) && (result != ESP_ERR_INVALID_STATE))
    {
        ESP_LOGE(TAG, "UART0 driver install failed: %s", esp_err_to_name(result));
        return false;
    }
    result = uart_param_config((uart_port_t)APP_UART_PORT, &uart_config);
    if (result == ESP_OK)
    {
        result = uart_set_pin((uart_port_t)APP_UART_PORT, APP_UART_TX_GPIO, APP_UART_RX_GPIO,
                              UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "UART0 configuration failed: %s", esp_err_to_name(result));
        return false;
    }

    ESP_LOGI(TAG, "ADC1 GPIO34, button GPIO0, LED GPIO2, UART0 initialized");
    return true;
}

bool env_platform_read(uint16_t *adc_raw)
{
    int raw_value = 0;
    esp_err_t result;

    if (adc_raw == NULL)
    {
        return false;
    }

    result = adc_oneshot_read(adc1_handle, env_adc_channel(), &raw_value);
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(result));
        return false;
    }

    *adc_raw = (uint16_t)raw_value;
    return true;
}

bool env_platform_button_read(bool *button_is_pressed)
{
    if (button_is_pressed == NULL)
    {
        return false;
    }

    *button_is_pressed = (gpio_get_level((gpio_num_t)APP_BUTTON_GPIO) == BOARD_BUTTON_ACTIVE_LEVEL);
    return true;
}

void env_platform_led_set(bool is_on)
{
    esp_err_t result = gpio_set_level((gpio_num_t)APP_LED_GPIO, board_led_level(is_on));
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "LED output failed: %s", esp_err_to_name(result));
    }
}

uint32_t env_platform_uart_read(uint8_t *buffer, uint32_t capacity)
{
    int bytes_read;

    if ((buffer == NULL) || (capacity == 0U))
    {
        return 0U;
    }

    bytes_read = uart_read_bytes((uart_port_t)APP_UART_PORT, buffer, capacity, 0U);
    return (bytes_read > 0) ? (uint32_t)bytes_read : 0U;
}

void env_platform_uart_write(const char *line)
{
    if (line != NULL)
    {
        size_t length = strlen(line);
        if (length <= (size_t)UINT32_MAX)
        {
            (void)uart_write_bytes((uart_port_t)APP_UART_PORT, line, (uint32_t)length);
        }
    }
}

bool env_platform_ble_start(void)
{
    bool result = esp_nimble_start(APP_DEVICE_NAME);
    if (!result)
    {
        ESP_LOGE(TAG, "BLE advertising initialization failed");
    }
    return result;
}
