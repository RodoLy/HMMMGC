"""mods/longer_days, run the way the game runs it: its hook called once per frame with what the clock
would add.

mods/longer_days, corrido como lo corre el juego: su hook llamado una vez por cuadro con lo que el reloj
sumaría.

The game adds the whole part of f1 (100 * the frame's time) to the part of a minute every frame. These
tests give the mod's hook a day's worth of frames and add up what fn_80037EAC would add, as the game does
(fctiwz cuts the decimals off), to check that the day lasts as long as asked.
"""

import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
MOD_SOURCE = REPOSITORY_ROOT / "mods" / "longer_days" / "mod.c"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

# The frame's time at 30 frames a second, as a float, times 100: what fn_80038088 gives.
GAME_TICK = 100.0 * (1.0 / 30.0)
FRAMES = 30 * 60 * 20  # twenty minutes of play

DRIVER = r"""
#include <stdio.h>
#include <stdlib.h>
#include MOD_SOURCE_PATH

static uint64_t no_read(CPUState* state, uint32_t address, uint8_t size)
{
    (void)state; (void)address; (void)size;
    return 0;
}

static void no_write(CPUState* state, uint32_t address, uint64_t value, uint8_t size)
{
    (void)state; (void)address; (void)value; (void)size;
}

int main(int count, char** arguments)
{
    if (count < 4) {
        return 2;
    }
    const double tick = atof(arguments[1]);
    const long frames = atol(arguments[2]);
    const int runs_per_call = atoi(arguments[3]);
    moderngekko_get_mod()->on_load(NULL);
    CPUState cpu = {0};
    cpu.external_read = no_read;
    cpu.external_write = no_write;
    long long added = 0;
    for (long frame = 0; frame < frames; frame++) {
        cpu.fpr[1] = (float)tick;
        for (int run = 0; run < runs_per_call; run++) {
            slow_the_clock_down(&cpu);
        }
        added += (long long)cpu.fpr[1]; /* fctiwz: the decimals are cut off */
    }
    printf("%lld\n", added);
    return 0;
}
"""


@pytest.fixture(scope="module")
def driver(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """The program, built once."""
    if C_COMPILER is None:
        pytest.skip("needs a C compiler")
    work = tmp_path_factory.mktemp("longer_days")
    subprocess.run(
        [sys.executable, REPOSITORY_ROOT / "tools" / "export_mod_symbols.py"]
        + ["--out", work / "G4AEE9_symbols.h"],
        check=True,
        capture_output=True,
    )
    (work / "driver.c").write_text(DRIVER, encoding="utf-8")
    program = work / "driver"
    subprocess.run(
        [C_COMPILER, "-std=c11", "-Wall", "-Wextra", "-Werror", f'-DMOD_SOURCE_PATH="{MOD_SOURCE}"']
        + [f"-I{REPOSITORY_ROOT / 'runtime' / 'mod_api'}", f"-I{REPOSITORY_ROOT / 'mods' / 'include'}"]
        + [f"-I{work}", str(work / "driver.c"), "-o", str(program), "-lm"],
        check=True,
    )
    return program


def clock_after(driver: Path, percent: str | None, runs_per_call: int = 1) -> int:
    """What the clock adds over FRAMES frames, with the day's length in the mod's variable."""
    environment = {"HM_MM_DAY_LENGTH_PERCENT": percent} if percent is not None else {}
    output = subprocess.run(
        [driver, str(GAME_TICK), str(FRAMES), str(runs_per_call)],
        check=True,
        capture_output=True,
        text=True,
        env=environment,
    ).stdout
    return int(output)


def test_at_100_the_clock_goes_as_in_the_original_game(driver: Path) -> None:
    assert clock_after(driver, "100") == int(GAME_TICK) * FRAMES


@pytest.mark.parametrize("percent", [125, 150, 200, 300, 400])
def test_a_longer_day_slows_the_clock_by_as_much(driver: Path, percent: int) -> None:
    original = int(GAME_TICK) * FRAMES
    slowed = clock_after(driver, str(percent))
    assert abs(slowed - original * 100 / percent) <= 1


def test_a_hook_run_twice_for_one_call_slows_the_clock_once(driver: Path) -> None:
    assert clock_after(driver, "200", runs_per_call=2) == clock_after(driver, "200")


@pytest.mark.parametrize("percent", [None, "", "abc", "99", "401", "150.5"])
def test_without_a_good_value_the_day_has_the_mods_default_length(driver: Path, percent: str | None) -> None:
    assert clock_after(driver, percent) == clock_after(driver, "150")
