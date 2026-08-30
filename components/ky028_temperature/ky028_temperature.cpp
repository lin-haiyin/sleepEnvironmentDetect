#include "ky028_temperature.hpp"

#include "esp_check.h"
#include "esp_log.h"

#include <algorithm>
#include <cstdint>

namespace ky028_temperature {
namespace {
constexpr char TAG[] = "ky028";
adc_oneshot_unit_handle_t s_adc = nullptr;
adc_channel_t s_channel = ADC_CHANNEL_1;
gpio_num_t s_threshold_gpio = GPIO_NUM_NC;
}

esp_err_t init(const Config &config) {
    adc_oneshot_unit_init_cfg_t unit_config{};
    unit_config.unit_id = config.unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &s_adc), TAG, "ADC init failed");
    adc_oneshot_chan_cfg_t channel_config{};
    channel_config.bitwidth = ADC_BITWIDTH_12;
    channel_config.atten = ADC_ATTEN_DB_12;
    s_channel = config.adc_channel;
    s_threshold_gpio = config.threshold_gpio;
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, s_channel, &channel_config),
                        TAG, "ADC channel config failed");
    return gpio_set_direction(s_threshold_gpio, GPIO_MODE_INPUT);
}

void read_once() {
    int64_t sum = 0;
    int minimum = 4095;
    int maximum = 0;
    for (int sample = 0; sample < 32; ++sample) {
        int raw = 0;
        const esp_err_t result = adc_oneshot_read(s_adc, s_channel, &raw);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(result));
            return;
        }
        sum += raw;
        minimum = std::min(minimum, raw);
        maximum = std::max(maximum, raw);
    }
    ESP_LOGI(TAG, "ADC raw average=%lld min=%d max=%d DO=%d (not degrees Celsius)",
             static_cast<long long>(sum / 32), minimum, maximum,
             gpio_get_level(s_threshold_gpio));
    ESP_LOGI(TAG, "test: touch the NTC to change AO; slowly turn the trimpot to switch DO");
}

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: 3V3->+ GND->GND GPIO2->AO GPIO%d->DO; values uncalibrated",
             static_cast<int>(config.threshold_gpio));
}

} // namespace ky028_temperature
