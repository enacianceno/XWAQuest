cmake_minimum_required(VERSION 3.22)
project(XWAQuestM8 LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "M8 requires Android arm64-v8a")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(M2 "${CMAKE_CURRENT_LIST_DIR}/../m2" ABSOLUTE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
add_library(OpenXWAM8 SHARED main.c frame_bridge.c input_xr.c xr_bridge.c
    vr_flight_bridge.c vr_flight_renderer.c diagnostics.c
    "${CMAKE_CURRENT_LIST_DIR}/../vr-probe/vr_convert.c"
    "${CMAKE_CURRENT_LIST_DIR}/../vr-probe/vr_blit.c"
    "${ROOT}/src/xwa_app/host_config.c" "${ROOT}/src/xwa_app/setup.c")
target_compile_definitions(OpenXWAM8 PRIVATE XWA_MODERN AERON_DEBUG_UI=1 YAML_DECLARE_STATIC)
target_include_directories(OpenXWAM8 PRIVATE "${ROOT}/src" "${ROOT}/src/xwa_app"
    "${ROOT}/aeron/include" "${ROOT}/aeron/src" "${ROOT}/aeron/third_party/imgui"
    "${ROOT}/third_party/SDL3/include" "${M2}/build-android/SDL3/include-revision"
    "${CMAKE_CURRENT_LIST_DIR}/../vr-probe")
set_target_properties(OpenXWAM8 PROPERTIES LINKER_LANGUAGE CXX C_STANDARD 99)
target_compile_options(OpenXWAM8 PRIVATE -Werror=implicit-function-declaration -Werror=incompatible-pointer-types)
target_link_options(OpenXWAM8 PRIVATE "-Wl,--no-undefined" "-Wl,--no-gc-sections"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/OpenXWAM8.map")
foreach(fn Aeron_Init Aeron_BeginFrame Aeron_Present Aeron_WaitForNextFrame Aeron_Shutdown Aeron_SetOutputHdr
    SDL_WaitForGPUSwapchain SDL_WaitAndAcquireGPUSwapchainTexture XwaPort_Tick
    XwaRemaster_Frame XwaFlightTask_Init Mission_Init FrontendDialog_PromptForPilotName
    CombatSimMenu_Update CombatSimMenu_Exit MissionSetup_Update MissionSetup_Exit
    Movie_Play FrontImage_RegisterResourceDefault FrontImage_FreeResourceByName
    FrontImage_LoadResourceList Mem_Alloc)
    target_link_options(OpenXWAM8 PRIVATE "-Wl,--wrap=${fn}")
endforeach()
# Link the same complete game/remaster/audio archives used by M7B, read-only.
set(ARCHIVES "${CMAKE_CURRENT_LIST_DIR}/../vr-probe/staging/libaeron_vrprobe.a")
foreach(name xwa_core xwa_remaster xwa_2d_formats)
    list(APPEND ARCHIVES "${M2}/build-android/openxwa/lib${name}.a")
endforeach()
foreach(name aeron_compat aeron_video aeron_audio_decode aeron_ffmpeg_vfs aeron_scene
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
target_link_libraries(OpenXWAM8 PRIVATE "-Wl,--whole-archive" ${ARCHIVES} "-Wl,--no-whole-archive"
    "${M2}/build-android/zstd/lib/libzstd.a" "${M2}/build-android/SDL3/libSDL3.so")
foreach(name avformat avcodec avutil swresample swscale)
    target_link_libraries(OpenXWAM8 PRIVATE "${M2}/prefix/android-arm64/lib/lib${name}.so")
endforeach()
target_link_libraries(OpenXWAM8 PRIVATE log android m dl)
