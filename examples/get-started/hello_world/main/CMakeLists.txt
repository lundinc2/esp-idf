set(esp_idf_dir $ENV{IDF_PATH})
set(posix_root_dir "../components/freertos_posix/Lab-Project-FreeRTOS-POSIX")
set(freertos_plus_posix_dir "${posix_root_dir}/FreeRTOS-Plus-POSIX")

idf_component_register(
    SRCS
        "hello_world_main.c"
    INCLUDE_DIRS
        "${freertos_plus_posix_dir}/include/portable/espressif/esp32_devkitc_esp_wrover_kit"
        "${posix_root_dir}/include"
        "${esp_idf_dir}/components/freertos/include/freertos"
    )
