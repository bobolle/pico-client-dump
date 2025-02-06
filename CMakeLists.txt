cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)
set(WIFI_SSID "EXAMPLE_SSID")
set(WIFI_PASSWD "EXAMPLE_PASSWD")

include(pico_sdk_import.cmake)

project(dump C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(pico-ultrasonic-lib)

add_executable(dump
    pico_tcp_client.c
)

pico_enable_stdio_usb(dump 1)
pico_enable_stdio_uart(dump 1)
pico_add_extra_outputs(dump)

target_compile_definitions(dump PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWD=\"${WIFI_PASSWD}\"
)

target_include_directories(dump PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(dump
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_ultrasonic_lib
)
