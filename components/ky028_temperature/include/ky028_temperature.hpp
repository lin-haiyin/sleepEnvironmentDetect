#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

namespace ky028_temperature {

struct Config {
    adc_unit_t unit;
    adc_channel_t adc_channel;
    gpio_num_t threshold_gpio;
};

esp_err_t init(const Config &config);
void read_once();
void log_wiring(const Config &config);

} // namespace ky028_temperature
