#include "sensor_drivers.h"

#include "app_config.h"
#if __has_include("secrets.h")
#define ENV_MQTT_COMPILED 1
#include "network_manager.hpp"
#include "secrets.h"
#else
#define ENV_MQTT_COMPILED 0
#endif
#include "esp_flash.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cinttypes>

extern "C" void app_main() {
    uint32_t flash_size = 0;
    const esp_err_t flash_result = esp_flash_get_size(nullptr, &flash_size);

    ESP_LOGI("app", "============================================================");
    ESP_LOGI("app", "ENV-SENSING-S3 SENSOR BRING-UP FIRMWARE");
    ESP_LOGI("app", "built=%s %s idf=%s", __DATE__, __TIME__, esp_get_idf_version());
    ESP_LOGI("app", "reset_reason=%d free_heap=%" PRIu32 " bytes",
             static_cast<int>(esp_reset_reason()), esp_get_free_heap_size());
    if (flash_result == ESP_OK) {
        unsigned psram_mb = 0;
#if CONFIG_SPIRAM
        psram_mb = static_cast<unsigned>(esp_psram_get_size() / (1024U * 1024U));
#endif
        ESP_LOGI("app", "detected_flash=%" PRIu32 " MB detected_psram=%u MB",
                 flash_size / (1024U * 1024U), psram_mb);
    } else {
        ESP_LOGW("app", "flash size query failed: %s", esp_err_to_name(flash_result));
    }
    ESP_LOGI("app", "ACTIVE_TEST=%s", sensor_test_name());
    sensor_test_log_configuration();
    ESP_LOGI("app", "============================================================");

    ESP_LOGI("app", "initializing active application profile");
    const esp_err_t init_result = sensor_test_init();
    if (init_result != ESP_OK) {
        ESP_LOGE("app", "INIT FAILED: %s", esp_err_to_name(init_result));
        ESP_LOGE("app", "other connected modules are intentionally inactive");
    } else {
        ESP_LOGI("app", "INIT OK: %s; periodic readings start now", sensor_test_name());
        sensor_test_start_interactive_control();
#if ENV_MQTT_COMPILED
        network_manager::Config network_config{
            ENV_WIFI_SSID,
            ENV_WIFI_PASSWORD,
            {ENV_MQTT_URI, ENV_MQTT_DEVICE_ID, ENV_MQTT_USERNAME, ENV_MQTT_PASSWORD,
             ENV_MQTT_ROOT_CA_PEM},
        };
        const esp_err_t network_result = network_manager::start(network_config);
        if (network_result != ESP_OK) {
            ESP_LOGE("app", "NETWORK INIT FAILED: %s; local sensor/LED test continues",
                     esp_err_to_name(network_result));
        } else {
            ESP_LOGI("app", "NETWORK INIT OK: Wi-Fi connects asynchronously; MQTT starts after GOT_IP");
        }
#endif
    }

    uint32_t failed_heartbeat = 0;
    while (true) {
        if (init_result == ESP_OK) {
            sensor_test_run_once();
        } else if ((failed_heartbeat++ % 5U) == 0U) {
            ESP_LOGE("app", "diagnostic heartbeat: %s remains inactive because init failed (%s)",
                     sensor_test_name(), esp_err_to_name(init_result));
        }
        vTaskDelay(pdMS_TO_TICKS(sensor_test_interval_ms()));
    }
}
