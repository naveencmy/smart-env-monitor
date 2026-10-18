/* NimBLE host initialization and advertising, isolated from unrelated ESP-IDF drivers. */
#include "esp_nimble.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "esp_nimble";
static const char *ble_device_name;
static bool ble_initialized;

/** Configures the discoverable beacon after NimBLE chooses its identity address. */
static void esp_nimble_advertise(void)
{
    struct ble_hs_adv_fields fields;
    struct ble_gap_adv_params parameters;
    uint8_t own_address_type;
    int result = ble_hs_id_infer_auto(0, &own_address_type);

    if (result != 0)
    {
        ESP_LOGE(TAG, "BLE address selection failed: %d", result);
        return;
    }

    memset(&fields, 0, sizeof(fields));
    fields.flags = (uint8_t)(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    fields.name = (uint8_t *)ble_device_name;
    fields.name_len = (uint8_t)strlen(ble_device_name);
    fields.name_is_complete = 1U;
    result = ble_gap_adv_set_fields(&fields);
    if (result != 0)
    {
        ESP_LOGE(TAG, "BLE advertisement payload failed: %d", result);
        return;
    }

    memset(&parameters, 0, sizeof(parameters));
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(own_address_type, NULL, BLE_HS_FOREVER,
                               &parameters, NULL, NULL);
    if (result != 0)
    {
        ESP_LOGE(TAG, "BLE advertising start failed: %d", result);
    }
    else
    {
        ESP_LOGI(TAG, "BLE advertising as %s", ble_device_name);
    }
}

/** Starts advertising when the NimBLE host reports identity synchronization complete. */
static void esp_nimble_on_sync(void)
{
    esp_nimble_advertise();
}

/** Owns the NimBLE host event loop until the stack is stopped. */
static void esp_nimble_host_task(void *argument)
{
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

bool esp_nimble_start(const char *device_name)
{
    esp_err_t result;

    if ((device_name == NULL) || (device_name[0] == '\0'))
    {
        return false;
    }
    if (ble_initialized)
    {
        return true;
    }

    ble_device_name = device_name;
    result = nvs_flash_init();
    if ((result == ESP_ERR_NVS_NO_FREE_PAGES) || (result == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        result = nvs_flash_erase();
        if (result == ESP_OK)
        {
            result = nvs_flash_init();
        }
    }
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(result));
        return false;
    }

    result = nimble_port_init();
    if (result != ESP_OK)
    {
        ESP_LOGE(TAG, "NimBLE initialization failed: %s", esp_err_to_name(result));
        return false;
    }

    ble_hs_cfg.sync_cb = esp_nimble_on_sync;
    result = ble_svc_gap_device_name_set(device_name);
    if (result != 0)
    {
        ESP_LOGE(TAG, "BLE name setup failed: %d", result);
        return false;
    }

    nimble_port_freertos_init(esp_nimble_host_task);
    ble_initialized = true;
    return true;
}
