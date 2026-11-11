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
#
# The runtime kit that a package carries instead of the game (docs/NATIVE_PACKAGE.md) is this
# build with -DTRANSLATED_GAME_DIR=runtime/kit/stub_game, a stand-in of the translated game.
cmake_minimum_required(VERSION 3.25)
project(hm_magical_melody_native LANGUAGES C CXX ASM)
if(WIN32)
    enable_language(RC)
endif()

set(WIICOMPILED_DIR "" CACHE PATH "a WiiCompiled checkout: its guest ABI headers and its copy of aurora")
set(TRANSLATED_GAME_DIR "" CACHE PATH "what the translator wrote for main.dol")
set(X86_CPU_PROFILE "v3" CACHE STRING "x86-64 micro-architecture level of the build (v2 or v3), as WiiCompiled's")
set(DAWN_SOURCE_DIR "" CACHE PATH "optional: a Dawn source tree, built in DAWN_BUILD_DIR, to use instead of aurora's own")
set(DAWN_BUILD_DIR "" CACHE PATH "optional: the build of DAWN_SOURCE_DIR (a static libwebgpu_dawn)")
set(DAWN_PACKAGE_DIR "" CACHE PATH "optional, Windows: an unpacked prebuilt Dawn package (bin/webgpu_dawn.dll)")

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
# The data blobs are assembly for ELF objects. Windows objects (COFF) name the read-only
# section differently and have no stack note, so a converted copy is assembled there.
set(translated_blobs "${TRANSLATED_GAME_DIR}/data_sections_init_blobs.S")
if(WIN32)
    file(READ "${translated_blobs}" blobs_assembly)
    string(REPLACE ".section .rodata,\"a\",@progbits" ".section .rdata,\"dr\"" blobs_assembly "${blobs_assembly}")
    string(REPLACE ".section .note.GNU-stack,\"\",@progbits" "" blobs_assembly "${blobs_assembly}")
    set(translated_blobs "${CMAKE_BINARY_DIR}/data_sections_init_blobs_coff.S")
    file(WRITE "${translated_blobs}" "${blobs_assembly}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TRANSLATED_GAME_DIR}/data_sections_init_blobs.S")
endif()
add_library(translated_game OBJECT
    ${translated_shards}
    "${TRANSLATED_GAME_DIR}/data_sections_init.cpp"
    "${translated_blobs}")
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
elseif(DAWN_PACKAGE_DIR)
    # The package aurora would download, unpacked beforehand. Its DLL is built with MSVC,
    # so only its C interface (webgpu.h) is used across it; see src/platform/dawn_mingw.cpp.
    add_library(dawn::webgpu_dawn SHARED IMPORTED GLOBAL)
    set_target_properties(dawn::webgpu_dawn PROPERTIES
        IMPORTED_LOCATION "${DAWN_PACKAGE_DIR}/bin/webgpu_dawn.dll"
        IMPORTED_IMPLIB "${DAWN_PACKAGE_DIR}/lib/webgpu_dawn.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${DAWN_PACKAGE_DIR}/include")
    set(Dawn_FOUND TRUE)
    set(AURORA_DAWN_PROVIDER system CACHE STRING "" FORCE)
