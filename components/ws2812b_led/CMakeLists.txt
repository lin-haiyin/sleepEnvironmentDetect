idf_component_register(
    SRCS "ws2812b_led.cpp"
    INCLUDE_DIRS "include"
    REQUIRES espressif__led_strip esp_driver_gpio
)
