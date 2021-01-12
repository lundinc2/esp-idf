set(esp_idf_dir $ENV{IDF_PATH})
set(posix_root_dir "Lab-Project-FreeRTOS-POSIX")
set(freertos_plus_posix_dir "${posix_root_dir}/FreeRTOS-Plus-POSIX")


idf_component_register(
        SRC_DIRS "${freertos_plus_posix_dir}/source"

        INCLUDE_DIRS
        "${posix_root_dir}/include"
        "${freertos_plus_posix_dir}/include"

        PRIV_INCLUDE_DIRS
        "${freertos_plus_posix_dir}/include/portable/espressif/esp32_devkitc_esp_wrover_kit"
        "${posix_root_dir}/include/private"
        "${esp_idf_dir}/components/freertos/include/freertos"
)
set_source_files_properties("${freertos_plus_posix_dir}/source/FreeRTOS_POSIX_pthread_cond.c"
    PROPERTIES COMPILE_FLAGS
    "-Wno-maybe-uninitialized"
)
