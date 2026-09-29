cmake_minimum_required(VERSION 3.22)
project(XWAQuestM2Host LANGUAGES C CXX)
if(NOT WIN32 OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Host tools must run natively on Windows x64")
endif()
get_filename_component(M2_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(OPENXWA_ROOT "${M2_ROOT}/../.." ABSOLUTE)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${M2_ROOT}/host/bin")
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory("${OPENXWA_ROOT}/third_party/SDL3" SDL3)

set(SPIRV_CROSS_STATIC ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${M2_ROOT}/deps/SDL_shadercross/external/SPIRV-Cross" spirv-cross)
add_library(spirv_cross_c ALIAS spirv-cross-c)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_VENDORED OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_INSTALL OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_SHARED ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory("${M2_ROOT}/deps/SDL_shadercross" shadercross)

set(AERON_HOST_TOOLS_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory("${OPENXWA_ROOT}/aeron" aeron)
# DXC is a host-only dependency of shadercross, never part of the Android APK.
add_custom_target(m2_host_runtime ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${M2_ROOT}/deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/windows/bin/x64/dxcompiler.dll"
        "${M2_ROOT}/deps/SDL_shadercross/external/DirectXShaderCompiler-binaries/windows/bin/x64/dxil.dll"
        "${M2_ROOT}/host/bin"
    DEPENDS shadercross aeron_fsr3_shadergen)
