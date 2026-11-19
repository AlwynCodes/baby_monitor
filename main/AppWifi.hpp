#pragma once

#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "secrets.h"

namespace app
{
    // Direct Wi-Fi STA connection using credentials from main/secrets.h.
    // Replaces the SoftAP provisioning flow of rmaker_app_network.
    class AppWifi
    {
    private:
        constexpr static const char *TAG{"wifi"};
        constexpr static int CONNECTED_BIT{ BIT0 };

        EventGroupHandle_t m_events{nullptr};
        bool m_connected{false};

        static void onEvent(void *arg, esp_event_base_t base, int32_t id, void *data)
        {
            AppWifi *self{static_cast<AppWifi *>(arg)};

            if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
            {
                esp_wifi_connect();
            }
            else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
            {
                const auto *ev = static_cast<wifi_event_sta_disconnected_t *>(data);
                // Common reasons: 15 = 4WAY_HANDSHAKE_TIMEOUT (wrong password),
                // 201 = NO_AP_FOUND (wrong SSID or 5 GHz-only AP),
                // 204 = HANDSHAKE_TIMEOUT, 8 = ASSOC_LEAVE.
                ESP_LOGW(TAG, "disconnected (reason %d), retrying...", ev->reason);
                if (!(xEventGroupGetBits(self->m_events) & CONNECTED_BIT))
                {
                    esp_wifi_connect();
                }
            }
            else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
            {
                const auto *ip = static_cast<ip_event_got_ip_t *>(data);
                ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&ip->ip_info.ip));
                xEventGroupSetBits(self->m_events, CONNECTED_BIT);
            }
        }

    public:
        // Blocks until connected or timeout. Returns false on failure; the
        // caller keeps running so audio still works without a network.
        bool connect(int timeout_ms = 20000)
        {
            if (strlen(WIFI_SSID) == 0)
            {
                ESP_LOGW(TAG, "main/secrets.h is empty - skipping Wi-Fi");
                return false;
            }

            m_events = xEventGroupCreate();

            ESP_ERROR_CHECK(esp_netif_init());
            ESP_ERROR_CHECK(esp_event_loop_create_default());
            esp_netif_create_default_wifi_sta();

            wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
            ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

            ESP_ERROR_CHECK(esp_event_handler_instance_register(
                WIFI_EVENT, ESP_EVENT_ANY_ID, &onEvent, this, nullptr));
            ESP_ERROR_CHECK(esp_event_handler_instance_register(
                IP_EVENT, IP_EVENT_STA_GOT_IP, &onEvent, this, nullptr));

            wifi_config_t cfg = {};
            strncpy(reinterpret_cast<char *>(cfg.sta.ssid), WIFI_SSID, sizeof(cfg.sta.ssid) - 1);
            strncpy(reinterpret_cast<char *>(cfg.sta.password), WIFI_PASSWORD, sizeof(cfg.sta.password) - 1);
            cfg.sta.threshold.authmode = strlen(WIFI_PASSWORD) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

            ESP_LOGI(TAG, "connecting to \"%s\"", WIFI_SSID);
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
            ESP_ERROR_CHECK(esp_wifi_start());

            const EventBits_t bits = xEventGroupWaitBits(
                m_events, CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));

            m_connected = (bits & CONNECTED_BIT) != 0;
            if (!m_connected)
            {
                ESP_LOGE(TAG, "could not connect to \"%s\" within %d ms", WIFI_SSID, timeout_ms);
            }
            return m_connected;
        }
    };
}
