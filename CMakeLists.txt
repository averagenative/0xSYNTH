cmake_minimum_required(VERSION 3.16)
project(0xSYNTH VERSION 0.1.0 LANGUAGES C CXX)

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
    src/engine/sampler.c
    src/engine/dr_libs_impl.c
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

# ImGui + SDL2 GUI (preferred — zero external deps on macOS/Windows)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()

if(SDL2_FOUND)
    message(STATUS "SDL2 found: ${SDL2_VERSION} — building ImGui GUI")

    # ImGui library (C++)
    add_library(imgui STATIC
        deps/imgui/imgui.cpp
        deps/imgui/imgui_draw.cpp
        deps/imgui/imgui_tables.cpp
        deps/imgui/imgui_widgets.cpp
        deps/imgui/backends/imgui_impl_sdl2.cpp
        deps/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/backends
        ${SDL2_INCLUDE_DIRS}
    )
    target_compile_features(imgui PUBLIC cxx_std_17)
    set_target_properties(imgui PROPERTIES LINKER_LANGUAGE CXX)

    # UI abstraction library
    add_library(oxs_ui STATIC src/ui/ui_layout.c)
    target_include_directories(oxs_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_ui PUBLIC oxs_engine)

    # ImGui app library
    add_library(oxs_imgui STATIC src/gui_imgui/imgui_app.cpp)
    target_include_directories(oxs_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_imgui PUBLIC oxs_api oxs_ui imgui ${SDL2_LIBRARIES})
    target_include_directories(oxs_imgui PUBLIC ${SDL2_INCLUDE_DIRS})
    find_package(OpenGL REQUIRED)
    target_link_libraries(oxs_imgui PUBLIC OpenGL::GL)

    # Link ImGui into standalone
    target_link_libraries(oxs_standalone PRIVATE oxs_imgui)
    target_compile_definitions(oxs_standalone PRIVATE OXS_HAS_IMGUI)
    set_target_properties(oxs_standalone PROPERTIES LINKER_LANGUAGE CXX)
else()
    message(STATUS "SDL2 not found — building without ImGui GUI")
    # Still build UI layout for testing
    add_library(oxs_ui STATIC src/ui/ui_layout.c)
    target_include_directories(oxs_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(oxs_ui PUBLIC oxs_engine)
endif()

# GTK 4 GUI (optional — Linux only alternative, only if ImGui not available)
if(NOT SDL2_FOUND AND PkgConfig_FOUND)
    pkg_check_modules(GTK4 gtk4)
endif()

if(GTK4_FOUND AND NOT SDL2_FOUND)
    message(STATUS "GTK4 found: ${GTK4_VERSION}")

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
    message(STATUS "GTK4 not found — building without GTK GUI")
endif()

# Ensure oxs_ui exists for tests even without any GUI
if(NOT TARGET oxs_ui)
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

add_executable(oxs_test_sampler tests/test_sampler.c)
target_link_libraries(oxs_test_sampler PRIVATE oxs_api)
add_test(NAME sampler COMMAND oxs_test_sampler)

add_executable(oxs_test_midi_cc tests/test_midi_cc.c)
target_link_libraries(oxs_test_midi_cc PRIVATE oxs_api)
add_test(NAME midi_cc COMMAND oxs_test_midi_cc)

add_executable(oxs_benchmark tests/test_benchmark.c)
target_link_libraries(oxs_benchmark PRIVATE oxs_api)
add_test(NAME benchmark COMMAND oxs_benchmark)

add_executable(oxs_test_preset_roundtrip tests/test_preset_roundtrip.c)
target_link_libraries(oxs_test_preset_roundtrip PRIVATE oxs_api)
add_test(NAME preset_roundtrip COMMAND oxs_test_preset_roundtrip)