endif()
# aurora's graphics, controllers and memory card: the runtime has its own disc and video
# interface.
set(AURORA_ENABLE_GX ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_CARD ON CACHE BOOL "" FORCE)
set(AURORA_ENABLE_THP OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(AURORA_TARGET_RVL OFF CACHE BOOL "" FORCE)
add_subdirectory("${AURORA_DIR}" "${CMAKE_BINARY_DIR}/aurora" EXCLUDE_FROM_ALL SYSTEM)

# What the launcher shares with the runtime (shared/README.md): the player's settings.json,
# read with nlohmann/json (MIT) in the launcher's version, and the look, texts and controls of
# the options menu inside the game, drawn with aurora's Dear ImGui. nlohmann/json is downloaded
# when the build is configured; FETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON points it at a local copy.
include(FetchContent)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(nlohmann_json)
include("${CMAKE_CURRENT_SOURCE_DIR}/../shared/shared.cmake")

# The game's mods (mods/), compiled in: each one is the same C file that ModernGekko loads
# as a library, built here against a stand-in of ModernGekko's mod ABI (mod_api/). The
# translated code must be marked for the same list of mods (tools/insert_mod_hooks.py,
# which scripts/build_native_runtime.sh runs with NATIVE_MODS).
set(NATIVE_MODS "jamie_marriage;player_speed;carry_speed" CACHE STRING "the mods in mods/ to compile into the runtime")
set(MOD_SYMBOL_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../build/G4AEE9_symbols.h" CACHE FILEPATH
    "G4AEE9_symbols.h, written by tools/export_mod_symbols.py")
set(mods_directory "${CMAKE_CURRENT_SOURCE_DIR}/../mods")
set(native_mod_list "${CMAKE_BINARY_DIR}/generated/native_mod_list.h")
set(native_mod_declarations "")
set(native_mod_getters "")
set(native_mod_objects "")
if(NATIVE_MODS AND NOT EXISTS "${MOD_SYMBOL_HEADER}")
    message(FATAL_ERROR "MOD_SYMBOL_HEADER: run tools/export_mod_symbols.py first")
endif()
get_filename_component(mod_symbol_header_directory "${MOD_SYMBOL_HEADER}" DIRECTORY)
foreach(mod_id IN LISTS NATIVE_MODS)
    add_library(native_mod_${mod_id} OBJECT "${mods_directory}/${mod_id}/mod.c")
    target_include_directories(native_mod_${mod_id} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/mod_api" "${mods_directory}/include" "${mod_symbol_header_directory}")
    # Every mod exports a function of the same name: here each gets one of its own.
    target_compile_definitions(native_mod_${mod_id} PRIVATE moderngekko_get_mod=moderngekko_get_mod_${mod_id})
    target_compile_options(native_mod_${mod_id} PRIVATE -std=c11 -Wall -Wextra)
    string(APPEND native_mod_declarations "extern \"C\" const ModernGekkoModDesc* moderngekko_get_mod_${mod_id}(void);\n")
    string(APPEND native_mod_getters "    moderngekko_get_mod_${mod_id},\n")
    list(APPEND native_mod_objects $<TARGET_OBJECTS:native_mod_${mod_id}>)
endforeach()
file(CONFIGURE OUTPUT "${native_mod_list}" CONTENT
"// Written by runtime/CMakeLists.txt: the mods compiled into the runtime (NATIVE_MODS).
#pragma once
#include \"moderngekko/mod_abi.h\"
${native_mod_declarations}
static const ModernGekkoGetModFn COMPILED_IN_MODS[] = {
${native_mod_getters}    nullptr,
};
")

file(GLOB_RECURSE runtime_sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(hmmm-native ${runtime_sources} ${native_mod_objects})
add_shared_ui(hmmm-native)
target_include_directories(hmmm-native PRIVATE src "${WIICOMPILED_RUNTIME_DIR}/include" mod_api
                                               "${CMAKE_BINARY_DIR}/generated")
target_compile_options(hmmm-native PRIVATE ${strict_floating_point} -Wall -Wextra -Wno-unused-parameter)
# aurora's graphics ask aurora's video interface (aurora::vi) for the size of the embedded
# frame buffer. The game keeps the console's own video interface (its VI library runs
# translated, on the emulated registers), so nothing configures aurora's: it answers the
# GPU's 640x528, the same as for any mode of the game.
target_link_libraries(hmmm-native PRIVATE translated_game wiicompiled_isa_helpers aurora::core aurora::gx aurora::vi
                                          aurora::pad aurora::card imgui nlohmann_json::nlohmann_json)
if(WIN32)
    # VirtualAlloc2 and MapViewOfFile3 (guest_memory.cpp), and the stack walk of the crash
    # reports. The C++ runtime is linked in, so that the program only needs Windows' DLLs.
    target_link_libraries(hmmm-native PRIVATE onecore dbghelp)
    target_link_options(hmmm-native PRIVATE -static)
    # The manifest: UTF-8 as the program's code page, for paths with any letter.
    target_sources(hmmm-native PRIVATE windows/hmmm-native.rc)
else()
    # The crash reports name the translated functions (func_XXXXXXXX) with dladdr.
    target_link_options(hmmm-native PRIVATE -rdynamic)
endif()
