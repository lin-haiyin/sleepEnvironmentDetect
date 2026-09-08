#include "rgb_console_control.hpp"

#include "device_app.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

namespace {
constexpr char TAG[] = "rgb_console";

void console_task(void *) {
    ESP_LOGI(TAG, "ready: type 0=off, 1=red, 2=green, 3=blue then Enter");
    while (true) {
        const int character = std::getchar();
        if (character >= '0' && character <= '3') {
            char payload[] = {static_cast<char>(character), '\0'};
            device_app::CommandReply reply{};
            const esp_err_t result = device_app::handle_command(
                device_app::topic::RGB_SET, payload, &reply);
            ESP_LOGI(TAG, "route=%s result=%s reply_topic=%s reply=%s",
                     device_app::topic::RGB_SET, esp_err_to_name(result), reply.topic, reply.payload);
        } else if (character != '\r' && character != '\n' && character != EOF) {
            ESP_LOGW(TAG, "ignored '%c'; valid commands: 0, 1, 2, 3", character);
        }
        if (character == EOF) vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

void rgb_console_control_start() {
    static bool started = false;
    if (started) return;
    started = true;
    const BaseType_t result = xTaskCreate(console_task, "rgb_console", 3072, nullptr, 5, nullptr);
    if (result != pdPASS) {
        started = false;
        ESP_LOGE(TAG, "failed to create console task");
    }
}
