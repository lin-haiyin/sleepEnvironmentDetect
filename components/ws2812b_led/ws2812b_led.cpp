#include "ws2812b_led.hpp"

#include "esp_log.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>

namespace ws2812b_led {
namespace {
constexpr char TAG[] = "ws2812b";
led_strip_handle_t s_strip = nullptr;
SemaphoreHandle_t s_mutex = nullptr;
uint32_t s_led_count = 0;
volatile uint8_t s_command = 7;
uint8_t s_step = 0;

const char *const COMMAND_NAMES[] = {
    "off", "warm_breath", "blue_breath", "marquee", "white", "chase", "rainbow", "auto"};

void set_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (uint32_t index = 0; index < s_led_count; ++index) {
        if (led_strip_set_pixel(s_strip, index, red, green, blue) != ESP_OK) return;
    }
}

esp_err_t render(uint8_t command) {
    if (s_strip == nullptr || s_led_count == 0) return ESP_ERR_INVALID_STATE;
    if (command == 0) {
        set_all(0, 0, 0);
    } else if (command == 1 || command == 2) {
        const uint8_t phase = static_cast<uint8_t>(s_step % 32U);
        const uint8_t triangle = phase < 16U ? phase : static_cast<uint8_t>(31U - phase);
        const uint8_t level = static_cast<uint8_t>(4U + triangle * 2U);
        if (command == 1) {
            set_all(level, static_cast<uint8_t>(level * 3U / 4U),
                    static_cast<uint8_t>(level / 8U));
        } else {
            set_all(static_cast<uint8_t>(level / 16U), static_cast<uint8_t>(level / 4U), level);
        }
    } else if (command == 5) {
        set_all(0, 0, 0);
        const uint32_t index = s_step % s_led_count;
        led_strip_set_pixel(s_strip, index, 0, 0, 64);
    } else if (command == 3) {
        set_all(0, 0, 0);
        const uint32_t index = s_step % s_led_count;
        const uint32_t previous = (index + s_led_count - 1U) % s_led_count;
        led_strip_set_pixel(s_strip, previous, 8, 3, 0);
        led_strip_set_pixel(s_strip, index, 32, 12, 1);
    } else if (command == 4) {
        set_all(24, 24, 24);
    } else if (command == 6) {
        for (uint32_t index = 0; index < s_led_count; ++index) {
            const uint8_t hue = static_cast<uint8_t>(index * 256U / s_led_count + s_step * 8U);
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            if (hue < 85) {
                red = static_cast<uint8_t>(255 - hue * 3);
                green = static_cast<uint8_t>(hue * 3);
            } else if (hue < 170) {
                const uint8_t shifted = static_cast<uint8_t>(hue - 85);
                green = static_cast<uint8_t>(255 - shifted * 3);
                blue = static_cast<uint8_t>(shifted * 3);
            } else {
                const uint8_t shifted = static_cast<uint8_t>(hue - 170);
                blue = static_cast<uint8_t>(255 - shifted * 3);
                red = static_cast<uint8_t>(shifted * 3);
            }
            led_strip_set_pixel(s_strip, index, red / 4, green / 4, blue / 4);
        }
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    return led_strip_refresh(s_strip);
}
}

esp_err_t init(const Config &config) {
    s_led_count = config.led_count;
    led_strip_config_t strip_config{};
    strip_config.strip_gpio_num = config.data_gpio;
    strip_config.max_leds = config.led_count;
    strip_config.led_model = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    led_strip_rmt_config_t rmt_config{};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz = 10 * 1000 * 1000;
    const esp_err_t result = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_strip);
    if (result == ESP_OK) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == nullptr) {
            led_strip_del(s_strip);
            s_strip = nullptr;
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "initialized %lu WS2812B LEDs on GPIO%d",
                 static_cast<unsigned long>(config.led_count), static_cast<int>(config.data_gpio));
    }
    return result;
}

void run_once() {
    if (s_mutex == nullptr || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "effect update skipped: LED mutex unavailable");
        return;
    }
    uint8_t command = s_command;
    if (command == 7) command = 1;
    else if (command == 8) command = 2;
    else if (command == 9) command = 3;
    const esp_err_t result = render(command);
    ESP_LOGI(TAG, "effect=%s command=%u step=%u result=%s",
             command_name(command), static_cast<unsigned>(s_command),
             static_cast<unsigned>(s_step), esp_err_to_name(result));
    ++s_step;
    xSemaphoreGive(s_mutex);
}

esp_err_t set_command(uint8_t command) {
    if (!(command == 0 || command == 7 || command == 8 || command == 9)) return ESP_ERR_INVALID_ARG;
    if (s_mutex == nullptr || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    s_command = command;
    s_step = 0;
    const uint8_t render_command = command == 7 ? 1 : (command == 8 ? 2 : (command == 9 ? 3 : 0));
    const esp_err_t result = render(render_command);
    ESP_LOGI(TAG, "command=%u effect=%s result=%s", static_cast<unsigned>(command),
             command_name(command), esp_err_to_name(result));
    xSemaphoreGive(s_mutex);
    return result;
}

const char *command_name(uint8_t command) {
    if (command == 0 || command == 7) return command == 0 ? "off" : "warm_breath";
    if (command == 8) return "blue_breath";
    if (command == 9) return "marquee";
    return "invalid";
}

void log_wiring(const Config &config) {
    ESP_LOGI(TAG, "wiring: 5V->VCC common GND GPIO%d->DIN (S-V-G input) through 330-470 ohm resistor; LEDs=%lu",
             static_cast<int>(config.data_gpio), static_cast<unsigned long>(config.led_count));
    ESP_LOGI(TAG, "do not connect GPIO to the V-G-DOUT output end; data direction is DIN -> DOUT");
}

} // namespace ws2812b_led
