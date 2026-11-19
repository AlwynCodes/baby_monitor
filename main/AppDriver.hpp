#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

namespace app
{
    // Platform bring-up that must happen before RainMaker: NVS.
    //
    // Wi-Fi/Thread provisioning is handled by the `rmaker_app_network`
    // component (app_network_init / app_network_start), which app_main() calls
    // directly. The hand-written components/app_wifi that used to provide
    // app_wifi_init/app_wifi_start has been removed: rmaker_app_network
    // supersedes it and both cannot be linked at once.
    class AppDriver
    {
    private:
        constexpr static const char *TAG{"app-driver"};

    public:
        void init()
        {
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                ESP_LOGW(TAG, "NVS unusable (%s), erasing and re-initialising", esp_err_to_name(err));
                ESP_ERROR_CHECK(nvs_flash_erase());
                err = nvs_flash_init();
            }
            ESP_ERROR_CHECK(err);
        }

        void start()
        {
            // Nothing to do: provisioning is started by app_network_start()
            // at the end of app_main().
        }
    };
}
