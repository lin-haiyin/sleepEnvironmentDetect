idf_component_register(
    SRCS "mqtt_transport.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt device_app
)
