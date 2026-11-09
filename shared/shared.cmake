# What the programs of the native version share (shared/README.md): the look after the game's
# own, its fonts, the texts in Spanish and English, the player's settings.json and the
# controllers. add_shared_ui(<target>) compiles them into a program that already links Dear
# ImGui, SDL3 and nlohmann/json.
#
# Lo que comparten los programas de la versión nativa: el aspecto a partir del propio juego,
# sus fuentes, los textos en español e inglés, el settings.json del jugador y los controles.
# add_shared_ui(<target>) los compila dentro de un programa que ya enlaza Dear ImGui, SDL3 y
# nlohmann/json.

set(SHARED_UI_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")

# A font of shared/fonts as a C++ array, `symbol`, with its size in `symbol`_size.
function(_shared_ui_embed_font target file symbol)
    set(source "${SHARED_UI_DIRECTORY}/fonts/${file}")
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/${symbol}.cpp")
    file(READ "${source}" bytes HEX)
    string(LENGTH "${bytes}" length)
    math(EXPR size "${length} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
    file(CONFIGURE OUTPUT "${output}" CONTENT
"// Written by shared/shared.cmake from shared/fonts/${file}.
#include <cstddef>
extern const unsigned char ${symbol}[] = {${bytes}};
extern const size_t ${symbol}_size = ${size};
")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    target_sources(${target} PRIVATE "${output}")
endfunction()

function(add_shared_ui target)
    target_sources(${target} PRIVATE
        "${SHARED_UI_DIRECTORY}/controls.cpp"
        "${SHARED_UI_DIRECTORY}/option_rows.cpp"
        "${SHARED_UI_DIRECTORY}/settings.cpp"
        "${SHARED_UI_DIRECTORY}/texts.cpp"
        "${SHARED_UI_DIRECTORY}/theme.cpp")
    target_include_directories(${target} PRIVATE "${SHARED_UI_DIRECTORY}")
    # The arithmetic of ImVec2 (+, -, *), which imgui.h defines when asked.
    target_compile_definitions(${target} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
    # The fonts go inside the program (fonts/README.md says where they come from).
    _shared_ui_embed_font(${target} RoundedMplus1c-Medium-subset.ttf FONT_ROUNDED_MEDIUM)
    _shared_ui_embed_font(${target} RoundedMplus1c-ExtraBold-subset.ttf FONT_ROUNDED_EXTRABOLD)
endfunction()
