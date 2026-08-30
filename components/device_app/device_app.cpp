#include "device_app.hpp"

#include "ky016_rgb.hpp"
#include "ws2812b_led.hpp"

#include "esp_log.h"

#include <cstdio>
#include <cstring>

namespace device_app {
namespace {
constexpr char TAG[] = "device_app";

void set_reply(CommandReply *reply, const char *reply_topic, const char *payload) {
    if (reply == nullptr) return;
    reply->accepted = false;
    reply->topic = reply_topic;
    std::snprintf(reply->payload, sizeof(reply->payload), "%s", payload);
}

bool is_single_command(const char *payload, char max_digit) {
    return payload != nullptr && payload[0] >= '0' && payload[0] <= max_digit && payload[1] == '\0';
}

bool is_led_command(const char *payload) {
    return payload != nullptr && payload[1] == '\0' &&
           (payload[0] == '0' || payload[0] == '7' || payload[0] == '8' || payload[0] == '9');
}
} // namespace

esp_err_t handle_command(const char *command_topic, const char *payload, CommandReply *reply) {
    if (command_topic == nullptr || reply == nullptr) return ESP_ERR_INVALID_ARG;
    const bool is_rgb = std::strcmp(command_topic, topic::RGB_SET) == 0;
    const bool is_led = std::strcmp(command_topic, topic::LED_SET) == 0;
    if (!is_rgb && !is_led) {
        set_reply(reply, topic::RGB_STATE, "{\"accepted\":false,\"reason\":\"unknown_topic\"}");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!is_rgb && !is_led_command(payload)) {
        set_reply(reply, topic::LED_STATE, "{\"accepted\":false,\"reason\":\"payload_must_be_0_7_8_or_9\"}");
        return ESP_ERR_INVALID_ARG;
    }
    if (is_rgb && !is_single_command(payload, '3')) {
        set_reply(reply, is_rgb ? topic::RGB_STATE : topic::LED_STATE, is_rgb ?
                  "{\"accepted\":false,\"reason\":\"payload_must_be_0_to_3\"}" :
                  "{\"accepted\":false,\"reason\":\"payload_must_be_0_to_7\"}");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t command = static_cast<uint8_t>(payload[0] - '0');
    esp_err_t result = ESP_OK;
    if (command == 0) {
        // Product rule: command 0 is the global off command. This remains
        // idempotent when the same command is sent repeatedly.
        const esp_err_t rgb_result = ky016_rgb::set_command(0);
        const esp_err_t strip_result = ws2812b_led::set_command(0);
        // In a single-actuator diagnostic profile the other driver may not be
        // initialized; success of either active driver is sufficient.
        result = (rgb_result == ESP_OK || strip_result == ESP_OK) ? ESP_OK : rgb_result;
    } else {
        result = is_rgb ? ky016_rgb::set_command(command)
                        : ws2812b_led::set_command(command);
    }
    if (result != ESP_OK) {
        set_reply(reply, is_rgb ? topic::RGB_STATE : topic::LED_STATE,
                  "{\"accepted\":false,\"reason\":\"actuator_error\"}");
        return result;
    }

    std::snprintf(reply->payload, sizeof(reply->payload),
                  "{\"accepted\":true,\"command\":%u,\"state\":\"%s\"}",
                  static_cast<unsigned>(command),
                  is_rgb ? ky016_rgb::command_name(command) : ws2812b_led::command_name(command));
    reply->accepted = true;
    reply->topic = is_rgb ? topic::RGB_STATE : topic::LED_STATE;
    ESP_LOGI(TAG, "accepted topic=%s payload=%s", command_topic, payload);
    return ESP_OK;
}

} // namespace device_app
