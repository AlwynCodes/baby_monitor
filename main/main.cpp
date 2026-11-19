#include "nvs_flash.h"
#include "esp_log.h"
#include "app_network.h"

#include "AppAudio.hpp"
#include "AppDriver.hpp"
#include "AppRmaker.hpp"
#include "AppMem.hpp"

namespace
{
    app::AppAudio app_audio;
    app::AppDriver app_driver;
    app::AppRmaker app_rmaker;
    app::AppMem app_mem;
}

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_mem.print();
    app_driver.init();
    app_network_init();
    app_rmaker.init();
    app_audio.init([](bool crying)
                   { app_rmaker.update(crying); });

    app_mem.print();
    app_rmaker.start();
    app_driver.start();
    app_audio.start();
    app_mem.monitor();

    // Not ESP_ERROR_CHECK()'d on purpose: abort()ing here turns any
    // provisioning/claiming failure into a reboot loop that also kills the
    // audio pipeline. Log it and keep the device running.
    esp_err_t prov_err = app_network_start(POP_TYPE_RANDOM); // blocks until Wi-Fi is provisioned
    if (prov_err != ESP_OK)
    {
        ESP_LOGE("app_main", "app_network_start failed: %s (device keeps running without cloud link)",
                 esp_err_to_name(prov_err));
    }
}