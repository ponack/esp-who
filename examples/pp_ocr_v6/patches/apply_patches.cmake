# Apply patches for known upstream bugs in managed_components/. Idempotent —
# safe to re-run every CMake configure. Each patch has a unique marker string
# that we grep the target file for; if present, the patch is skipped.
#
# Included from the top-level CMakeLists.txt AFTER project(...), because
# project() triggers the IDF Component Manager to populate managed_components/.
#
# See patches/README.md for what each patch fixes and when it can be dropped.

find_program(PATCH_EXECUTABLE patch DOC "GNU patch, required to auto-apply managed_components/ hotfixes")
if(NOT PATCH_EXECUTABLE)
    message(FATAL_ERROR
        "`patch` not found in PATH. This example needs it to apply hotfixes to "
        "managed_components/ (see patches/README.md). On Debian/Ubuntu install "
        "with `sudo apt install patch`; on macOS it ships with Xcode CLT.")
endif()

# _apply_patch_once(<patch_file> <target_file> <marker>)
#   marker: a unique substring that appears in <target_file> ONLY after the
#           patch has been applied — used as an idempotency guard.
function(_apply_patch_once patch_file target_file marker)
    if(NOT EXISTS ${target_file})
        # Managed component not yet fetched — this happens on the very first
        # configure of a fresh checkout, where the Component Manager runs
        # AFTER this include(). It's not an error; the next configure will
        # see the file and apply the patch. Emit a soft warning to make the
        # two-pass behavior visible.
        message(STATUS "Patch target not present yet, will retry on next "
                       "configure: ${target_file}")
        return()
    endif()
    file(READ ${target_file} _content)
    string(FIND "${_content}" "${marker}" _found)
    if(NOT _found EQUAL -1)
        return()
    endif()
    execute_process(
        COMMAND ${PATCH_EXECUTABLE} -p1 -i ${patch_file}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "Failed to apply ${patch_file}\n"
            "stdout:\n${_out}\n"
            "stderr:\n${_err}\n"
            "The upstream managed_component likely changed shape. Regenerate "
            "the patch from its current source, or delete the patch if the "
            "upstream bug has been fixed.")
    endif()
    message(STATUS "Applied managed_components hotfix: ${patch_file}")
endfunction()

set(_mc ${CMAKE_CURRENT_SOURCE_DIR}/managed_components)
set(_pd ${CMAKE_CURRENT_LIST_DIR})

_apply_patch_once(
    ${_pd}/lvgl_frogfs_typedef.patch
    ${_mc}/lvgl__lvgl/src/libs/frogfs/src/frogfs_format.h
    "Upstream LVGL 9.5.0 bug"
)
_apply_patch_once(
    ${_pd}/esp_video_v4l2_include.patch
    ${_mc}/espressif__esp_video/private_include/esp_video_internal.h
    "Upstream header hygiene fix"
)
_apply_patch_once(
    ${_pd}/pp_ocr_v6_param_copy_false.patch
    ${_mc}/espressif__pp_ocr_v6/pp_ocr_v6.cpp
    "Explicit param_copy=false"
)
_apply_patch_once(
    ${_pd}/pp_ocr_v6_param_copy_false.patch
    ${_mc}/espressif__pp_ocr_v6/pp_ocr_v6.cpp
    "Explicit param_copy=false"
)
