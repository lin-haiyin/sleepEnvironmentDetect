#include "dht11_sensor.hpp"

#include "dht.h"
#include "esp_log.h"

namespace dht11_sensor {
namespace {
constexpr char TAG[] = "dht11";
gpio_num_t s_data_gpio = GPIO_NUM_NC;
}

esp_err_t init(const Config &config) {
    s_data_gpio = config.data_gpio;
    gpio_config_t gpio{};
    gpio.pin_bit_mask = 1ULL << config.data_gpio;
    gpio.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    gpio.pull_up_en = GPIO_PULLUP_ENABLE;
    return gpio_config(&gpio);
}

void read_once() {
    Reading reading{};
    const esp_err_t result = read(&reading);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "temperature=%.1f C humidity=%.1f %%RH", reading.temperature_c, reading.humidity_rh);
    } else {
        ESP_LOGE(TAG, "read failed: %s; check DATA pull-up", esp_err_to_name(result));
    }
}

esp_err_t read(Reading *reading) {
    if (reading == nullptr || s_data_gpio == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;
    float humidity = 0;
    float temperature = 0;
    const esp_err_t result = dht_read_float_data(DHT_TYPE_DHT11, s_data_gpio, &humidity, &temperature);
    if (result == ESP_OK) {
        reading->temperature_c = temperature;
        reading->humidity_rh = humidity;
    }
    return result;
}

void log_wiring(const Config &config, unsigned interval_ms) {
    ESP_LOGI(TAG, "wiring: 3V3->VCC GND->GND GPIO%d->DATA; interval=%u ms",
             static_cast<int>(config.data_gpio), interval_ms);
}

} // namespace dht11_sensor
