# The launcher inside the Android app (src/in_app.h): its code, its lettering and the mods'
# manifests go into the game's library, which shows the launcher before the game.
# add_launcher_in_app(<target> <mods directory> <mod ids>) adds them to a target that already has
# what the launcher shares with the game (shared/shared.cmake's add_shared_ui), Dear ImGui with its
# SDL3 backends, SDL3 and nlohmann/json.
#
# El launcher dentro de la app de Android: su código, sus letras y los manifiestos de los mods van
# dentro de la biblioteca del juego, que muestra el launcher antes del juego.

set(LAUNCHER_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
set(HMMM_PACKAGE_VERSION "dev" CACHE STRING "the version of the package, which the launcher shows")

function(add_launcher_in_app target mods_directory mod_ids)
    set(source "${LAUNCHER_DIRECTORY}/src")
    # The launcher's code, but its main.cpp and what only a computer does (the other programs it
    # starts: child_process_android.cpp says no to them).
    target_sources(${target} PRIVATE
        "${source}/app.cpp" "${source}/view.cpp" "${source}/look.cpp" "${source}/lettering.cpp"
        "${source}/kerning.cpp" "${source}/mods.cpp" "${source}/paths.cpp" "${source}/preparation.cpp"
        "${source}/disc_search.cpp" "${source}/game_start.cpp" "${source}/updater.cpp"
        "${source}/child_process_android.cpp" "${source}/in_app.cpp")
    target_include_directories(${target} PRIVATE "${source}")
    target_compile_definitions(${target} PRIVATE HMMM_PACKAGE_VERSION="${HMMM_PACKAGE_VERSION}"
                                                 HMMM_SHOWN_VERSION="${HMMM_PACKAGE_VERSION}")
    embed_file_as_array(${target} "${LAUNCHER_DIRECTORY}/fonts/Baloo2-ExtraBold-subset.ttf" FONT_BALOO2_EXTRABOLD)
    embed_file_as_array(${target} "${LAUNCHER_DIRECTORY}/fonts/Dosis-Bold-subset.ttf" FONT_DOSIS_BOLD)
    embed_file_as_array(${target} "${LAUNCHER_DIRECTORY}/fonts/Dosis-SemiBold-subset.ttf" FONT_DOSIS_SEMIBOLD)
    embed_file_as_array(${target} "${LAUNCHER_DIRECTORY}/fonts/DejaVuSans-Bold-symbols.ttf" FONT_DEJAVU_SYMBOLS)

    # The mods' manifests (mods/<id>/mod.json), which the Mods tab reads: as texts in the program.
    set(cases "")
    foreach(mod_id IN LISTS mod_ids)
        set(manifest "${mods_directory}/${mod_id}/mod.json")
        if(EXISTS "${manifest}")
            file(READ "${manifest}" text)
            string(APPEND cases "    if (id == \"${mod_id}\") {\n        return R\"manifest(${text})manifest\";\n    }\n")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
        endif()
    endforeach()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/mod_manifests.cpp")
    file(CONFIGURE OUTPUT "${output}" CONTENT
"// Written by launcher/in_app.cmake: the mods' manifests (mods/<id>/mod.json), for the launcher
// inside the Android app (in_app.h).
#include \"in_app.h\"

std::string in_app::mod_manifest(const std::string& id) {
${cases}    return {};
}
" @ONLY)
    target_sources(${target} PRIVATE "${output}")
endfunction()
