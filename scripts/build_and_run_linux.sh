#!/usr/bin/env bash
# Build ModernGekko and the recompiled Harvest Moon: Magical Melody on Ubuntu
# (native, or inside WSL2 on Windows), then start the game.
#
# Compila ModernGekko y Harvest Moon: Magical Melody recompilado en Ubuntu
# (nativo, o dentro de WSL2 en Windows) y arranca el juego.
#
# Usage / Uso:
#   scripts/build_and_run_linux.sh "/mnt/c/Users/you/Downloads/Harvest Moon.rvz"   # first time
#   scripts/build_and_run_linux.sh                                                  # afterwards
#
# Every step is skipped when its result already exists, so later runs only
# start the game, and a run that stopped halfway continues where it was.
# Everything is built under $HOME; only the apt packages need sudo.
# docs/BUILDING.md explains each step in Spanish and English.
#
# Optional environment variables:
#   MODERNGEKKO_TEMPLATE  where ModernGekko-Template lives (default: ~/ModernGekko-Template)
#   GRAPHICS_BACKEND      Vulkan or OGL, when the automatic choice is wrong
#   EXTRA_RUN_ARGS        more arguments for moderngekko-run, e.g. "--mute"

set -euo pipefail

REPOSITORY_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly REPOSITORY_ROOT

# The ModernGekko-Template version this game was tested with. It pins
# ModernGekko edd460a and DolRecomp 1bec355, which boot the game into a new
# save (docs/RECOMPILER_EVALUATION.md). Newer versions may work as well.
readonly TEMPLATE_URL="https://github.com/ExpansionPak/ModernGekko-Template.git"
readonly TEMPLATE_COMMIT="c2553a99584b2045dbe46a70e1570994dae8beef"
readonly TEMPLATE_DIRECTORY="${MODERNGEKKO_TEMPLATE:-$HOME/ModernGekko-Template}"

# The template keeps each game under extracted/<slug>/.
readonly GAME_SLUG="Harvest-Moon-Magical-Melody"
readonly EXTRACTED_DIRECTORY="$TEMPLATE_DIRECTORY/extracted/$GAME_SLUG"

# ModernGekko-Template's package list for Ubuntu (from its README), plus what
# this script needs: git and curl to download, python3 for our disc tools,
# mesa-utils and vulkan-tools to see which graphics drivers are available.
readonly APT_PACKAGES=(
    ninja-build build-essential pkg-config cmake
    libevdev-dev libudev-dev libgtk-3-dev libsystemd-dev
    libbluetooth-dev libasound2-dev libpulse-dev libgl1-mesa-dev
    libxrandr-dev libxi-dev
    git curl python3 mesa-utils vulkan-tools
)

step() { printf '\n==> %s\n' "$*"; }
note() { printf '    %s\n' "$*"; }
fail() {
    printf '\nerror: %s\n' "$*" >&2
    exit 1
}

running_in_wsl() { grep -qi microsoft /proc/version 2> /dev/null; }

check_system() {
    step "Checking the system"
    [[ "$(uname -s)" == Linux ]] || fail "this script is for Linux; on Windows, run it inside WSL2 (see docs/BUILDING.md)"
    command -v apt-get > /dev/null ||
        fail "only Ubuntu and Debian are automated; install the packages from docs/BUILDING.md by hand"
    if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
        running_in_wsl &&
            fail "WSL has no window support yet: run 'wsl --update' in Windows PowerShell, then reopen Ubuntu"
        fail "no graphical session (DISPLAY and WAYLAND_DISPLAY are empty)"
    fi
    if running_in_wsl; then
        note "$(uname -m), inside WSL2"
    else
        note "$(uname -m), native Linux"
    fi
}

