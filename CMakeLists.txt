cmake_minimum_required(VERSION 3.21)
project(VoxelForge)

include(cmake/CPM.cmake)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

include(cmake/EnumOption.cmake)

if(EMSCRIPTEN)
    set(PLATFORM Web CACHE STRING "Platform to build for.")
endif()

enum_option(PLATFORM "Web" "Platform to build for.")

find_package(Threads REQUIRED)

# Web platform libs
set(PNG_LIB png)
set(AL_LIBTYPE "STATIC")

add_library(zlib INTERFACE IMPORTED)
set_target_properties(zlib PROPERTIES
    INTERFACE_LINK_OPTIONS "-sUSE_ZLIB=1"
)

add_library(png INTERFACE IMPORTED)
set_target_properties(png PROPERTIES
    INTERFACE_COMPILE_OPTIONS "-sUSE_LIBPNG=1"
    INTERFACE_LINK_OPTIONS "-sUSE_LIBPNG=1"
)

add_library(glfw INTERFACE IMPORTED)
set_target_properties(glfw PROPERTIES
    INTERFACE_LINK_OPTIONS "-sUSE_GLFW=3"
)

CPMAddPackage(
    NAME "openal"
    GIT_REPOSITORY "https://github.com/kcat/openal-soft.git"
    GIT_TAG "1.25.1"
    OPTIONS
        "ALSOFT_EXAMPLES OFF"
        "ALSOFT_TESTS OFF"
        "ALSOFT_UTILS OFF"
        "LIBTYPE ${AL_LIBTYPE}"
        "ALSOFT_ENABLE_MODULES OFF"
        "ALSOFT_STATIC_STDCXX ON"
        "ALSOFT_STATIC_LIBGCC ON"
)

file(GLOB CLIENT_SOURCES
    "src/client/*.cpp"
    "src/client/gamemode/*.cpp"
    "src/client/gui/*.cpp"
    "src/client/gui/components/*.cpp"
    "src/client/gui/screens/*.cpp"
    "src/client/gui/screens/crafting/*.cpp"
    "src/client/gui/screens/touch/*.cpp"
    "src/client/model/*.cpp"
    "src/client/model/geom/*.cpp"
    "src/client/particle/*.cpp"
    "src/client/player/*.cpp"
    "src/client/player/input/*.cpp"
    "src/client/player/input/touchscreen/*.cpp"
    "src/client/renderer/*.cpp"
    "src/client/renderer/culling/*.cpp"
    "src/client/renderer/entity/*.cpp"
    "src/client/renderer/ptexture/*.cpp"
    "src/client/renderer/tileentity/*.cpp"
    "src/client/sound/*.cpp"
    "src/locale/*.cpp"
    "src/nbt/*.cpp"
    "src/platform/CThread.cpp"
    "src/platform/PngLoader.cpp"
    "src/platform/time.cpp"
    "src/platform/audio/SoundSystemAL.cpp"
    "src/platform/input/*.cpp"
    "src/util/**.cpp"
    "src/world/*.cpp"
    "src/world/phys/*.cpp"
    "src/world/entity/*.cpp"
    "src/world/entity/ai/control/*.cpp"
    "src/world/entity/animal/*.cpp"
    "src/world/entity/item/*.cpp"
    "src/world/entity/monster/*.cpp"
    "src/world/entity/player/*.cpp"
    "src/world/entity/projectile/*.cpp"
    "src/world/food/*.cpp"
    "src/world/inventory/*.cpp"
    "src/world/item/*.cpp"
    "src/world/item/crafting/*.cpp"
    "src/world/level/*.cpp"
    "src/world/level/biome/*.cpp"
    "src/world/level/chunk/*.cpp"
    "src/world/level/dimension/*.cpp"
    "src/world/level/levelgen/*.cpp"
    "src/world/level/levelgen/feature/*.cpp"
    "src/world/level/levelgen/synth/*.cpp"
    "src/world/level/material/*.cpp"
    "src/world/level/pathfinder/*.cpp"
    "src/world/level/storage/*.cpp"
    "src/world/level/tile/*.cpp"
    "src/world/level/tile/entity/*.cpp"
    "src/SharedConstants.cpp"
    "src/main.cpp"
    "src/NinecraftApp.cpp"
    "src/AppPlatform_glfw.cpp"
)

add_executable(${PROJECT_NAME} ${CLIENT_SOURCES})

set(CMAKE_CXX_STANDARD 11)
set(EM_FLAGS "-pthread -sUSE_PTHREADS=1 -sUSE_LIBPNG=1 -sSHARED_MEMORY=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EM_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EM_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EM_FLAGS} --preload-file ${CMAKE_SOURCE_DIR}/data@/data")
set(EXTRA_LIBS "idbfs.js")

target_compile_options(${PROJECT_NAME} PUBLIC
    "-Os"
    "-Wno-invalid-source-encoding"
    "-Wno-narrowing"
    "-Wno-deprecated-register"
    "-Wno-reserved-user-defined-literal"
)

target_link_options(${PROJECT_NAME} PUBLIC
    "-Os"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sFORCE_FILESYSTEM=1"
    "-sLEGACY_GL_EMULATION=1"
    "-sGL_UNSAFE_OPTS=0"
    "-sEMULATE_FUNCTION_POINTER_CASTS=1"
    "-sALLOW_TABLE_GROWTH=1"
    "-sEXPORTED_RUNTIME_METHODS=['FS','stringToUTF8','UTF8ToString','cwrap','ccall','HEAP8','HEAPU8','HEAP32','HEAPU32']"
    "-sEXPORTED_FUNCTIONS=['_main']"
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(${PROJECT_NAME} PUBLIC
        "-sASSERTIONS=2"
        "-sSTACK_OVERFLOW_CHECK=2"
        "-sSTACK_SIZE=5242880"
        "-sGL_DEBUG=1"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC "__EMSCRIPTEN__" "NO_SOUND" "NO_NETWORK" "PLATFORM_WEB")

target_compile_definitions(${PROJECT_NAME} PUBLIC "OPENGL_ES" "NO_EGL" "Web")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}/glad/include/"
    "${CMAKE_SOURCE_DIR}/src"
    "${openal_SOURCE_DIR}/include"
    "${glfw_SOURCE_DIR}/include"
    "lib/include"
)

target_link_libraries(${PROJECT_NAME} zlib ${PNG_LIB} OpenAL::OpenAL glfw ${EXTRA_LIBS})

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/misc/web/index.html" $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

message(STATUS "Compiling for Web (Emscripten)")
