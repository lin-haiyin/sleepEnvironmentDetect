#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

namespace ky018_light {

struct Config {
    adc_unit_t unit;
    adc_channel_t channel;
};

struct Reading {
    int average;
    int minimum;
    int maximum;
};

esp_err_t init(const Config &config);
void read_once();
esp_err_t read(Reading *reading);
void log_wiring();

} // namespace ky018_light
