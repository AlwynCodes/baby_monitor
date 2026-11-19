#pragma once

#include <cstring>

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_random.h>
#include <esp_mac.h>

#include <esp_rmaker_factory.h>

#include "sdkconfig.h"

namespace app
{
    // Platform bring-up that must run before RainMaker:
    //   1. NVS
    //   2. RainMaker factory NVS partition ("fctry")
    //   3. seed the provisioning PoP random bytes on a freshly flashed device
    //
    // Wi-Fi/Thread provisioning itself is handled by rmaker_app_network
    // (app_network_init / app_network_start), which app_main() calls directly.
    class AppDriver
    {
    private:
        constexpr static const char *TAG{"app-driver"};
        bool m_ready{false};

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

            // Initialises the factory NVS partition named by
            // CONFIG_ESP_RMAKER_FACTORY_PARTITION_NAME ("fctry"). Without this,
            // esp_rmaker_node_init() fails with "Failed to initialise storage".
            err = esp_rmaker_factory_init();
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "factory partition init failed: %s", esp_err_to_name(err));
                return;
            }

            // rmaker_app_network derives the PoP for POP_TYPE_RANDOM from 4
            // random bytes stored in the factory partition. Factory data is
            // normally written at manufacture time; on a freshly flashed board
            // the partition is empty, so app_network_start() fails with
            //   "Failed to read random bytes from NVS to generate PoP."
            //   "ESP_ERROR_CHECK failed: ESP_ERR_NO_MEM ... app_network_start()"
            // Seed those bytes once so first boot works.
            seed_pop_random();

            // esp_rmaker_node_create() requires a node id. RainMaker generates
            // one from the base MAC in esp_rmaker_populate_node_id(), but that
            // code is inside "#ifdef ESP_RMAKER_CLAIM_ENABLED", which is only
            // defined for SELF_CLAIM (unavailable on the classic ESP32:
            // "depends on !IDF_TARGET_ESP32") or ASSISTED_CLAIM (needs BT).
            // Without it, node creation fails with
            //   "Failed to initialise Node Id. Please perform claiming..."
            //   "Node or Device/Service handle cannot be NULL."
            // Seed the same MAC-derived id RainMaker would have generated.
            seed_node_id();

            m_ready = true;
        }

        void start()
        {
            // Nothing to do: provisioning is started by app_network_start()
            // at the end of app_main().
        }

    private:
        void seed_node_id()
        {
            // 12 uppercase hex chars, identical in form to what
            // esp_rmaker_populate_node_id() produces from the base MAC.
            //
            // Only ever seed when the key is completely ABSENT. A claimed
            // device already holds its cloud-assigned node id here, and
            // overwriting it would invalidate the claiming.
            size_t len = esp_rmaker_factory_get_size("node_id");
            if (len > 0)
            {
                ESP_LOGI(TAG, "node_id already present, leaving it untouched");
                return;
            }

            uint8_t mac[6];
            if (esp_read_mac(mac, ESP_MAC_BASE) != ESP_OK)
            {
                ESP_LOGE(TAG, "could not read base MAC to derive node_id");
                return;
            }

            char buf[13];
            snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            esp_err_t err = esp_rmaker_factory_set("node_id", buf, 12);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "seeded node_id %s", buf);
            }
            else
            {
                ESP_LOGE(TAG, "failed to seed node_id: %s", esp_err_to_name(err));
            }
        }

        void seed_pop_random()
        {
            const char *key = CONFIG_APP_NETWORK_PROV_RANDOM_NVS_KEY;

            // Absent-only, for the same reason as node_id: never disturb
            // factory data that claiming may have written.
            size_t len = esp_rmaker_factory_get_size(key);
            if (len > 0)
            {
                ESP_LOGI(TAG, "PoP random bytes already present (%u bytes), leaving them", (unsigned)len);
                return;
            }

            uint8_t buf[4];
            esp_fill_random(buf, sizeof(buf));

            esp_err_t err = esp_rmaker_factory_set((char *)key, buf, sizeof(buf));
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "seeded PoP random bytes in factory partition");
            }
            else
            {
                ESP_LOGE(TAG, "failed to seed PoP random bytes: %s", esp_err_to_name(err));
            }
        }
    };
}
