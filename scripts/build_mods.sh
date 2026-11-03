#!/usr/bin/env bash
# Build the mods in mods/ and, optionally, install some into ModernGekko's
# Mods folder.
#
# Compila los mods de mods/ y, si quieres, instala algunos en la carpeta Mods
# de ModernGekko.
#
# Usage / Uso:
#   scripts/build_mods.sh                  # build every mod into build/mods
#   scripts/build_mods.sh boot_logger      # build, then install the named mods
#
# Run scripts/build_and_run_linux.sh once first: it downloads ModernGekko,
# whose headers the mods are built against. mods/README.md explains the rest.
#
# Optional environment variables:
#   MODERNGEKKO_TEMPLATE  where ModernGekko-Template lives (default: ~/ModernGekko-Template)
#   MODERNGEKKO_USER_DIR  ModernGekko's user folder (default: ~/.local/share/moderngekko)

set -euo pipefail

REPOSITORY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly REPOSITORY_ROOT
readonly TEMPLATE_DIRECTORY="${MODERNGEKKO_TEMPLATE:-$HOME/ModernGekko-Template}"
readonly MODERNGEKKO_SOURCE_DIRECTORY="$TEMPLATE_DIRECTORY/lib/ModernGekko"
readonly USER_DIRECTORY="${MODERNGEKKO_USER_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/moderngekko}"
readonly MODS_BUILD_DIRECTORY="$REPOSITORY_ROOT/build/mods"
readonly SYMBOLS_HEADER="$REPOSITORY_ROOT/build/G4AEE9_symbols.h"

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

list_built_mods() {
    find "$MODS_BUILD_DIRECTORY" -maxdepth 1 -name '*.mgm' -printf '%f\n' | sed 's/\.mgm$//' | sort | paste -sd ' ' -
}

[[ -d "$MODERNGEKKO_SOURCE_DIRECTORY/include/moderngekko" ]] ||
    fail "ModernGekko was not found in $TEMPLATE_DIRECTORY; run scripts/build_and_run_linux.sh first"
command -v cmake > /dev/null || fail "cmake is not installed (sudo apt install cmake ninja-build)"

# The mods name game functions through this header (DOLRECOMP_SYMBOL_<name>).
python3 "$REPOSITORY_ROOT/tools/export_mod_symbols.py" --out "$SYMBOLS_HEADER"

cmake -S "$REPOSITORY_ROOT/mods" -B "$MODS_BUILD_DIRECTORY" \
    -DMODERNGEKKO_SOURCE_DIR="$MODERNGEKKO_SOURCE_DIRECTORY" \
    -DMOD_SYMBOL_HEADER="$SYMBOLS_HEADER"
cmake --build "$MODS_BUILD_DIRECTORY"

mod_id=""
for mod_id in "$@"; do
    built_mod="$MODS_BUILD_DIRECTORY/$mod_id.mgm"
    [[ -d "$built_mod" ]] || fail "there is no mod called '$mod_id' (built mods: $(list_built_mods))"
    mkdir -p "$USER_DIRECTORY/Mods"
    # Replace an older copy completely, so a file the mod no longer ships does not linger.
    rm -rf "${USER_DIRECTORY:?}/Mods/$mod_id.mgm"
    cp -r "$built_mod" "$USER_DIRECTORY/Mods/$mod_id.mgm"
    printf 'installed %s in %s/Mods\n' "$mod_id" "$USER_DIRECTORY"
done
