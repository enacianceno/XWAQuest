cmake_minimum_required(VERSION 3.22)
project(XWAQuestM5 LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "M5 requires Android arm64-v8a")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(M2 "${CMAKE_CURRENT_LIST_DIR}/../m2" ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)

# Compile the real application entry point with a narrow M5 stop policy.
# Original sources and the M2 archives stay untouched. Fail if anchors drift.
file(READ "${ROOT}/src/xwa_app/main.c" MAIN)
function(replace_once old new)
    string(FIND "${MAIN}" "${old}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "M5 instrumentation anchor missing: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" MAIN "${MAIN}")
    set(MAIN "${MAIN}" PARENT_SCOPE)
endfunction()
replace_once("int main(int argc, char** argv) {" "int main(int argc, char** argv) {\n\tSDL_Log(\"M5 SDL_main ENTER: real OpenXWA application startup\");")
replace_once("\t\tif (!setup_cancelled) {\n\t\t\tAeron_LogError(\"xwa.config\", \"%s\", config_error);\n\t\t\tAeron_FatalError(\"OpenXWA\", config_error);\n\t\t}"
"\t\tSDL_Log(\"M5 STOP GameData validation rejected: %s; cancelled=%d\", config_error, setup_cancelled);")
replace_once("\texit_code = 0;" "\tUint64 m5_start = SDL_GetTicks();\n\texit_code = 0;")
replace_once("\t\t\tconst int32_t delta_us = Aeron_BeginFrame();"
"\t\t\tif (SDL_GetTicks() - m5_start > 120000 || XwaFlightTask_IsActive()) {\n\t\t\t\tSDL_Log(\"M5 STOP observation limit or flight boundary\");\n\t\t\t\tbreak;\n\t\t\t}\n\t\t\tconst int32_t delta_us = Aeron_BeginFrame();")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main-m5.c" "#include <SDL3/SDL.h>\n#include \"xwa_runtime/runtime/flight_task.h\"\n${MAIN}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ROOT}/src/xwa_app/main.c")
add_library(OpenXWAM5 SHARED "${CMAKE_CURRENT_BINARY_DIR}/main-m5.c" trace.c
    "${ROOT}/src/xwa_app/host_config.c" "${ROOT}/src/xwa_app/setup.c")
target_compile_definitions(OpenXWAM5 PRIVATE XWA_MODERN AERON_DEBUG_UI=1 YAML_DECLARE_STATIC)
target_include_directories(OpenXWAM5 PRIVATE "${ROOT}/src" "${ROOT}/src/xwa_app"
    "${ROOT}/aeron/include" "${ROOT}/aeron/src" "${ROOT}/aeron/third_party/imgui"
    "${ROOT}/third_party/SDL3/include" "${M2}/build-android/SDL3/include-revision")
set_target_properties(OpenXWAM5 PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 99)
target_link_options(OpenXWAM5 PRIVATE "-Wl,--no-undefined" "-Wl,--no-gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/OpenXWAM5.map")
foreach(fn SDL_Init Aeron_Init Aeron_WindowInit Aeron_RenderBackendInit Aeron_AudioInit
    Aeron_ControllersInit Aeron_InitVfs Aeron_DebugUiInitInternal XwaHostConfig_Load
    XwaSetup_ValidateGameData Aeron_Shutdown XwaRemaster_Init XwaPort_Init AeronVfs_Exists AeronVfs_Open XwaPort_Tick Aeron_Present Movie_Play XwaRemaster_Frame Aeron_VideoUpdate)
    target_link_options(OpenXWAM5 PRIVATE "-Wl,--wrap=${fn}")
endforeach()
# Reuse the complete real M2 engine, retaining every subsystem in the link.
set(ARCHIVES)
foreach(name xwa_core xwa_remaster xwa_2d_formats)
    list(APPEND ARCHIVES "${M2}/build-android/openxwa/lib${name}.a")
endforeach()
foreach(name aeron aeron_compat aeron_video aeron_audio_decode aeron_ffmpeg_vfs aeron_scene
    aeron_asset aeron_fsr3 aeron_fsr3_host aeron_imgui aeron_yaml aeron_cgltf aeron_cjson lucas_opt opt2gltf_lib)
    list(APPEND ARCHIVES "${M2}/build-android/openxwa/aeron/lib${name}.a")
endforeach()
list(APPEND ARCHIVES "${M2}/build-android/openxwa/aeron/tools/gltf_cook/libaeron_gltf_cook_lib.a"
    "${M2}/build-android/openxwa/aeron/tools/imgbake/libimgbake.a")
foreach(lib IN LISTS ARCHIVES)
    if(NOT EXISTS "${lib}")
        message(FATAL_ERROR "Build approved M2 first: ${lib}")
    endif()
endforeach()
target_link_libraries(OpenXWAM5 PRIVATE "-Wl,--whole-archive" ${ARCHIVES} "-Wl,--no-whole-archive"
    "${M2}/build-android/zstd/lib/libzstd.a" "${M2}/build-android/SDL3/libSDL3.so")
foreach(name avformat avcodec avutil swresample swscale)
    target_link_libraries(OpenXWAM5 PRIVATE "${M2}/prefix/android-arm64/lib/lib${name}.so")
endforeach()
target_link_libraries(OpenXWAM5 PRIVATE log android m dl)






