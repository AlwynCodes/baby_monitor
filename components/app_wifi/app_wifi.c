#include "app_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include <esp_rmaker_utils.h>

static const char *TAG = "app_wifi";

void app_wifi_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    ESP_LOGI(TAG, "app_wifi_init done");
}

void app_wifi_start(app_wifi_pop_type_t pop_type)
{
    ESP_LOGI(TAG, "app_wifi_start");
    esp_wifi_connect();
}
