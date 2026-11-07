# The native runtime of Harvest Moon: Magical Melody: the game's code, translated to C++
# by WiiCompiled's translator, linked with the runtime in src/ instead of Dolphin.
#
# El runtime nativo de Harvest Moon: Magical Melody: el código del juego, traducido a C++
# por el traductor de WiiCompiled, enlazado con el runtime de src/ en vez de Dolphin.
#
# scripts/build_native_runtime.sh runs the translator and then this. By hand:
#   cmake -S runtime -B build/native/runtime -G Ninja \
#         -DWIICOMPILED_DIR=<WiiCompiled checkout> \
#         -DTRANSLATED_GAME_DIR=<translator output>
#   cmake --build build/native/runtime
cmake_minimum_required(VERSION 3.25)
project(hm_magical_melody_native LANGUAGES C CXX ASM)

set(WIICOMPILED_DIR "" CACHE PATH "a WiiCompiled checkout: its guest ABI headers and its copy of aurora")
set(TRANSLATED_GAME_DIR "" CACHE PATH "what the translator wrote for main.dol")
set(X86_CPU_PROFILE "v3" CACHE STRING "x86-64 micro-architecture level of the build (v2 or v3), as WiiCompiled's")
set(DAWN_SOURCE_DIR "" CACHE PATH "optional: a Dawn source tree, built in DAWN_BUILD_DIR, to use instead of aurora's own")
set(DAWN_BUILD_DIR "" CACHE PATH "optional: the build of DAWN_SOURCE_DIR (a static libwebgpu_dawn)")

set(WIICOMPILED_RUNTIME_DIR "${WIICOMPILED_DIR}/runtime")
set(AURORA_DIR "${WIICOMPILED_DIR}/aurora-main")
if(NOT IS_DIRECTORY "${WIICOMPILED_RUNTIME_DIR}/include/guest_abi" OR NOT IS_DIRECTORY "${AURORA_DIR}/include/aurora")
    message(FATAL_ERROR "WIICOMPILED_DIR must be a WiiCompiled checkout (with runtime/ and aurora-main/)")
endif()
if(NOT EXISTS "${TRANSLATED_GAME_DIR}/data_sections_init.cpp")
    message(FATAL_ERROR "TRANSLATED_GAME_DIR must hold the translator's output (run scripts/build_native_runtime.sh)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "the translated code is written for Clang (set CMAKE_CXX_COMPILER=clang++)")
endif()

# The translator's explicit float casts are the PowerPC's single-precision rounding
# points: fast-math or fused multiply-adds would change results the game can see.
set(strict_floating_point -fno-fast-math -ffp-contract=off)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    list(APPEND strict_floating_point -march=x86-64-${X86_CPU_PROFILE})
endif()

# The game, as the translator wrote it. It is not part of this repository: it is made
# from the player's own disc. It is compiled the way WiiCompiled compiles it.
file(GLOB translated_shards CONFIGURE_DEPENDS
     "${TRANSLATED_GAME_DIR}/build_shards/base_common/*.cpp"
     "${TRANSLATED_GAME_DIR}/build_shards/base_registration/*.cpp"
     "${TRANSLATED_GAME_DIR}/build_shards/base_dispatch/*.cpp")
add_library(translated_game OBJECT
    ${translated_shards}
    "${TRANSLATED_GAME_DIR}/data_sections_init.cpp"
    "${TRANSLATED_GAME_DIR}/data_sections_init_blobs.S")
target_include_directories(translated_game PRIVATE
    "${WIICOMPILED_RUNTIME_DIR}/include"
    "${TRANSLATED_GAME_DIR}"
    "${TRANSLATED_GAME_DIR}/build_shards/base_common")
target_compile_options(translated_game PRIVATE ${strict_floating_point} -O2 -fno-slp-vectorize -w)

# Two helpers of WiiCompiled's runtime (GPL-3.0, like this project), compiled from its
# checkout: the FPSCR instructions and the paired-single quantized loads and stores.
add_library(wiicompiled_isa_helpers OBJECT
    "${WIICOMPILED_RUNTIME_DIR}/src/fpu_helpers.cpp"
    "${WIICOMPILED_RUNTIME_DIR}/src/ppc_quantized.cpp")
target_include_directories(wiicompiled_isa_helpers PRIVATE "${WIICOMPILED_RUNTIME_DIR}/include")
target_compile_options(wiicompiled_isa_helpers PRIVATE ${strict_floating_point} -O2 -fno-slp-vectorize -w)

# The picture: aurora (https://github.com/encounter/aurora, MIT), the GameCube's graphics
# library on WebGPU, in the version that WiiCompiled carries. It draws with Dawn, which
# aurora downloads already built for the platform unless DAWN_BUILD_DIR names a Dawn
# built from source (how the runtime is tested where those downloads are not possible).
if(DAWN_BUILD_DIR)
    add_library(dawn::webgpu_dawn STATIC IMPORTED GLOBAL)
    set_target_properties(dawn::webgpu_dawn PROPERTIES
        IMPORTED_LOCATION "${DAWN_BUILD_DIR}/src/dawn/native/libwebgpu_dawn.a"
        INTERFACE_INCLUDE_DIRECTORIES "${DAWN_SOURCE_DIR}/include;${DAWN_BUILD_DIR}/gen/include")
    find_package(Threads REQUIRED)
    set_property(TARGET dawn::webgpu_dawn APPEND PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS})
    set(Dawn_FOUND TRUE)
    set(AURORA_DAWN_PROVIDER system CACHE STRING "" FORCE)
endif()
# aurora's graphics and controllers: the runtime has its own disc, memory card and video
# interface.
set(AURORA_ENABLE_GX ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_THP OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(AURORA_TARGET_RVL OFF CACHE BOOL "" FORCE)
add_subdirectory("${AURORA_DIR}" "${CMAKE_BINARY_DIR}/aurora" EXCLUDE_FROM_ALL SYSTEM)

file(GLOB_RECURSE runtime_sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(hmmm-native ${runtime_sources})
target_include_directories(hmmm-native PRIVATE src "${WIICOMPILED_RUNTIME_DIR}/include")
target_compile_options(hmmm-native PRIVATE ${strict_floating_point} -Wall -Wextra -Wno-unused-parameter)
# aurora's graphics ask aurora's video interface (aurora::vi) for the size of the embedded
# frame buffer. The game keeps the console's own video interface (its VI library runs
# translated, on the emulated registers), so nothing configures aurora's: it answers the
# GPU's 640x528, the same as for any mode of the game.
target_link_libraries(hmmm-native PRIVATE translated_game wiicompiled_isa_helpers aurora::core aurora::gx aurora::vi
                                          aurora::pad)
# The crash reports name the translated functions (func_XXXXXXXX) with dladdr.
target_link_options(hmmm-native PRIVATE -rdynamic)
