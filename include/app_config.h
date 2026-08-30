#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"

namespace envcfg {

enum class SensorTestMode {
    Integrated,
    I2cScan,
    Max30102,
    Ky018Light,
    Ky028TemperatureModule,
    Dht11,
    C4002,
    HcSr04,
    Ky037Sound,
    Ky016Rgb,
    Ws2812b,
};

// Initialize one module only. Change this line, rebuild and flash for each test.
// Product runtime: collect all currently used sensors and keep both actuators available.
// Change to a single module only when isolating hardware during bring-up.
constexpr SensorTestMode ACTIVE_TEST = SensorTestMode::Integrated;

constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
constexpr uint32_t I2C_HZ = 400000;
constexpr uint8_t MAX30102_ADDRESS = 0x57;

constexpr uart_port_t RADAR_UART = UART_NUM_1;
constexpr gpio_num_t RADAR_TX = GPIO_NUM_17; // ESP TX -> C4002 RX
constexpr gpio_num_t RADAR_RX = GPIO_NUM_18; // ESP RX <- C4002 TX
constexpr int RADAR_BAUD = 115200;            // DFRobot documented default

constexpr adc_unit_t ANALOG_ADC_UNIT = ADC_UNIT_1;
constexpr adc_channel_t LIGHT_ADC_CHANNEL = ADC_CHANNEL_0; // GPIO1
constexpr adc_channel_t KY028_ADC_CHANNEL = ADC_CHANNEL_1; // GPIO2
constexpr gpio_num_t DHT11_GPIO = GPIO_NUM_4;
constexpr gpio_num_t KY028_DO_GPIO = GPIO_NUM_5;
constexpr gpio_num_t HCSR04_TRIG_GPIO = GPIO_NUM_13;
constexpr gpio_num_t HCSR04_ECHO_GPIO = GPIO_NUM_14;
constexpr adc_channel_t KY037_ADC_CHANNEL = ADC_CHANNEL_6; // GPIO7
constexpr gpio_num_t KY037_DO_GPIO = GPIO_NUM_15;
constexpr gpio_num_t KY016_RED_GPIO = GPIO_NUM_10;
constexpr gpio_num_t KY016_GREEN_GPIO = GPIO_NUM_11;
constexpr gpio_num_t KY016_BLUE_GPIO = GPIO_NUM_12;
constexpr gpio_num_t WS2812B_DATA_GPIO = GPIO_NUM_6;
// Sleep-scene test: drive all ten LEDs.
constexpr uint32_t WS2812B_LED_COUNT = 10;

constexpr uint32_t TEST_INTERVAL_MS = 1000;
constexpr uint32_t DHT11_INTERVAL_MS = 2000;
constexpr uint32_t HCSR04_INTERVAL_MS = 100;
constexpr uint32_t WS2812B_INTERVAL_MS = 100;
constexpr uint32_t INTEGRATED_INTERVAL_MS = 100;

// Network integration remains opt-in. Do not place broker credentials here;
// pass them from a local, ignored production configuration when enabled.
// Integration mode: Wi-Fi and MQTT start asynchronously after sensor/actuator init.
constexpr bool MQTT_ENABLED = true;

} // namespace envcfg
