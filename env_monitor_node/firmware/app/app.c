/* Application startup for the environmental monitor service. */
#include "app.h"

#include "env_monitor_service.h"
#include "logger.h"

static const char *TAG = "app";

void app_start(void)
{
    ESP_LOGI(TAG, "starting environmental monitor");
    if (!env_monitor_service_start())
    {
        ESP_LOGE(TAG, "environmental monitor startup failed");
    }
}
