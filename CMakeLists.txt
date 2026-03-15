cmake_minimum_required(VERSION 3.16)
project(0xSYNTH VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(OXS_PLATFORM_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(OXS_PLATFORM_MACOS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(OXS_PLATFORM_WINDOWS)
endif()

# Sanitizer options
option(SANITIZE_THREADS "Enable ThreadSanitizer" OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer" OFF)

if(SANITIZE_THREADS)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

if(SANITIZE_ADDRESS)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# =============================================================================
# Engine static library
# =============================================================================
add_library(oxs_engine STATIC
    src/engine/params.c
    src/engine/command_queue.c
    src/engine/output_events.c
    src/engine/oscillator.c
    src/engine/envelope.c
    src/engine/filter.c
    src/engine/voice.c
    src/engine/fm.c
    src/engine/wavetable.c
    src/engine/effects.c
    src/engine/preset.c
)

target_include_directories(oxs_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

target_link_libraries(oxs_engine PUBLIC m cjson)

# =============================================================================
# API library (bridges public API to engine internals)
# =============================================================================
add_library(oxs_api STATIC
    src/api/synth_api.c
)

target_include_directories(oxs_api PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
)

target_link_libraries(oxs_api PUBLIC oxs_engine)

# =============================================================================
# Dependencies (header-only, vendored)
# =============================================================================
# cJSON
add_library(cjson STATIC deps/cJSON.c)
target_include_directories(cjson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deps)

# =============================================================================
# Standalone executable
# =============================================================================
add_executable(oxs_standalone
    src/standalone/main.c
    src/standalone/audio.c
    src/standalone/midi.c
)

target_link_libraries(oxs_standalone PRIVATE oxs_api)
target_include_directories(oxs_standalone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

# miniaudio needs pthread and dl on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(oxs_standalone PRIVATE pthread dl)
endif()

set_target_properties(oxs_standalone PROPERTIES OUTPUT_NAME "0xsynth")

# GTK 4 GUI (optional)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(GTK4 gtk4)
endif()

if(GTK4_FOUND)
    message(STATUS "GTK4 found: ${GTK4_VERSION}")

    # UI abstraction library (toolkit-agnostic layout)
    add_library(oxs_ui STATIC src/ui/ui_layout.c)
    target_include_directories(oxs_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_ui PUBLIC oxs_engine)

    # GTK app library
    add_library(oxs_gtk STATIC src/gui_gtk/gtk_app.c)
    target_include_directories(oxs_gtk PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_gtk PUBLIC oxs_api oxs_ui)
    target_include_directories(oxs_gtk PUBLIC ${GTK4_INCLUDE_DIRS})
    target_link_libraries(oxs_gtk PUBLIC ${GTK4_LIBRARIES})
    target_compile_options(oxs_gtk PUBLIC ${GTK4_CFLAGS_OTHER})

    # Link GTK into standalone
    target_link_libraries(oxs_standalone PRIVATE oxs_gtk)
    target_compile_definitions(oxs_standalone PRIVATE OXS_HAS_GTK)
else()
    message(STATUS "GTK4 not found — building without GUI")
    # Still build UI layout for testing
    add_library(oxs_ui STATIC src/ui/ui_layout.c)
    target_include_directories(oxs_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_ui PUBLIC oxs_engine)
endif()

# =============================================================================
# Plugin targets (CLAP + VST3 via CPLUG)
# =============================================================================
option(BUILD_PLUGINS "Build CLAP and VST3 plugins" ON)

if(BUILD_PLUGINS)
    set(PLUGIN_SOURCES src/plugin/plugin.c)
    set(CPLUG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/cplug)
    set(PLUGIN_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/config.h)

    # Common plugin compile options
    set(PLUGIN_COMPILE_FLAGS
        -include ${PLUGIN_CONFIG}
    )

    # CLAP plugin (.clap)
    add_library(oxs_clap MODULE
        ${PLUGIN_SOURCES}
        ${CPLUG_DIR}/cplug_clap.c
    )
    target_link_libraries(oxs_clap PRIVATE oxs_api)
    target_include_directories(oxs_clap PRIVATE
        ${CPLUG_DIR}
        ${CPLUG_DIR}/clap
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(oxs_clap PRIVATE ${PLUGIN_COMPILE_FLAGS})
    set_target_properties(oxs_clap PROPERTIES
        OUTPUT_NAME "0xSYNTH"
        PREFIX ""
        SUFFIX ".clap"
    )

    # VST3 plugin (.vst3)
    add_library(oxs_vst3 MODULE
        ${PLUGIN_SOURCES}
        ${CPLUG_DIR}/cplug_vst3.c
    )
    target_link_libraries(oxs_vst3 PRIVATE oxs_api)
    target_include_directories(oxs_vst3 PRIVATE
        ${CPLUG_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_options(oxs_vst3 PRIVATE ${PLUGIN_COMPILE_FLAGS})
    set_target_properties(oxs_vst3 PROPERTIES
        OUTPUT_NAME "0xSYNTH"
        PREFIX ""
        SUFFIX ".vst3"
    )
endif()

# =============================================================================
# Tests
# =============================================================================
enable_testing()

add_executable(oxs_test_foundation tests/test_foundation.c)
target_link_libraries(oxs_test_foundation PRIVATE oxs_api)
add_test(NAME foundation COMMAND oxs_test_foundation)

add_executable(oxs_test_subtractive tests/test_subtractive.c)
target_link_libraries(oxs_test_subtractive PRIVATE oxs_api)
add_test(NAME subtractive COMMAND oxs_test_subtractive)

add_executable(oxs_test_fm tests/test_fm.c)
target_link_libraries(oxs_test_fm PRIVATE oxs_api)
add_test(NAME fm COMMAND oxs_test_fm)

add_executable(oxs_test_wavetable tests/test_wavetable.c)
target_link_libraries(oxs_test_wavetable PRIVATE oxs_api)
add_test(NAME wavetable COMMAND oxs_test_wavetable)

add_executable(oxs_test_effects tests/test_effects.c)
target_link_libraries(oxs_test_effects PRIVATE oxs_api)
add_test(NAME effects COMMAND oxs_test_effects)

add_executable(oxs_test_presets tests/test_presets.c)
target_link_libraries(oxs_test_presets PRIVATE oxs_api)
add_test(NAME presets COMMAND oxs_test_presets)

add_executable(oxs_test_ui_layout tests/test_ui_layout.c)
target_link_libraries(oxs_test_ui_layout PRIVATE oxs_ui oxs_engine)
add_test(NAME ui_layout COMMAND oxs_test_ui_layout)
