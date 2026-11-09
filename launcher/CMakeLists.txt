# The launcher of the native version (docs/NATIVE_PACKAGE.md): the window a player opens. It
# prepares the game from the player's own disc the first time (tools/prepare_game.py), keeps
# the options, the mods and the controls, and starts the game. It holds nothing of the game,
# so it ships in the package as it is.
#
# El launcher de la versión nativa: la ventana que abre el jugador. Prepara el juego con el
# disco del propio jugador la primera vez, guarda las opciones, los mods y los controles, y
# arranca el juego. No tiene nada del juego, así que va en el paquete tal cual.
#
#   cmake -S launcher -B build/launcher -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
#   cmake --build build/launcher
#
# It draws with SDL3's 2D renderer and Dear ImGui, the same versions as aurora (WiiCompiled's
# aurora-main/cmake), and reads JSON with nlohmann/json. They are downloaded when the build is
# configured; FETCHCONTENT_SOURCE_DIR_SDL, _IMGUI and _NLOHMANN_JSON point it at local copies.
cmake_minimum_required(VERSION 3.25)
project(hmmm_launcher LANGUAGES C CXX)
if(WIN32)
    enable_language(RC)
endif()

set(HMMM_PACKAGE_VERSION "dev" CACHE STRING "the version of the package, which the launcher shows and records")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# SDL3, static, with only what the launcher uses: video, the 2D renderer, gamepads, processes and
# file dialogs.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz
    URL_HASH SHA256=12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL)
# Dear ImGui has no CMake build of its own (SOURCE_SUBDIR names a folder it does not have, so that it
# is only downloaded): its sources are compiled below.
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b-docking.tar.gz
    URL_HASH SHA256=466fdef9b18de15f0bb6e288e3d00ffa3d82200ec458ce5e4f724a161d9528a5
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    SOURCE_SUBDIR no-cmake-build)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SDL imgui nlohmann_json)
add_library(launcher_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
target_include_directories(launcher_imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(launcher_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(launcher_imgui PUBLIC SDL3::SDL3-static)

# The fonts, as C++ arrays inside the program (fonts/README.md says where they come from).
function(embed_font file symbol)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/fonts/${file}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${symbol}.cpp")
    file(READ "${source}" bytes HEX)
    string(LENGTH "${bytes}" length)
    math(EXPR size "${length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
    file(CONFIGURE OUTPUT "${output}" CONTENT
"// Written by launcher/CMakeLists.txt from fonts/${file}.
#include <cstddef>
extern const unsigned char ${symbol}[] = {${bytes}};
extern const size_t ${symbol}_size = ${size};
")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    target_sources(HarvestMoonMM PRIVATE "${output}")
endfunction()

file(GLOB launcher_sources CONFIGURE_DEPENDS src/*.cpp)
if(WIN32)
    list(FILTER launcher_sources EXCLUDE REGEX "_posix\\.cpp$")
else()
    list(FILTER launcher_sources EXCLUDE REGEX "_windows\\.cpp$")
endif()
add_executable(HarvestMoonMM WIN32 ${launcher_sources})
embed_font(RoundedMplus1c-Medium-subset.ttf FONT_ROUNDED_MEDIUM)
embed_font(RoundedMplus1c-ExtraBold-subset.ttf FONT_ROUNDED_EXTRABOLD)
target_compile_definitions(HarvestMoonMM PRIVATE HMMM_PACKAGE_VERSION="${HMMM_PACKAGE_VERSION}")
target_compile_options(HarvestMoonMM PRIVATE -Wall -Wextra)
target_link_libraries(HarvestMoonMM PRIVATE launcher_imgui SDL3::SDL3-static nlohmann_json::nlohmann_json)
if(WIN32)
    # The manifest: UTF-8 as the program's code page (paths with any letter), and sharp text on
    # screens with scaling. The C++ runtime is linked in, so that only Windows' DLLs are needed.
    target_sources(HarvestMoonMM PRIVATE windows/launcher.rc)
    target_link_options(HarvestMoonMM PRIVATE -static)
endif()
