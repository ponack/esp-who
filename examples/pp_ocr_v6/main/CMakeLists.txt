set(srcs        app_main.cpp
                frame_cap_pipeline.cpp)

set(include_dirs    ./)

set(requires who_pp_ocr_v6_app
             esp_video_cxx)

idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${include_dirs} REQUIRES ${requires})

if(CONFIG_PP_OCR_V6_EXAMPLE_HAVE_CJK_FONT)
    set(_cjk_font_c
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/pp_ocr_v6_cjk_24.c")
    if(NOT EXISTS "${_cjk_font_c}")
        message(FATAL_ERROR
            "CONFIG_PP_OCR_V6_EXAMPLE_HAVE_CJK_FONT=y but ${_cjk_font_c} "
            "is missing. Generate it first with: "
            "python3 tools/gen_cjk_font.py <path/to/cjk.ttf> --size 24 --bpp 2")
    endif()
    # `who_pp_ocr_v6_app` links before main; force-keep the font symbol.
    idf_build_set_property(LINK_OPTIONS
        "-Wl,--undefined=pp_ocr_v6_cjk_24" APPEND)
    target_sources(${COMPONENT_LIB} PRIVATE ${_cjk_font_c})
    # lv_font_conv emits `#include "lvgl/lvgl.h"`; the IDF-namespaced header
    # is at `lvgl.h`, so use the simple-include form.
    set_source_files_properties(${_cjk_font_c} PROPERTIES
        COMPILE_DEFINITIONS "LV_LVGL_H_INCLUDE_SIMPLE=1")
    idf_component_get_property(_lvgl_lib lvgl__lvgl COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PRIVATE ${_lvgl_lib})
endif()
