// https://rainmaker.espressif.com/docs/standard-types.html

#pragma once

#include <cstring>
#include <cstdint>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_on_network_chal_resp.h>

namespace app
{
    class AppRmaker
    {
    private:
        constexpr static const char *TAG{"rmaker"};
        esp_rmaker_node_t *m_rmaker_node{nullptr};
        esp_rmaker_device_t *m_device{nullptr};
        esp_rmaker_param_t *m_cry_param{nullptr};

    public:
        void init()
        {
            esp_rmaker_time_set_timezone(CONFIG_ESP_RMAKER_DEF_TIMEZONE);
            esp_rmaker_config_t rainmaker_cfg = {
                .enable_time_sync = true,
            };
            m_rmaker_node = esp_rmaker_node_init(&rainmaker_cfg, "Baby node", "esp.node.sensor");
            // On a classic ESP32 the node cannot be created until the device is
            // claimed (node id / MQTT params come from the factory partition).
            // Everything below dereferences node handles, so bail out instead
            // of building a half-initialised device.
            if (m_rmaker_node == nullptr)
            {
                ESP_LOGE(TAG, "RainMaker node init failed - cloud reporting disabled until claimed");
                return;
            }

            m_device = esp_rmaker_device_create("Cry sensor", "esp.device.other", (void *)this);
            if (m_device == nullptr)
            {
                ESP_LOGE(TAG, "device create failed");
                return;
            }
            esp_rmaker_device_add_param(m_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "Cry sensor"));

            m_cry_param = esp_rmaker_param_create("Baby crying", "esp.param.toggle", esp_rmaker_bool(false), PROP_FLAG_READ);
            if (m_cry_param == nullptr)
            {
                ESP_LOGE(TAG, "param create failed");
                return;
            }
            esp_rmaker_param_add_ui_type(m_cry_param, ESP_RMAKER_UI_TOGGLE);
            esp_rmaker_device_add_param(m_device, m_cry_param);
            esp_rmaker_device_assign_primary_param(m_device, m_cry_param);

            esp_rmaker_node_add_device(m_rmaker_node, m_device);
        }
        void start()
        {
            if (m_rmaker_node == nullptr)
            {
                ESP_LOGW(TAG, "start() skipped: RainMaker not initialised");
                return;
            }
            esp_rmaker_start();
        }

        // Starts the on-network challenge-response service. With
        // CONFIG_ESP_RMAKER_ENABLE_CHALLENGE_RESPONSE=y this replaces the
        // traditional user-node mapping flow: the RainMaker app (or CLI) finds
        // the already-claimed node on the LAN via mDNS
        // (_esp_rmaker_chal_resp._tcp) and maps it to the logged-in user
        // account without re-claiming and without touching the fctry partition.
        //
        // Must be called AFTER esp_rmaker_node_init() (the service advertises
        // the node id and needs it for mDNS) and BEFORE esp_rmaker_start().
        void startOnNetworkChalResp()
        {
            if (m_rmaker_node == nullptr)
            {
                ESP_LOGW(TAG, "on-network chal-resp skipped: RainMaker not initialised");
                return;
            }

            esp_rmaker_on_network_chal_resp_config_t cfg =
                ESP_RMAKER_ON_NETWORK_CHAL_RESP_DEFAULT_CONFIG();
            cfg.enable_mdns = true; // required for app/CLI discovery

            esp_err_t err = esp_rmaker_on_network_chal_resp_start(&cfg);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "on-network challenge-response started (port %u, sec %d, mdns)",
                         (unsigned)cfg.port, (int)cfg.sec_ver);
            }
            else if (err == ESP_ERR_INVALID_STATE)
            {
                ESP_LOGW(TAG, "on-network challenge-response already running");
            }
            else
            {
                ESP_LOGE(TAG, "on-network challenge-response failed: %s", esp_err_to_name(err));
            }
        }

        void update(bool state)
        {
            if (m_cry_param == nullptr)
            {
                // Called from the audio task; must not dereference a NULL param.
                ESP_LOGW(TAG, "crying=%d (RainMaker not initialised, not reported)", (int)state);
                return;
            }
            esp_rmaker_param_update_and_report(m_cry_param, esp_rmaker_bool(state));
            if (state)
            {
                esp_rmaker_raise_alert("Baby Monitor Alert: Your baby is crying!");
            }
        }
    };

}
