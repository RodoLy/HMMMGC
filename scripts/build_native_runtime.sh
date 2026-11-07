#!/usr/bin/env bash
# Build the native runtime: translate main.dol to C++ with WiiCompiled's translator,
# then compile it with the runtime in runtime/ (docs/NATIVE_RUNTIME.md).
#
# Compila el runtime nativo: traduce main.dol a C++ con el traductor de WiiCompiled y
# luego lo compila con el runtime de runtime/.
#
# Usage / Uso:
#   scripts/build_native_runtime.sh [extra cmake arguments for the runtime]
#   build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
#
# Needs git, the .NET 8 SDK (dotnet), clang++, cmake and ninja, and the extracted disc
# in orig/G4AEE9 (tools/extract_disc.py). Everything is written under build/native.
# The picture comes from aurora, which downloads its own dependencies when the runtime
# is configured (Dawn already built, SDL3 and a few libraries); where downloads are not
# possible, the extra cmake arguments can point it at local copies, for example
# -DDAWN_SOURCE_DIR=... -DDAWN_BUILD_DIR=... -DFETCHCONTENT_SOURCE_DIR_SDL=...
#
# Optional environment variables:
#   WIICOMPILED_DIR  a WiiCompiled checkout to use instead of cloning one
#                    (default: build/native/wiicompiled, at the commit below)
#   GAME_DIR         the extracted disc (default: orig/G4AEE9)

set -euo pipefail

REPOSITORY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly REPOSITORY_ROOT

# The WiiCompiled version the runtime is written against: its translator, and the guest
# ABI headers that the translated code and the runtime share.
readonly WIICOMPILED_URL="https://github.com/patchzyy/wiicompiled.git"
readonly WIICOMPILED_COMMIT="0d5c402d0366640a294bf9c699068299591f7eff"

readonly BUILD_DIRECTORY="$REPOSITORY_ROOT/build/native"
readonly WIICOMPILED_DIRECTORY="${WIICOMPILED_DIR:-$BUILD_DIRECTORY/wiicompiled}"
readonly GAME_DIRECTORY="${GAME_DIR:-$REPOSITORY_ROOT/orig/G4AEE9}"
readonly TRANSLATION_DIRECTORY="$BUILD_DIRECTORY/translation"
readonly TRANSLATED_GAME_DIRECTORY="$TRANSLATION_DIRECTORY/game"
readonly RUNTIME_BUILD_DIRECTORY="$BUILD_DIRECTORY/runtime"
readonly TRANSLATOR_PROJECT="$WIICOMPILED_DIRECTORY/translator/src/Translator.Cli"
readonly TRANSLATOR="$TRANSLATOR_PROJECT/bin/Release/net8.0/Translator.Cli.dll"
# The mods of mods/ that go into the runtime (NATIVE_MODS="" for none).
readonly NATIVE_MODS="${NATIVE_MODS-jamie_marriage player_speed}"

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

step() {
    printf '\n== %s\n' "$*"
}

check_tools() {
    local tool
    for tool in git dotnet clang++ cmake ninja python3; do
        command -v "$tool" > /dev/null || fail "$tool is not installed"
    done
    [[ -f "$GAME_DIRECTORY/sys/main.dol" ]] ||
        fail "no extracted disc in $GAME_DIRECTORY (tools/extract_disc.py writes it from your own disc)"
}

fetch_wiicompiled() {
    if [[ -d "$WIICOMPILED_DIRECTORY/translator" ]]; then
        return
    fi
    step "Downloading WiiCompiled $WIICOMPILED_COMMIT"
    git clone --filter=blob:none --no-checkout "$WIICOMPILED_URL" "$WIICOMPILED_DIRECTORY"
    git -C "$WIICOMPILED_DIRECTORY" checkout --quiet "$WIICOMPILED_COMMIT"
}

build_translator() {
    # The checkout is pinned, so a translator built once stays valid (and the build would
    # otherwise need NuGet again).
    if [[ -f "$TRANSLATOR" ]]; then
        return
    fi
    step "Building the translator"
    dotnet build "$TRANSLATOR_PROJECT" --configuration Release --nologo --verbosity quiet
}

translator() {
    dotnet "$TRANSLATOR" "$@"
}

translate_game() {
    step "Translating main.dol"
    local entry_point
    entry_point="$(python3 "$REPOSITORY_ROOT/tools/write_translator_project.py" \
        --dol "$GAME_DIRECTORY/sys/main.dol" --out "$TRANSLATION_DIRECTORY")"
    local project="$TRANSLATION_DIRECTORY/project.yml"
    translator translate-recursive "$entry_point" --project "$project" \
        --outdir "$TRANSLATED_GAME_DIRECTORY/functions" \
        --output-metadata "$TRANSLATED_GAME_DIRECTORY/generated/base_translation_output.json"
    translator generate-data-init --project "$project"
    translator emit-build-shards --project "$project" \
        --base-metadata "$TRANSLATED_GAME_DIRECTORY/generated/base_translation_output.json" \
        --base-functions-dir "$TRANSLATED_GAME_DIRECTORY/functions" \
        --native-source-dir "$REPOSITORY_ROOT/runtime/src" \
        --out "$TRANSLATED_GAME_DIRECTORY/build_shards"
}

# The mods are compiled into the runtime; the translated code gets a call at every place
# where they act (tools/insert_mod_hooks.py), and the mods' names for the game's functions
# come from symbols.txt (tools/export_mod_symbols.py).
mark_mod_hooks() {
    step "Marking where the mods act: ${NATIVE_MODS:-no mods}"
    python3 "$REPOSITORY_ROOT/tools/export_mod_symbols.py"
    local mod_arguments=()
    local mod_id
    for mod_id in $NATIVE_MODS; do
        mod_arguments+=(--mod "$mod_id")
    done
    python3 "$REPOSITORY_ROOT/tools/insert_mod_hooks.py" --translation "$TRANSLATED_GAME_DIRECTORY" "${mod_arguments[@]}"
}

compile_runtime() {
    step "Compiling the runtime and the translated game"
    cmake -S "$REPOSITORY_ROOT/runtime" -B "$RUNTIME_BUILD_DIRECTORY" -G Ninja \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DWIICOMPILED_DIR="$WIICOMPILED_DIRECTORY" \
        -DTRANSLATED_GAME_DIR="$TRANSLATED_GAME_DIRECTORY" \
        -DNATIVE_MODS="${NATIVE_MODS// /;}" \
        "$@"
    cmake --build "$RUNTIME_BUILD_DIRECTORY"
}

main() {
    check_tools
    fetch_wiicompiled
    build_translator
    translate_game
    mark_mod_hooks
    compile_runtime "$@"
    step "Done"
    printf 'Run it with:\n  %s --disc %s --symbols %s\n' "$RUNTIME_BUILD_DIRECTORY/hmmm-native" \
        "$GAME_DIRECTORY" "$REPOSITORY_ROOT/config/G4AEE9/symbols.txt"
}

main "$@"
