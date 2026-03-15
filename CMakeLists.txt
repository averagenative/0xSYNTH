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
)

target_include_directories(oxs_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
)

target_link_libraries(oxs_engine PUBLIC m)

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