install_missing_packages() {
    local missing=()
    local package
    for package in "${APT_PACKAGES[@]}"; do
        dpkg -s "$package" > /dev/null 2>&1 || missing+=("$package")
    done
    [[ ${#missing[@]} -eq 0 ]] && return

    step "Installing ${#missing[@]} packages (sudo asks for your Linux password)"
    local as_root=()
    [[ "$(id -u)" -ne 0 ]] && as_root=(sudo)
    "${as_root[@]}" apt-get update
    "${as_root[@]}" apt-get install -y "${missing[@]}"
}

# Pick the graphics backend that reaches the GPU. Sets GRAPHICS_BACKEND.
choose_graphics_backend() {
    step "Looking for GPU acceleration"
    local opengl_renderer vulkan_device_types
    opengl_renderer="$(glxinfo -B 2> /dev/null | sed -n 's/^OpenGL renderer string: //p')"
    vulkan_device_types="$(vulkaninfo --summary 2> /dev/null | sed -n 's/^[[:space:]]*deviceType[[:space:]]*= //p')"
    note "OpenGL renderer: ${opengl_renderer:-none}"
    note "Vulkan devices:  $(echo "${vulkan_device_types:-none}" | paste -sd ' ' -)"

    if [[ -n "${GRAPHICS_BACKEND:-}" ]]; then
        note "using GRAPHICS_BACKEND=$GRAPHICS_BACKEND as given"
        return
    fi
    if running_in_wsl && [[ "$opengl_renderer" == *D3D12* ]]; then
        # WSL reaches the Windows GPU driver through Mesa's Direct3D 12
        # layer, whose OpenGL driver is more mature than its Vulkan one.
        GRAPHICS_BACKEND=OGL
    elif grep -qE 'PHYSICAL_DEVICE_TYPE_(INTEGRATED|DISCRETE|VIRTUAL)_GPU' <<< "$vulkan_device_types"; then
        GRAPHICS_BACKEND=Vulkan
    elif [[ -n "$opengl_renderer" && "$opengl_renderer" != *llvmpipe* ]]; then
        GRAPHICS_BACKEND=OGL
    else
        # Software Vulkan (lavapipe) is what the game was tested with on a
        # machine without a GPU: it works, at a few frames per second.
        GRAPHICS_BACKEND=Vulkan
        note "no GPU acceleration found: the game will run, but very slowly"
        if running_in_wsl && [[ ! -e /dev/dxg ]]; then
            note "WSL cannot see the GPU (/dev/dxg is missing): update Windows, the GPU driver and WSL"
        fi
    fi
    note "graphics backend: $GRAPHICS_BACKEND"
}

download_template() {
    # Written only once every submodule is in place. Checking for the folder
    # alone is not enough: a download stopped halfway (Ctrl+C, the computer
    # going to sleep) leaves the folder with submodules missing.
    local download_finished_marker="$TEMPLATE_DIRECTORY/.git/hm-mm-recomp-download-finished"
    [[ -f "$download_finished_marker" ]] && return

    step "Downloading ModernGekko-Template and its submodules (about 1 GB)"
    note "Keep the computer awake until this finishes; a sleeping PC stalls the download."
    if [[ ! -d "$TEMPLATE_DIRECTORY/.git" ]]; then
        git clone --no-checkout "$TEMPLATE_URL" "$TEMPLATE_DIRECTORY"
    fi
    git -C "$TEMPLATE_DIRECTORY" checkout --quiet "$TEMPLATE_COMMIT"
    git -C "$TEMPLATE_DIRECTORY" submodule sync --recursive --quiet
    # --force checks every submodule out again, which also restores the files
    # an interrupted earlier run never got to write.
    git -C "$TEMPLATE_DIRECTORY" submodule update --init --recursive --force
    touch "$download_finished_marker"
}

build_tools() {
    local build_directory="$TEMPLATE_DIRECTORY/lib/ModernGekko/build"
    [[ -x "$build_directory/moderngekko-port" && -x "$build_directory/moderngekko-run" ]] && return
    step "Building DolRecomp and ModernGekko (the longest step, 15 to 40 minutes)"
    make -C "$TEMPLATE_DIRECTORY" tools
}

# nodtool reads RVZ, ISO and GCM images; tools/extract_disc.py drives it.
install_nodtool() {
    # shellcheck source=/dev/null
    [[ -f "$HOME/.cargo/env" ]] && source "$HOME/.cargo/env"
    command -v nodtool > /dev/null && return
    step "Installing nodtool, which reads RVZ/ISO disc images"
    if ! command -v cargo > /dev/null; then
        note "installing Rust with rustup (into ~/.cargo)"
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
        # shellcheck source=/dev/null
        source "$HOME/.cargo/env"
    fi
    cargo install --locked nodtool
}

extract_disc() {
    [[ -f "$EXTRACTED_DIRECTORY/sys/main.dol" ]] && return
    local disc_image="$1"
    [[ -n "$disc_image" ]] ||
        fail "the game is not extracted yet; give your disc image, for example: $0 \"/mnt/c/Users/you/Downloads/Harvest Moon - Magical Melody (USA).rvz\""
    # Accept a Windows path pasted from Explorer, such as C:\Users\you\game.rvz.
    if [[ "$disc_image" == [A-Za-z]:\\* ]] && command -v wslpath > /dev/null; then
        disc_image="$(wslpath -u "$disc_image")"
    fi
    [[ -f "$disc_image" ]] || fail "disc image not found: $disc_image"

    install_nodtool
    step "Extracting your disc into $EXTRACTED_DIRECTORY"
    python3 "$REPOSITORY_ROOT/tools/extract_disc.py" "$disc_image" "$EXTRACTED_DIRECTORY"
}

# Checked on every run, so a wrong disc never reaches the recompiler.
verify_disc() {
    step "Checking that the disc is Harvest Moon: Magical Melody (USA)"
    python3 "$REPOSITORY_ROOT/tools/check_disc.py" "$EXTRACTED_DIRECTORY" > /dev/null ||
        fail "$EXTRACTED_DIRECTORY is not the G4AEE9 revision 0 disc; delete it and run again with the right image"
    note "OK"
}

run_game() {
    step "Starting the game"
    note "The first start also compiles the game module (a few minutes); later starts are immediate."
    note "Keyboard: arrows = stick (hold Shift to walk), X = A, Z = B, Enter = Start,"
    note "          C = X, S = Y, D = Z, Q / W = L / R, T F G H = D-pad"
    local extra_arguments=()
    read -r -a extra_arguments <<< "${EXTRA_RUN_ARGS:-}"
    # moderngekko-port directly rather than the template's 'make run': that
    # target also downloads Wii-only tools (wit, GameTDB titles) a GameCube
    # disc never uses, and reconfigures CMake on every start.
    "$TEMPLATE_DIRECTORY/lib/ModernGekko/build/moderngekko-port" run "$EXTRACTED_DIRECTORY" \
        --backend c --toolchain auto --output "$TEMPLATE_DIRECTORY/build/modules" \
        -- --graphics "$GRAPHICS_BACKEND" "${extra_arguments[@]}"
}

main() {
    local disc_image="${1:-}"
    check_system
    install_missing_packages
    choose_graphics_backend
    download_template
    build_tools
    extract_disc "$disc_image"
    verify_disc
    run_game
}

main "$@"
