"""The shell helpers of scripts/build_and_run_linux.sh. No game data needed.

Los ayudantes de shell de scripts/build_and_run_linux.sh. No necesita datos del juego.

The script runs main() only when it is executed, so these tests source it and
call one function at a time.

El script ejecuta main() solo cuando se ejecuta, así que estas pruebas lo cargan
con "source" y llaman a una función a la vez."""

import os
import subprocess
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "build_and_run_linux.sh"
LIGHT_RESOLUTION_LINE = "resolution=640x528"


def call_script_function(function_name: str, user_directory: Path) -> subprocess.CompletedProcess[str]:
    """Source the script in a fresh bash and call one of its functions.

    The script path travels as $1, not $0: the script decides whether it was
    executed or sourced by comparing its own path with $0.
    """
    environment = {
        **os.environ,
        "MODERNGEKKO_USER_DIR": str(user_directory),
        "GRAPHICS_BACKEND": "Vulkan",
    }
    return subprocess.run(
        ["bash", "-c", f'source "$1" && {function_name}', "caller", str(SCRIPT)],
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )


def test_sourcing_the_script_does_not_start_anything():
    result = subprocess.run(
        ["bash", "-c", 'source "$1" && echo sourced', "caller", str(SCRIPT)],
        capture_output=True,
        text=True,
        check=True,
    )
    # main() would have printed "==> Checking the system" first.
    assert result.stdout.strip() == "sourced"


def test_first_run_without_gpu_writes_the_light_resolution(tmp_path):
    user_directory = tmp_path / "moderngekko"

    result = call_script_function("use_light_resolution_without_gpu", user_directory)

    config_lines = (user_directory / "config.ini").read_text().splitlines()
    assert "[Video]" in config_lines
    assert LIGHT_RESOLUTION_LINE in config_lines
    assert "640x528" in result.stdout


def test_the_light_config_is_accepted_by_moderngekkos_parser(tmp_path):
    # ModernGekko skips lines that start with '#' or '[' and splits the others
    # at '=', so a stray line without '=' would make it reject the file.
    user_directory = tmp_path / "moderngekko"
    call_script_function("use_light_resolution_without_gpu", user_directory)

    for line in (user_directory / "config.ini").read_text().splitlines():
        assert line.startswith(("#", "[")) or "=" in line, line


@pytest.mark.parametrize("existing_resolution", ["1920x1080", "3840x2112"])
def test_an_existing_config_is_never_changed_and_only_mentioned(tmp_path, existing_resolution):
    user_directory = tmp_path / "moderngekko"
    user_directory.mkdir()
    chosen_config = f"[Video]\nresolution={existing_resolution}\n"
    (user_directory / "config.ini").write_text(chosen_config)

    result = call_script_function("use_light_resolution_without_gpu", user_directory)

    assert (user_directory / "config.ini").read_text() == chosen_config
    assert LIGHT_RESOLUTION_LINE in result.stdout


def test_a_config_that_already_is_light_gets_no_hint(tmp_path):
    user_directory = tmp_path / "moderngekko"
    user_directory.mkdir()
    (user_directory / "config.ini").write_text(f"[Video]\n{LIGHT_RESOLUTION_LINE}\n")

    result = call_script_function("use_light_resolution_without_gpu", user_directory)

    assert result.stdout == ""
