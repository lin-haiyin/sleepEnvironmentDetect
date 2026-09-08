#include "combined_console_control.hpp"

#include "device_app.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

namespace {
constexpr char TAG[] = "console";

void execute(const char *route, char character) {
    char payload[] = {character, '\0'};
    device_app::CommandReply reply{};
    const esp_err_t result = device_app::handle_command(route, payload, &reply);
    ESP_LOGI(TAG, "route=%s command=%c result=%s reply=%s",
             route, character, esp_err_to_name(result), reply.payload);
}

void console_task(void *) {
    ESP_LOGI(TAG, "ready: 0=both off, 1/2/3=RGB, 7/8/9=strip; Enter optional");
    while (true) {
        const int character = std::getchar();
        if (character == '0') {
            execute(device_app::topic::RGB_SET, '0');
            execute(device_app::topic::LED_SET, '0');
        } else if (character >= '1' && character <= '3') {
            execute(device_app::topic::RGB_SET, static_cast<char>(character));
        } else if (character == '7' || character == '8' || character == '9') {
            execute(device_app::topic::LED_SET, static_cast<char>(character));
        } else if (character != '\r' && character != '\n' && character != EOF) {
            ESP_LOGW(TAG, "ignored '%c'; valid commands: 0, 1, 2, 3, 7, 8, 9", character);
        }
        if (character == EOF) vTaskDelay(pdMS_TO_TICKS(20));
    }
}
} // namespace

void combined_console_control_start() {
    static bool started = false;
    if (started) return;
    started = true;
    if (xTaskCreate(console_task, "console", 3072, nullptr, 5, nullptr) != pdPASS) {
        started = false;
        ESP_LOGE(TAG, "failed to create combined console task");
    }
}
