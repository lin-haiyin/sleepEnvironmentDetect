#include "c4002_radar.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace c4002_radar {
namespace {
constexpr char TAG[] = "c4002";
Config s_config{};
std::array<uint8_t, 128> s_rx{};
size_t s_rx_length = 0;

uint16_t checksum16(const uint8_t *data, size_t length) {
    uint16_t sum = 0;
    for (size_t i = 0; i < length; ++i) sum = static_cast<uint16_t>(sum + data[i]);
    return sum;
}

void log_hex(const uint8_t *bytes, size_t length) {
    char line[3 * 48 + 1]{};
    size_t used = 0;
    const size_t shown = std::min(length, size_t{48});
    for (size_t i = 0; i < shown; ++i) {
        used += static_cast<size_t>(std::snprintf(line + used, sizeof(line) - used,
                                                  "%02X%s", bytes[i], i + 1 == shown ? "" : " "));
    }
    ESP_LOGI(TAG, "frame [%u]: %s%s", unsigned(length), line, length > shown ? " ..." : "");
}

void parse_buffer() {
    while (s_rx_length >= 8) {
        size_t header = 0;
        while (header + 3 < s_rx_length &&
               !(s_rx[header] == 0xfa && s_rx[header + 1] == 0xf5 &&
                 s_rx[header + 2] == 0xaa && s_rx[header + 3] == 0xa5)) ++header;
        if (header > 0) {
            std::memmove(s_rx.data(), s_rx.data() + header, s_rx_length - header);
            s_rx_length -= header;
        }
        if (s_rx_length < 8) return;
        const size_t frame_length = s_rx[4] | (size_t{s_rx[5]} << 8U);
        if (frame_length < 10 || frame_length > s_rx.size()) {
            --s_rx_length;
            std::memmove(s_rx.data(), s_rx.data() + 1, s_rx_length);
            continue;
        }
        if (s_rx_length < frame_length) return;
        const uint16_t expected = static_cast<uint16_t>(
            s_rx[frame_length - 2] | (s_rx[frame_length - 1] << 8U));
        log_hex(s_rx.data(), frame_length);
        ESP_LOGI(TAG, "type=0x%02X checksum=%s", s_rx[7],
                 expected == checksum16(s_rx.data(), frame_length - 2) ? "OK" : "BAD");
        std::memmove(s_rx.data(), s_rx.data() + frame_length, s_rx_length - frame_length);
        s_rx_length -= frame_length;
    }
}
} // namespace

esp_err_t init(const Config &config) {
    s_config = config;
    uart_config_t uart{};
    uart.baud_rate = config.baud;
    uart.data_bits = UART_DATA_8_BITS;
    uart.parity = UART_PARITY_DISABLE;
    uart.stop_bits = UART_STOP_BITS_1;
    uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart.source_clk = UART_SCLK_DEFAULT;
    ESP_RETURN_ON_ERROR(uart_param_config(config.port, &uart), TAG, "UART config");
    ESP_RETURN_ON_ERROR(uart_set_pin(config.port, config.tx_gpio, config.rx_gpio,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "UART pins");
    ESP_RETURN_ON_ERROR(uart_driver_install(config.port, 2048, 0, 0, nullptr, 0),
                        TAG, "UART driver");
    uint8_t command[] = {0xfa, 0xf5, 0xaa, 0xa5, 0x0f, 0, 0, 0,
                         0x83, 0, 0x05, 0, 0x0a, 0, 0};
    const uint16_t sum = checksum16(command, sizeof(command) - 2);
    command[sizeof(command) - 2] = static_cast<uint8_t>(sum);
    command[sizeof(command) - 1] = static_cast<uint8_t>(sum >> 8U);
    uart_flush_input(config.port);
    if (uart_write_bytes(config.port, command, sizeof(command)) != sizeof(command)) return ESP_FAIL;
    ESP_LOGI(TAG, "UART %d 8N1; requested 1 s reports", config.baud);
    return ESP_OK;
}

void read_once() {
    const int received = uart_read_bytes(s_config.port, s_rx.data() + s_rx_length,
                                         s_rx.size() - s_rx_length, pdMS_TO_TICKS(200));
    if (received > 0) {
        s_rx_length += static_cast<size_t>(received);
        parse_buffer();
    } else {
        ESP_LOGW(TAG, "no UART data; check crossed TX/RX and baud rate");
    }
    if (s_rx_length == s_rx.size()) s_rx_length = 0;
}

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: 5V->VCC GND->GND C4002_TX->GPIO%d C4002_RX->GPIO%d baud=%d",
             static_cast<int>(config.rx_gpio), static_cast<int>(config.tx_gpio), config.baud);
}

} // namespace c4002_radar
