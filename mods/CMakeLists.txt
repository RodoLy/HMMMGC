# Builds the Harvest Moon: Magical Melody mods against ModernGekko's headers.
# Compila los mods de Harvest Moon: Magical Melody con los headers de ModernGekko.
#
# Normally driven by scripts/build_mods.sh; by hand:
#   python3 tools/export_mod_symbols.py
#   cmake -S mods -B build/mods \
#       -DMODERNGEKKO_SOURCE_DIR=$HOME/ModernGekko-Template/lib/ModernGekko \
#       -DMOD_SYMBOL_HEADER=$PWD/build/G4AEE9_symbols.h
#   cmake --build build/mods

cmake_minimum_required(VERSION 3.20)
project(HarvestMoonMagicalMelodyMods LANGUAGES C)

set(MODERNGEKKO_SOURCE_DIR "" CACHE PATH
    "ModernGekko checkout that provides include/moderngekko")
set(MOD_SYMBOL_HEADER "" CACHE FILEPATH
    "G4AEE9_symbols.h, written by tools/export_mod_symbols.py")

if(NOT IS_DIRECTORY "${MODERNGEKKO_SOURCE_DIR}/include/moderngekko")
    message(FATAL_ERROR
        "Set MODERNGEKKO_SOURCE_DIR to the ModernGekko checkout "
        "(<ModernGekko-Template>/lib/ModernGekko).")
endif()
if(NOT EXISTS "${MOD_SYMBOL_HEADER}")
    message(FATAL_ERROR
        "Set MOD_SYMBOL_HEADER to the header written by "
        "tools/export_mod_symbols.py.")
endif()
get_filename_component(MOD_SYMBOL_HEADER_DIRECTORY "${MOD_SYMBOL_HEADER}" DIRECTORY)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# A mod is a shared library called "mod" inside a folder "<mod id>.mgm";
# ModernGekko finds those folders in its Mods directory.
function(add_game_mod mod_id)
    add_library(${mod_id} SHARED ${ARGN})
    target_include_directories(${mod_id} PRIVATE
        "${MODERNGEKKO_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${MOD_SYMBOL_HEADER_DIRECTORY}")
    if(MSVC)
        target_compile_options(${mod_id} PRIVATE /W4)
    else()
        target_compile_options(${mod_id} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(${mod_id} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "mod"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${mod_id}.mgm"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${mod_id}.mgm")
endfunction()

add_game_mod(boot_logger boot_logger/mod.c)
add_game_mod(state_logger state_logger/mod.c)
add_game_mod(jamie_marriage jamie_marriage/mod.c)
