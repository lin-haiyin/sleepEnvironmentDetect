idf_component_register(
    SRCS "main.cpp" "sensor_test_runner.cpp" "rgb_console_control.cpp" "ws2812_console_control.cpp" "combined_console_control.cpp"
    INCLUDE_DIRS "." "../include"
    REQUIRES
        spi_flash
        esp_psram
        max30102
        ky018_light
        ky028_temperature
        dht11_sensor
        c4002_radar
        hcsr04_ultrasonic
        ky037_sound
        ky016_rgb
        device_app
        ws2812b_led
        mqtt_transport
        network_manager
)
