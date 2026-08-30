#include "ky018_light.hpp"

#include "esp_check.h"
#include "esp_log.h"

#include <algorithm>
#include <cstdint>

namespace ky018_light {
namespace {
constexpr char TAG[] = "ky018";
adc_oneshot_unit_handle_t s_adc = nullptr;
adc_channel_t s_channel = ADC_CHANNEL_0;
}

esp_err_t init(const Config &config) {
    adc_oneshot_unit_init_cfg_t unit_config{};
    unit_config.unit_id = config.unit;
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &s_adc), TAG, "ADC init failed");
    adc_oneshot_chan_cfg_t channel_config{};
    channel_config.bitwidth = ADC_BITWIDTH_12;
    channel_config.atten = ADC_ATTEN_DB_12;
    s_channel = config.channel;
    return adc_oneshot_config_channel(s_adc, s_channel, &channel_config);
}

void read_once() {
    Reading reading{};
    if (read(&reading) != ESP_OK) return;
    ESP_LOGI(TAG, "ADC raw average=%d min=%d max=%d full_scale=%d%% (not lux)",
             reading.average, reading.minimum, reading.maximum, (reading.average * 100) / 4095);
}

esp_err_t read(Reading *reading) {
    if (reading == nullptr || s_adc == nullptr) return ESP_ERR_INVALID_STATE;
    int64_t sum = 0;
    int minimum = 4095;
    int maximum = 0;
    for (int sample = 0; sample < 32; ++sample) {
        int raw = 0;
        const esp_err_t result = adc_oneshot_read(s_adc, s_channel, &raw);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(result));
            return result;
        }
        sum += raw;
        minimum = std::min(minimum, raw);
        maximum = std::max(maximum, raw);
    }
    reading->average = static_cast<int>(sum / 32);
    reading->minimum = minimum;
    reading->maximum = maximum;
    return ESP_OK;
}

void log_wiring() {
    ESP_LOGI(TAG, "wiring: 3V3->+ GND->- GPIO1->S/AO (ADC1_CH0)");
    ESP_LOGI(TAG, "test: cover the photoresistor, then shine a lamp on it; compare ADC raw");
}

} // namespace ky018_light
