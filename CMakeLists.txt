cmake_minimum_required(VERSION 3.21)
project(MinecraftPE)

include(cmake/CPM.cmake)

# C++14 everywhere — must be set BEFORE add_executable, not overridden after
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

find_package(Threads REQUIRED)

# ── Emscripten-provided libs (linked via flags, not system install) ──────────
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

# ── OpenAL-soft (static, fetched via CPM) ───────────────────────────────────
CPMAddPackage(
    NAME "openal"
    GIT_REPOSITORY "https://github.com/kcat/openal-soft.git"
    GIT_TAG "1.25.1"
    OPTIONS
        "ALSOFT_EXAMPLES OFF"
        "ALSOFT_TESTS OFF"
        "ALSOFT_UTILS OFF"
        "LIBTYPE STATIC"
        "ALSOFT_ENABLE_MODULES OFF"
        "ALSOFT_STATIC_STDCXX ON"
        "ALSOFT_STATIC_LIBGCC ON"
)

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

# Exclude Android-only platform files from web build
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*SoundSystemSL\\.cpp$")
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*main_android_java\\.cpp$")

add_executable(${PROJECT_NAME} ${CLIENT_SOURCES})

# ── Emscripten compile/link flags ────────────────────────────────────────────
# pthread + shared memory required for OpenAL-soft
set(EM_THREAD_FLAGS "-pthread -sUSE_PTHREADS=1 -sSHARED_MEMORY=1")

target_compile_options(${PROJECT_NAME} PRIVATE
    "-Os"
    "-Wno-invalid-source-encoding"
    "-Wno-narrowing"
    "-Wno-deprecated-register"
    "-Wno-reserved-user-defined-literal"
    ${EM_THREAD_FLAGS}
)

target_link_options(${PROJECT_NAME} PRIVATE
    "-Os"
    ${EM_THREAD_FLAGS}
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
    "${openal_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    zlib
    png
    glfw
    OpenAL::OpenAL
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

message(STATUS "Configured MinecraftPE for Web (Emscripten)")
