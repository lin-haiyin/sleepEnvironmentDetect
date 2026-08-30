#include "ws2812_console_control.hpp"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "device_app.hpp"

#include <cstdio>
#include <cstdint>

namespace {
constexpr char TAG[] = "ws_console";

void console_task(void *) {
    ESP_LOGI(TAG, "ready: type one digit in the serial monitor (Enter optional)");
    ESP_LOGI(TAG, "effects: 0=off 7=warm-breath 8=blue-breath 9=marquee");
    while (true) {
        const int character = std::getchar();
        if (character == '0' || character == '7' || character == '8' || character == '9') {
            char payload[] = {static_cast<char>(character), '\0'};
            device_app::CommandReply reply{};
            const esp_err_t result = device_app::handle_command(
                device_app::topic::LED_SET, payload, &reply);
            ESP_LOGI(TAG, "route=%s result=%s reply_topic=%s reply=%s",
                     device_app::topic::LED_SET, esp_err_to_name(result), reply.topic, reply.payload);
        } else if (character != '\r' && character != '\n' && character != EOF) {
            ESP_LOGW(TAG, "ignored '%c'; valid commands: 0, 7, 8, 9", character);
        }
        if (character == EOF) vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

void ws2812_console_control_start() {
    static bool started = false;
    if (started) return;
    started = true;
    if (xTaskCreate(console_task, "ws_console", 3072, nullptr, 5, nullptr) != pdPASS) {
        started = false;
        ESP_LOGE(TAG, "failed to create console task");
    }
}
