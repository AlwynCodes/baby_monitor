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
    constexpr const char *TAG{"app_main"};

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_mem.print();

    // 1. NVS + RainMaker factory partition ("fctry").
    app_driver.init();

    // 2. Wi-Fi FIRST. RainMaker reads the base MAC to derive the node id and
    //    builds its MQTT credentials at node-init time, so esp_wifi_init() and
    //    a DHCP lease must already exist. Initialising RainMaker before this
    //    produced:
    //      "Could not fetch MAC address. Please initialise Wi-Fi first"
    //      "username received is NULL" / "Failed to initialise MQTT"
    const bool online = app_wifi.connect();

    // 3. RainMaker node, now that the network stack is up.
    if (online)
    {
        app_rmaker.init();
    }
    else
    {
        ESP_LOGW(TAG, "no Wi-Fi link - skipping RainMaker init (audio still runs)");
    }

    // 4. Audio runs regardless of the cloud link.
    app_audio.init([](bool crying)
                   { app_rmaker.update(crying); });

    app_mem.print();

    // 5. Start the RainMaker agent only after a successful connection; it
    //    spawns the MQTT client and reports node config.
    if (online)
    {
        app_rmaker.start();
    }
    app_driver.start();
    app_audio.start();
    app_mem.monitor();
}
