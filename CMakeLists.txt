cmake_minimum_required(VERSION 3.21)
project(MinecraftPE)

include(cmake/CPM.cmake)

# C++14 — set ONCE before any targets
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

include(cmake/EnumOption.cmake)

# Only Web (Emscripten) target is supported via CMake.
# Android is built via build.sh + NDK ndk-build.
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "This CMakeLists.txt is for the Web (Emscripten) target only.\n"
        "For Android, use build.sh with the NDK.\n"
        "Run:  emcmake cmake .. -G Ninja")
endif()

set(PLATFORM Web CACHE STRING "Platform to build for." FORCE)

# ── Emscripten-provided libs (via port flags, no system install needed) ──────
add_library(zlib INTERFACE IMPORTED)
set_target_properties(zlib PROPERTIES
    INTERFACE_LINK_OPTIONS "-sUSE_ZLIB=1"
)

add_library(png INTERFACE IMPORTED)
set_target_properties(png PROPERTIES
    INTERFACE_COMPILE_OPTIONS "-sUSE_LIBPNG=1"
    INTERFACE_LINK_OPTIONS   "-sUSE_LIBPNG=1"
)

add_library(glfw INTERFACE IMPORTED)
set_target_properties(glfw PROPERTIES
    INTERFACE_LINK_OPTIONS "-sUSE_GLFW=3"
)

# ── OpenAL is NOT used in web build (NO_SOUND is defined) ────────────────────
# OpenAL-soft 1.24+ requires clang-scan-deps for C++20 module scanning,
# which is NOT available in Emscripten's toolchain — causes code=127 crash.
# Since the web target compiles with NO_SOUND, we don't need OpenAL at all.
# We create a stub INTERFACE target so any accidental references link cleanly.
add_library(OpenAL::OpenAL INTERFACE IMPORTED GLOBAL)

# ── Source files ─────────────────────────────────────────────────────────────
file(GLOB_RECURSE CLIENT_SOURCES
    "src/client/*.cpp"
    "src/locale/*.cpp"
    "src/nbt/*.cpp"
    "src/network/*.cpp"
    "src/platform/*.cpp"
    "src/raknet/*.cpp"
    "src/server/*.cpp"
    "src/util/*.cpp"
    "src/world/*.cpp"
    "src/SharedConstants.cpp"
    "src/main.cpp"
    "src/NinecraftApp.cpp"
    "src/AppPlatform_glfw.cpp"
)

# Exclude Android-only and sound-system files from web build
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*SoundSystemSL\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*SoundSystemAL\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*main_android_java\\.cpp$")

add_executable(${PROJECT_NAME} ${CLIENT_SOURCES})

# ── Emscripten compile/link flags ────────────────────────────────────────────
# NO pthread/shared memory needed since we dropped OpenAL.
# Emscripten pthreads also require cross-origin isolation headers on the server,
# which makes deployment harder — skip it for the web target.

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Os"
    "-Wno-invalid-source-encoding"
    "-Wno-narrowing"
    "-Wno-deprecated-register"
    "-Wno-reserved-user-defined-literal"
)

target_link_options(${PROJECT_NAME} PRIVATE
    "-Os"
    "-sUSE_LIBPNG=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sLEGACY_GL_EMULATION=1"
    "-sGL_UNSAFE_OPTS=0"
    "-sEMULATE_FUNCTION_POINTER_CASTS=1"
    "-sALLOW_TABLE_GROWTH=1"
    "-sEXPORTED_RUNTIME_METHODS=['FS','stringToUTF8','UTF8ToString','cwrap','ccall','HEAP8','HEAPU8','HEAP32','HEAPU32']"
    "-sEXPORTED_FUNCTIONS=['_main']"
    "--preload-file=${CMAKE_SOURCE_DIR}/data@/data"
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-sASSERTIONS=2"
        "-sSTACK_OVERFLOW_CHECK=2"
        "-sSTACK_SIZE=5242880"
        "-sGL_DEBUG=1"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "__EMSCRIPTEN__"
    "NO_SOUND"
    "NO_NETWORK"
    "PLATFORM_WEB"
    "OPENGL_ES"
    "NO_EGL"
    "Web"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/glad/include/"
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    zlib
    png
    glfw
    idbfs.js
)

# ── Post-build: copy index.html next to the built files ─────────────────────
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/misc/web/index.html"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying index.html to build dir"
)

message(STATUS "Configured MinecraftPE for Web (Emscripten) — NO_SOUND mode")
