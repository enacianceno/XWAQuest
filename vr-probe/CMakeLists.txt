cmake_minimum_required(VERSION 3.22)
project(XWAQuestVrProbe LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "vr-probe requires Android arm64-v8a")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(M2 "${CMAKE_CURRENT_LIST_DIR}/../m2" ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)

# Standalone VR probe entry point (vr_main.c provides SDL_main).
# Original engine sources and M2 archives stay untouched.
add_library(OpenXWAVRP SHARED vr_main.c vr_openxr.c vr_stereo.c vr_blit.c vr_framing.c vr_convert.c vr_audit.c vr_readback.c vr_trace.c
    "${ROOT}/src/xwa_app/host_config.c" "${ROOT}/src/xwa_app/setup.c")
target_compile_definitions(OpenXWAVRP PRIVATE XWA_MODERN AERON_DEBUG_UI=1 YAML_DECLARE_STATIC)
target_include_directories(OpenXWAVRP PRIVATE "${CMAKE_CURRENT_LIST_DIR}"
    "${ROOT}/src" "${ROOT}/src/xwa_app"
    "${ROOT}/aeron/include" "${ROOT}/aeron/src" "${ROOT}/aeron/third_party/imgui"
    "${ROOT}/third_party/SDL3/include" "${M2}/build-android/SDL3/include-revision")
set_target_properties(OpenXWAVRP PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 99)
target_link_options(OpenXWAVRP PRIVATE "-Wl,--no-undefined" "-Wl,--no-gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/OpenXWAVRP.map")
foreach(fn SDL_Init Aeron_RenderBackendInit XwaHostConfig_Load
    XwaSetup_ValidateGameData XwaRemaster_Init XwaPort_Init Aeron_Present ModelPreview_LoadModel)
    target_link_options(OpenXWAVRP PRIVATE "-Wl,--wrap=${fn}")
endforeach()
# Reuse the complete real M2 engine, retaining every subsystem in the link.
# Exception: aeron comes from the frozen VR-PROBE staging copy (identical to
# approved M2 libaeron.a plus the documented XR opt-in block), so m2/ outputs
# are never touched again. See evidence/original-libaeron.a + shared patch.
set(ARCHIVES)
foreach(name xwa_core xwa_remaster xwa_2d_formats)
    list(APPEND ARCHIVES "${M2}/build-android/openxwa/lib${name}.a")
endforeach()
list(APPEND ARCHIVES "${CMAKE_CURRENT_LIST_DIR}/staging/libaeron_vrprobe.a")
foreach(name aeron_compat aeron_video aeron_audio_decode aeron_ffmpeg_vfs aeron_scene
    aeron_asset aeron_fsr3 aeron_fsr3_host aeron_imgui aeron_yaml aeron_cgltf aeron_cjson lucas_opt opt2gltf_lib)
    list(APPEND ARCHIVES "${M2}/build-android/openxwa/aeron/lib${name}.a")
endforeach()
list(APPEND ARCHIVES "${M2}/build-android/openxwa/aeron/tools/gltf_cook/libaeron_gltf_cook_lib.a"
    "${M2}/build-android/openxwa/aeron/tools/imgbake/libimgbake.a")
foreach(lib IN LISTS ARCHIVES)
    if(NOT EXISTS "${lib}")
        message(FATAL_ERROR "Build approved M2 first (or vr-probe staging copy missing): ${lib}")
    endif()
endforeach()
target_link_libraries(OpenXWAVRP PRIVATE "-Wl,--whole-archive" ${ARCHIVES} "-Wl,--no-whole-archive"
    "${M2}/build-android/zstd/lib/libzstd.a" "${M2}/build-android/SDL3/libSDL3.so")
foreach(name avformat avcodec avutil swresample swscale)
    target_link_libraries(OpenXWAVRP PRIVATE "${M2}/prefix/android-arm64/lib/lib${name}.so")
endforeach()
target_link_libraries(OpenXWAVRP PRIVATE log android m dl)
