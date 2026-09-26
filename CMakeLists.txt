# retip - Viva Pinata: Trouble in Paradise (Xbox 360), ReXGlue recompilation
#
# Configured for upstream ReXGlue SDK 0.10.0.8-dev.g1406e1b. SDK boilerplate lives in
# generated/rexglue.cmake (from `rexglue init`, 0.10).

cmake_minimum_required(VERSION 3.25)
project(retip LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- ReXGlue SDK: fetch the prebuilt release into rexglue/<platform> ----------
set(REXSDK_VERSION "0.10.0.8-dev.g1406e1b")
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set(REXSDK_PLATFORM "win-amd64")
elseif(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^ARM64$")
    message(FATAL_ERROR "win-arm64 ReXGlue SDK release is not published")
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(REXSDK_PLATFORM "mac-arm64")
elseif(APPLE)
    set(REXSDK_PLATFORM "mac-amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(REXSDK_PLATFORM "linux-arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(REXSDK_PLATFORM "linux-amd64")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_PROCESSOR}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND}
            "-DREXSDK_VERSION='${REXSDK_VERSION}'"
            "-DREXSDK_PLATFORM=${REXSDK_PLATFORM}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fetch-rexglue-sdk.cmake")
# generated/rexglue.cmake uses REXSDK_VERSION as an optional EXACT pin; clear it
# so find_package accepts any 0.10.x found on CMAKE_PREFIX_PATH.
set(REXSDK_VERSION "")

# --- First configure on a fresh clone: run codegen once ----------------------
# generated/default/sources.cmake only exists after `rexglue codegen`. Without
# it the recomp target is not created on the first configure. assets/default.xex
# must be unpacked before this runs (see README).
set(RETIP_GENERATED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/generated/default/sources.cmake")
if(NOT EXISTS "${RETIP_GENERATED_SOURCES}")
    set(RETIP_REXGLUE_EXE "${CMAKE_CURRENT_SOURCE_DIR}/rexglue/${REXSDK_PLATFORM}/bin/rexglue${CMAKE_EXECUTABLE_SUFFIX}")
    if(EXISTS "${RETIP_REXGLUE_EXE}")
        message(STATUS "[retip] generated/default/sources.cmake missing - running codegen once at configure time")
        execute_process(
            COMMAND "${RETIP_REXGLUE_EXE}" codegen "${CMAKE_CURRENT_SOURCE_DIR}/retip_manifest.toml"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE RETIP_CODEGEN_RESULT)
        if(NOT RETIP_CODEGEN_RESULT EQUAL 0)
            message(WARNING "[retip] bootstrap codegen failed (${RETIP_CODEGEN_RESULT}); "
                            "the build-time codegen step will report the errors")
        endif()
    endif()
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RETIP_GENERATED_SOURCES}")

include(generated/rexglue.cmake)

# --- Sources -----------------------------------------------------------------
set(RETIP_SOURCES
    src/main.cpp
    src/tip_engine/Compat.cpp
    src/tip_engine/CursorHooks.cpp
    src/tip_engine/hooks.cpp
    src/tip_engine/GraphicsHooks.cpp
    src/tip_engine/Overlays/Fps.cpp
    src/tip_engine/Overlays/LaunchMenu.cpp
    src/tip_engine/Overlays/QuitMenu.cpp
    src/tip_engine/Overlays/TiPTools.cpp
    src/tip_engine/Overlays/TiPTools/SpawnMenu.cpp
    src/tip_engine/Overlays/TiPTools/GraphicsMenu.cpp
    src/tip_engine/Overlays/TiPTools/UpscalingMenu.cpp
    src/tip_engine/Overlays/TiPTools/SettingsMenu.cpp
    src/tip_engine/Overlays/TiPTools/PinataMenu.cpp
    src/tip_engine/Overlays/TiPTools/PlantMenu.cpp
    src/tip_engine/Overlays/TiPTools/PlayerMenu.cpp
    src/tip_engine/Overlays/TiPTools/RuffianMenu.cpp
    src/tip_engine/Overlays/TiPTools/ShovelMenu.cpp
    src/tip_engine/CRT/math.cpp
    src/tip_engine/CRT/assetman.cpp
)

if(WIN32)
    list(APPEND RETIP_SOURCES icon/app.rc)
    add_executable(retip WIN32 ${RETIP_SOURCES})
else()
    add_executable(retip ${RETIP_SOURCES})
endif()

rexglue_setup_target(retip GPU_PLUGINS xenos)

target_compile_definitions(retip PRIVATE _CRT_SECURE_NO_WARNINGS)

# --- Runtime files next to the exe -------------------------------------------
# packaged/ holds retip.toml (runtime config), retip/wallpaper.png (launcher)
# and mods/. Staged after every build; an existing retip.toml is kept because
# the launcher saves the player's settings into it.
add_custom_command(TARGET retip POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/packaged"
            "-DDST=$<TARGET_FILE_DIR:retip>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stage-packaged.cmake"
    COMMENT "Staging packaged/ (retip.toml, mods, launcher wallpaper) next to retip.exe")
