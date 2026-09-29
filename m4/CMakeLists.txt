cmake_minimum_required(VERSION 3.22)
project(XWAQuestM4 LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "M4 requires Android arm64-v8a")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(M2 "${CMAKE_CURRENT_LIST_DIR}/../m2" ABSOLUTE)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)

# Compile the real application entry point with a narrow M4 stop policy.
# Original sources and the M2 archives stay untouched. Fail if anchors drift.
file(READ "${ROOT}/src/xwa_app/main.c" MAIN)
function(replace_once old new)
    string(FIND "${MAIN}" "${old}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "M4 instrumentation anchor missing: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" MAIN "${MAIN}")
    set(MAIN "${MAIN}" PARENT_SCOPE)
endfunction()
replace_once("int main(int argc, char** argv) {" "int main(int argc, char** argv) {\n\tSDL_Log(\"M4 SDL_main ENTER: real OpenXWA application startup\");")
replace_once("\t\tif (!setup_cancelled) {\n\t\t\tAeron_LogError(\"xwa.config\", \"%s\", config_error);\n\t\t\tAeron_FatalError(\"OpenXWA\", config_error);\n\t\t}"
"\t\tSDL_Log(\"M4 STOP GameData validation rejected: %s; cancelled=%d\", config_error, setup_cancelled);")
replace_once("\texit_code = 0;"
"\tSDL_Log(\"M4 STOP XwaPort_Init succeeded; no game loop authorized\");\n\tXwaRemaster_Shutdown();\n\tXwaPort_Shutdown();\n\tAeron_Shutdown();\n\treturn 0;\n\texit_code = 0;")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/main-m4.c" "#include <SDL3/SDL.h>\n${MAIN}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ROOT}/src/xwa_app/main.c")
add_library(OpenXWAM4 SHARED "${CMAKE_CURRENT_BINARY_DIR}/main-m4.c" trace.c
    "${ROOT}/src/xwa_app/host_config.c" "${ROOT}/src/xwa_app/setup.c")
target_compile_definitions(OpenXWAM4 PRIVATE XWA_MODERN AERON_DEBUG_UI=1 YAML_DECLARE_STATIC)
target_include_directories(OpenXWAM4 PRIVATE "${ROOT}/src" "${ROOT}/src/xwa_app"
    "${ROOT}/aeron/include" "${ROOT}/aeron/src" "${ROOT}/aeron/third_party/imgui"
    "${ROOT}/third_party/SDL3/include" "${M2}/build-android/SDL3/include-revision")
set_target_properties(OpenXWAM4 PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 99)
target_link_options(OpenXWAM4 PRIVATE "-Wl,--no-undefined" "-Wl,--no-gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/OpenXWAM4.map")
foreach(fn SDL_Init Aeron_Init Aeron_WindowInit Aeron_RenderBackendInit Aeron_AudioInit
    Aeron_ControllersInit Aeron_InitVfs Aeron_DebugUiInitInternal XwaHostConfig_Load
    XwaSetup_ValidateGameData Aeron_Shutdown XwaRemaster_Init XwaPort_Init AeronVfs_Exists AeronVfs_Open)
    target_link_options(OpenXWAM4 PRIVATE "-Wl,--wrap=${fn}")
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
target_link_libraries(OpenXWAM4 PRIVATE "-Wl,--whole-archive" ${ARCHIVES} "-Wl,--no-whole-archive"
    "${M2}/build-android/zstd/lib/libzstd.a" "${M2}/build-android/SDL3/libSDL3.so")
foreach(name avformat avcodec avutil swresample swscale)
    target_link_libraries(OpenXWAM4 PRIVATE "${M2}/prefix/android-arm64/lib/lib${name}.so")
endforeach()
target_link_libraries(OpenXWAM4 PRIVATE log android m dl)


