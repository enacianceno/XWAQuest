cmake_minimum_required(VERSION 3.22)
project(XWAQuestM2 LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "M2 target requires Android arm64-v8a")
endif()
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR "This reproducible M2 configuration uses Windows x64 host tools")
endif()
get_filename_component(OPENXWA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${OPENXWA_ROOT}/third_party/SDL3" rev-parse HEAD
    OUTPUT_VARIABLE M2_SDL_REV OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT M2_SDL_REV STREQUAL "8f8ed757bc94d2e097aab8c4c0f58b57dcee9871")
    message(FATAL_ERROR "M2 requires the SDL revision verified in M1")
endif()
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# Only executables are host-side. Never search host library/package registries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_GPU_OPENXR ON CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
add_subdirectory("${OPENXWA_ROOT}/third_party/SDL3" SDL3)
set(SDL3_VERSION "3.5.0")

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/zstd/build/cmake zstd)
add_library(zstd::libzstd ALIAS libzstd_static)

set(M2_FFMPEG_PREFIX "${CMAKE_CURRENT_LIST_DIR}/prefix/android-arm64" CACHE PATH "Android FFmpeg prefix")
foreach(component avformat avcodec avutil swresample swscale)
    set(lib "${M2_FFMPEG_PREFIX}/lib/lib${component}.so")
    if(NOT EXISTS "${lib}")
        message(FATAL_ERROR "Build Android FFmpeg first: ${lib}")
    endif()
    add_library(m2_${component} SHARED IMPORTED GLOBAL)
    set_target_properties(m2_${component} PROPERTIES
        IMPORTED_LOCATION "${lib}"
        INTERFACE_INCLUDE_DIRECTORIES "${M2_FFMPEG_PREFIX}/include")
endforeach()
add_library(PkgConfig::FFMPEG_MEDIA INTERFACE IMPORTED GLOBAL)
set_target_properties(PkgConfig::FFMPEG_MEDIA PROPERTIES
    INTERFACE_LINK_LIBRARIES "m2_avformat;m2_avcodec;m2_avutil;m2_swresample")
add_library(PkgConfig::FFMPEG_VIDEO INTERFACE IMPORTED GLOBAL)
set_target_properties(PkgConfig::FFMPEG_VIDEO PROPERTIES INTERFACE_LINK_LIBRARIES m2_swscale)

set(XWA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(XWA_ENABLE_DEBUG_UI ON CACHE BOOL "" FORCE)
set(AERON_ENABLE_AUDIO_DECODE ON CACHE BOOL "" FORCE)
set(AERON_SHADER_FORMATS SPIRV CACHE STRING "" FORCE)
set(AERON_SHADERCROSS_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/host/bin/shadercross.exe" CACHE FILEPATH "")
set(AERON_FSR3_SHADERGEN_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/host/bin/aeron_fsr3_shadergen.exe" CACHE FILEPATH "")
add_subdirectory("${OPENXWA_ROOT}" openxwa)

# Force every application/engine object into the link, including paths not yet
# reachable from SDL_main. This prevents static-archive dead stripping from
# hiding unresolved engine subsystems in the M2 acceptance test.
set_property(TARGET xwa PROPERTY LINK_LIBRARIES "")
target_link_options(xwa PRIVATE "-Wl,--no-gc-sections" "-Wl,-Map=${CMAKE_BINARY_DIR}/OpenXWA.map")
target_link_libraries(xwa PRIVATE "-Wl,--whole-archive"
    xwa_core xwa_remaster xwa_2d_formats aeron aeron_compat aeron_video
    aeron_audio_decode aeron_ffmpeg_vfs aeron_scene aeron_asset
    aeron_fsr3 aeron_fsr3_host aeron_imgui aeron_yaml aeron_cgltf aeron_cjson
    lucas_opt opt2gltf_lib aeron_gltf_cook_lib imgbake
    "-Wl,--no-whole-archive" log)
