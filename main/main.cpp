#include "nvs_flash.h"
#include "esp_log.h"

#include "AppAudio.hpp"
#include "AppDriver.hpp"
#include "AppRmaker.hpp"
#include "AppMem.hpp"
#include "AppWifi.hpp"

namespace
{
    app::AppAudio app_audio;
    app::AppDriver app_driver;
    app::AppRmaker app_rmaker;
    app::AppMem app_mem;
    app::AppWifi app_wifi;
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
    app_wifi.connect(); // credentials from main/secrets.h; non-fatal
    app_rmaker.init();
    app_audio.init([](bool crying)
                   { app_rmaker.update(crying); });

    app_mem.print();
    app_rmaker.start();
    app_driver.start();
    app_audio.start();
    app_mem.monitor();
}