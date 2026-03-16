set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# SDL2 MinGW — static link (no SDL2.dll needed at runtime)
set(SDL2_MINGW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/SDL2-2.30.12/x86_64-w64-mingw32)
set(SDL2_INCLUDE_DIRS ${SDL2_MINGW_DIR}/include/SDL2 ${SDL2_MINGW_DIR}/include)
# No libSDL2main.a — that's for executables only, breaks DLL plugins
set(SDL2_LIBRARIES
    ${SDL2_MINGW_DIR}/lib/libSDL2.a
    imm32 version winmm setupapi gdi32 ole32 oleaut32 uuid
)
set(SDL2_FOUND TRUE)

# OpenGL on Windows
set(OPENGL_FOUND TRUE)
set(OPENGL_gl_LIBRARY opengl32)
