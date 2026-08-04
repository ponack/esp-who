# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ../../components/who_task
                         ../../components/who_peripherals/esp_video_cxx
                         ../../components/who_peripherals/who_lcd
                         ../../components/who_peripherals/who_spiflash_fatfs
                         ../../components/who_frame_cap
                         ../../components/who_frame_lcd_disp
                         ../../components/who_pp_ocr_v6
                         ../../components/who_app/who_pp_ocr_v6_app)

add_compile_options(-fdiagnostics-color=always)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if (NOT DEFINED BSP)
    message(FATAL_ERROR "BSP is not defined, please make sure that the environment variable IDF_EXTRA_ACTIONS_PATH is properly set.")
endif()
idf_build_set_property(DEPENDENCIES_LOCK dependencies.lock.${BSP})
project(pp_ocr_v6)

# Apply hotfixes for two known upstream bugs in managed_components/
# (LVGL frogfs typedef, esp_video V4L2 include). Idempotent — safe to re-run.
# See patches/README.md.
include(${CMAKE_CURRENT_SOURCE_DIR}/patches/apply_patches.cmake)
