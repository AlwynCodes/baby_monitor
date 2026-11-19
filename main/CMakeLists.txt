cmake_minimum_required(VERSION 3.13.1)

set(MODEL_FOLDER ..)
set(EI_SDK_FOLDER ../edge-impulse-sdk)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    # ESP-NN is the ESP32-S3 accelerated kernel set. This project targets the
    # classic ESP32, so the portable C++ kernels are used instead.
    MESSAGE("ESP-NN disabled (classic ESP32 target).")
    add_definitions(-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0)
endif()

OPTION(DEFINE_DEBUG
    "Build the project using debugging code"
    OFF)
IF(DEFINE_DEBUG)
    MESSAGE("Adding Debug flag...")
    ADD_DEFINITIONS(-DDEBUG)
    SET(CMAKE_BUILD_TYPE Debug)
    MESSAGE("Build type is " ${CMAKE_BUILD_TYPE})
ENDIF(DEFINE_DEBUG)

set(include_dirs
    ${MODEL_FOLDER}
    ${MODEL_FOLDER}/tflite-model
    ${MODEL_FOLDER}/model-parameters
    ${EI_SDK_FOLDER}
)

include(${EI_SDK_FOLDER}/cmake/utils.cmake)

RECURSIVE_FIND_FILE_EXCLUDE_DIR(SOURCE_FILES "../edge-impulse-sdk" "CMSIS" "*.cpp")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(MODEL_FILES "../tflite-model" "CMSIS" "*.cpp")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(CC_FILES "../edge-impulse-sdk" "CMSIS" "*.cc")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(S_FILES "../edge-impulse-sdk" "CMSIS" "*.s")
RECURSIVE_FIND_FILE_EXCLUDE_DIR(C_FILES "../edge-impulse-sdk" "CMSIS" "*.c")

# Drop the ESP-NN kernels entirely. ESP-NN is Espressif's ESP32-S3/Xtensa-SIMD
# acceleration library; its assembly (ee.vld.128.ip, ee.vmax.s8, ...) cannot be
# assembled for the classic ESP32. ESP-NN is disabled above, so nothing needs
# it, but the glob would otherwise still pull in the *_esp32s3.S sources and
# fail with "unknown opcode or format name 'ee.zero.q'".
foreach(_ei_list SOURCE_FILES MODEL_FILES CC_FILES S_FILES C_FILES)
    list(FILTER ${_ei_list} EXCLUDE REGEX "ESP-NN")
endforeach()

list(APPEND SOURCE_FILES ${S_FILES})
list(APPEND SOURCE_FILES ${C_FILES})
list(APPEND SOURCE_FILES ${CC_FILES})
list(APPEND SOURCE_FILES ${MODEL_FILES})

# The headers in this component include driver/i2s.h, driver/adc.h,
# esp_timer.h, app_network.h and the esp_rmaker_* headers. Without these
# REQUIRES entries those includes fail with "file not found".
#
# rmaker_app_network provides app_network.h (app_network_init /
# app_network_start) and replaces the old hand-written components/app_wifi.
idf_component_register(SRCS "main.cpp" "${SOURCE_FILES}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_rainmaker driver esp_timer nvs_flash esp_wifi esp_netif)

# Compatibility with the vendored Edge Impulse SDK (written for IDF ~4.x):
#  * portTICK_RATE_MS was removed from FreeRTOS in IDF 5.x; portTICK_PERIOD_MS
#    is the direct replacement.
#  * -O2/-O3 triggers a GCC ICE in speechpy/processing.hpp
#    ("insn does not satisfy its constraints ... in extract_constrain_insn"),
#    so build this translation unit at -Os.
add_definitions(-DportTICK_RATE_MS=portTICK_PERIOD_MS)
target_compile_options(${COMPONENT_LIB} PRIVATE -Os -Wno-error=uninitialized)
# Per-target options are placed last on the compile line (verified via
# build/compile_commands.json), so they win over IDF's earlier -Werror=all.
# A *specific* -Wno-error=<warning> is required here: a plain -Wno-error does
# not undo IDF's -Werror=all for individual diagnostics.

# IDF appends its default "-Werror=all" to the build-wide COMPILE_OPTIONS list
# *after* component CMakeLists are processed, so a per-target -Wno-error has no
# effect (the later -Werror wins). Appending here lands after IDF's default and
# actually takes effect. Needed because the vendored Edge Impulse SDK is not
# -Wall/-Wextra clean under xtensa GCC 13 - e.g. dsp/numpy.hpp:1575 warns
# "'g' is used uninitialized" for a deliberate float/int type-pun. Warnings are
# still printed; they just no longer fail the build.
idf_build_set_property(COMPILE_OPTIONS "-Wno-error" APPEND)
