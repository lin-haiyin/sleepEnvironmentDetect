#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

namespace c4002_radar {

struct Config {
    uart_port_t port;
    gpio_num_t tx_gpio;
    gpio_num_t rx_gpio;
    int baud;
};

esp_err_t init(const Config &config);
void read_once();
void log_wiring(const Config &config);

} // namespace c4002_radar
