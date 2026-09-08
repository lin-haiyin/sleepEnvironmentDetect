idf_component_register(
    SRCS "hcsr04_ultrasonic.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer esp_driver_gpio
)
