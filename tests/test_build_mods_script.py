"""scripts/build_mods.sh stops with a clear message when it cannot build.
No game data needed.

scripts/build_mods.sh se detiene con un mensaje claro cuando no puede compilar.
No necesita datos del juego."""

import os
import subprocess
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "scripts" / "build_mods.sh"


def run_build_mods(template_directory: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    environment = {**os.environ, "MODERNGEKKO_TEMPLATE": str(template_directory)}
    return subprocess.run(
        ["bash", str(SCRIPT), *arguments],
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_missing_moderngekko_points_to_the_script_that_downloads_it(tmp_path):
    result = run_build_mods(tmp_path / "not-downloaded")

    assert result.returncode != 0
    assert "ModernGekko was not found" in result.stderr
    assert "scripts/build_and_run_linux.sh" in result.stderr


def test_a_folder_that_is_not_moderngekko_is_rejected_too(tmp_path):
    # A half-downloaded template has the folder but not the headers inside it.
    (tmp_path / "lib" / "ModernGekko").mkdir(parents=True)

    result = run_build_mods(tmp_path)

    assert result.returncode != 0
    assert "ModernGekko was not found" in result.stderr
