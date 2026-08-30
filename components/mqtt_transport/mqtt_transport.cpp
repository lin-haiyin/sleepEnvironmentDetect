#include "mqtt_transport.hpp"

#include "device_app.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "mqtt_client.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace mqtt_transport {
namespace {
constexpr char TAG[] = "mqtt";
esp_mqtt_client_handle_t s_client = nullptr;
char s_device_id[48]{};
char s_led_topic[96]{};
char s_rgb_topic[96]{};
char s_status_topic[96]{};

bool topic_equals(const esp_mqtt_event_handle_t event, const char *expected) {
    return event != nullptr && event->topic != nullptr &&
           static_cast<size_t>(event->topic_len) == std::strlen(expected) &&
           std::memcmp(event->topic, expected, static_cast<size_t>(event->topic_len)) == 0;
}

void publish_status(const char *payload) {
    if (s_client == nullptr) return;
    esp_mqtt_client_publish(s_client, s_status_topic, payload, 0, 1, true);
}

void on_event(void *, esp_event_base_t, int32_t event_id, void *event_data) {
    auto *event = static_cast<esp_mqtt_event_handle_t>(event_data);
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected; subscribing to %s and %s", s_led_topic, s_rgb_topic);
        esp_mqtt_client_subscribe(s_client, s_led_topic, 1);
        esp_mqtt_client_subscribe(s_client, s_rgb_topic, 1);
        publish_status("{\"state\":\"online\"}");
        break;
    case MQTT_EVENT_DATA: {
        if (!topic_equals(event, s_led_topic) && !topic_equals(event, s_rgb_topic)) break;
        char payload[16]{};
        const size_t length = std::min(sizeof(payload) - 1U, static_cast<size_t>(event->data_len));
        std::memcpy(payload, event->data, length);
        device_app::CommandReply reply{};
        const char *relative_topic = topic_equals(event, s_led_topic)
                                         ? device_app::topic::LED_SET
                                         : device_app::topic::RGB_SET;
        const esp_err_t result = device_app::handle_command(relative_topic, payload, &reply);
        char full_reply_topic[96]{};
        std::snprintf(full_reply_topic, sizeof(full_reply_topic), "%s/%s", s_device_id, reply.topic);
        ESP_LOGI(TAG, "command topic=%.*s payload=%s result=%s", event->topic_len, event->topic,
                 payload, esp_err_to_name(result));
        if (reply.topic != nullptr) {
            esp_mqtt_client_publish(s_client, full_reply_topic, reply.payload, 0, 1, false);
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT transport error");
        break;
    default:
        break;
    }
}
} // namespace

esp_err_t start(const Config &config) {
    if (config.broker_uri == nullptr || config.device_id == nullptr || config.device_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_client != nullptr) return ESP_ERR_INVALID_STATE;
    std::snprintf(s_device_id, sizeof(s_device_id), "%s", config.device_id);
    std::snprintf(s_led_topic, sizeof(s_led_topic), "%s/%s", s_device_id, device_app::topic::LED_SET);
    std::snprintf(s_rgb_topic, sizeof(s_rgb_topic), "%s/%s", s_device_id, device_app::topic::RGB_SET);
    std::snprintf(s_status_topic, sizeof(s_status_topic), "%s/%s", s_device_id, device_app::topic::STATUS);

    esp_mqtt_client_config_t mqtt_config{};
    mqtt_config.broker.address.uri = config.broker_uri;
    mqtt_config.credentials.username = config.username;
    mqtt_config.credentials.authentication.password = config.password;
    mqtt_config.session.last_will.topic = s_status_topic;
    mqtt_config.session.last_will.msg = "{\"state\":\"offline\"}";
    mqtt_config.session.last_will.qos = 1;
    mqtt_config.session.last_will.retain = 1;
    if (config.root_ca_pem != nullptr) {
        mqtt_config.broker.verification.certificate = config.root_ca_pem;
    }
    s_client = esp_mqtt_client_init(&mqtt_config);
    if (s_client == nullptr) return ESP_ERR_NO_MEM;
    esp_err_t result = esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, on_event, nullptr);
    if (result == ESP_OK) result = esp_mqtt_client_start(s_client);
    if (result != ESP_OK) {
        esp_mqtt_client_destroy(s_client);
        s_client = nullptr;
    }
    return result;
}

esp_err_t stop() {
    if (s_client == nullptr) return ESP_ERR_INVALID_STATE;
    esp_err_t result = esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = nullptr;
    return result;
}

int publish(const char *relative_topic, const char *payload, int qos, bool retain) {
    if (s_client == nullptr || relative_topic == nullptr || payload == nullptr) return -1;
    char full_topic[96]{};
    std::snprintf(full_topic, sizeof(full_topic), "%s/%s", s_device_id, relative_topic);
    return esp_mqtt_client_publish(s_client, full_topic, payload, 0, qos, retain);
}

bool is_started() { return s_client != nullptr; }

const char *device_id() { return s_device_id; }

int publish_measurement(const char *relative_topic, double value, const char *unit,
                        bool valid, const char *error, int qos, bool retain) {
    if (relative_topic == nullptr || unit == nullptr || unit[0] == '\0') return -1;
    char payload[192]{};
    if (valid) {
        std::snprintf(payload, sizeof(payload),
                      "{\"value\":%.3f,\"unit\":\"%s\",\"valid\":true,\"ts_ms\":%lu}",
                      value, unit, static_cast<unsigned long>(esp_log_timestamp()));
    } else {
        const char *reason = error != nullptr && error[0] != '\0' ? error : "unavailable";
        std::snprintf(payload, sizeof(payload),
                      "{\"value\":0,\"unit\":\"%s\",\"valid\":false,\"error\":\"%s\",\"ts_ms\":%lu}",
                      unit, reason, static_cast<unsigned long>(esp_log_timestamp()));
    }
    return publish(relative_topic, payload, qos, retain);
}

} // namespace mqtt_transport
