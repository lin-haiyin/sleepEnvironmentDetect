#include "network_manager.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <cstring>

namespace network_manager {
namespace {
constexpr char TAG[] = "network";
esp_netif_t *s_netif = nullptr;
Config s_config{};
bool s_started = false;
bool s_connected = false;
bool s_mqtt_started = false;

void event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi station started; connecting to configured SSID");
        esp_wifi_connect();
        return;
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_mqtt_started) {
            mqtt_transport::stop();
            s_mqtt_started = false;
        }
        const auto *disconnected = static_cast<const wifi_event_sta_disconnected_t *>(event_data);
        ESP_LOGW(TAG, "Wi-Fi disconnected (reason=%u); retrying",
                 disconnected != nullptr ? static_cast<unsigned>(disconnected->reason) : 0U);
        esp_wifi_connect();
        return;
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        const auto *got_ip = static_cast<const ip_event_got_ip_t *>(event_data);
        if (got_ip != nullptr) {
            ESP_LOGI(TAG, "Wi-Fi got IP: " IPSTR, IP2STR(&got_ip->ip_info.ip));
        } else {
            ESP_LOGI(TAG, "Wi-Fi got IP");
        }
        if (!s_mqtt_started) {
            const esp_err_t result = mqtt_transport::start(s_config.mqtt);
            if (result == ESP_OK) {
                s_mqtt_started = true;
                ESP_LOGI(TAG, "MQTT client start requested");
            } else {
                ESP_LOGE(TAG, "MQTT start failed: %s", esp_err_to_name(result));
            }
        }
    }
}
} // namespace

esp_err_t start(const Config &config) {
    if (s_started || config.wifi_ssid == nullptr || config.wifi_password == nullptr ||
        config.mqtt.broker_uri == nullptr || config.mqtt.device_id == nullptr) {
        return s_started ? ESP_ERR_INVALID_STATE : ESP_ERR_INVALID_ARG;
    }
    s_config = config;

    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return result;
    }
    if (result != ESP_OK) return result;
    result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == nullptr) return ESP_ERR_NO_MEM;

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&wifi_init);
    if (result != ESP_OK) return result;
    result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr);
    if (result != ESP_OK) return result;
    result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr);
    if (result != ESP_OK) return result;

    wifi_config_t wifi_config{};
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), config.wifi_ssid,
                 sizeof(wifi_config.sta.ssid) - 1U);
    std::strncpy(reinterpret_cast<char *>(wifi_config.sta.password), config.wifi_password,
                 sizeof(wifi_config.sta.password) - 1U);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result == ESP_OK) result = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (result == ESP_OK) result = esp_wifi_start();
    if (result != ESP_OK) return result;
    s_started = true;
    ESP_LOGI(TAG, "Wi-Fi configured (SSID length=%u); waiting for IP", 
             static_cast<unsigned>(std::strlen(config.wifi_ssid)));
    return ESP_OK;
}

esp_err_t stop() {
    if (!s_started) return ESP_ERR_INVALID_STATE;
    if (s_mqtt_started) {
        mqtt_transport::stop();
        s_mqtt_started = false;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    s_connected = false;
    s_started = false;
    return ESP_OK;
}

bool is_connected() { return s_connected; }

} // namespace network_manager
