"""mods/faster_tools, run the way the game runs it: its hook called at the entry of fn_800A78D0(counter,
amount) with the counter of a tool, or of something else, and the amount the game adds.

mods/faster_tools, corrido como lo corre el juego: su hook llamado a la entrada de fn_800A78D0(contador,
cantidad) con el contador de una herramienta, o de otra cosa, y la cantidad que suma el juego.
"""

import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
MOD_SOURCE = REPOSITORY_ROOT / "mods" / "faster_tools" / "mod.c"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

SAVE_DATA = 0x80400000
FIRST_TOOL_COUNTER = 0xCD1C
COUNTER_BYTES = 12

DRIVER = r"""
#include <stdio.h>
#include <stdlib.h>
#include MOD_SOURCE_PATH

/* The only word the mod reads: the game's pointer to the save data. */
static uint64_t fake_read(CPUState* state, uint32_t address, uint8_t size)
{
    (void)state; (void)size;
    return address == SAVE_DATA_POINTER_ADDRESS ? SAVE_DATA : 0;
}

/* Calls the hook once per use with the counter's offset and prints the sum of the amounts it leaves. */
int main(int count, char** arguments)
{
    if (count < 4) {
        return 2;
    }
    const uint32_t counter = SAVE_DATA + (uint32_t)strtoul(arguments[1], NULL, 0);
    const int amount = atoi(arguments[2]);
    const long uses = atol(arguments[3]);
    moderngekko_get_mod()->on_load(NULL);
    CPUState cpu = {0};
    cpu.external_read = fake_read;
    long total = 0;
    for (long use = 0; use < uses; use++) {
        cpu.gpr[3] = counter;
        cpu.gpr[4] = (uint32_t)amount;
        give_more_experience(&cpu);
        total += (int32_t)cpu.gpr[4];
    }
    printf("%ld\n", total);
    return 0;
}
"""


@pytest.fixture(scope="module")
def driver(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """The program, built once."""
    if C_COMPILER is None:
        pytest.skip("needs a C compiler")
    work = tmp_path_factory.mktemp("faster_tools")
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
        + [f"-DSAVE_DATA={SAVE_DATA:#x}u"]
        + [f"-I{REPOSITORY_ROOT / 'runtime' / 'mod_api'}", f"-I{REPOSITORY_ROOT / 'mods' / 'include'}"]
        + [f"-I{work}", str(work / "driver.c"), "-o", str(program)],
        check=True,
    )
    return program


def experience(driver: Path, percent: str | None, counter: int, uses: int, amount: int = 1) -> int:
    environment = {"HM_MM_TOOL_EXPERIENCE_PERCENT": percent} if percent is not None else {}
    output = subprocess.run(
        [driver, hex(counter), str(amount), str(uses)],
        check=True,
        capture_output=True,
        text=True,
        env=environment,
    ).stdout
    return int(output)


A_TOOL = FIRST_TOOL_COUNTER + 3 * COUNTER_BYTES


def test_at_100_a_tool_gains_what_it_gains_in_the_original_game(driver: Path) -> None:
    assert experience(driver, "100", A_TOOL, 1000) == 1000


@pytest.mark.parametrize("percent", [150, 200, 250, 400])
def test_a_tool_gains_the_chosen_share_counting_what_is_left_of_a_point(driver: Path, percent: int) -> None:
    assert experience(driver, str(percent), A_TOOL, 1000) == 1000 * percent // 100


@pytest.mark.parametrize("tool", range(9))
def test_every_tool_counter_is_one_of_the_tools(driver: Path, tool: int) -> None:
    counter = FIRST_TOOL_COUNTER + tool * COUNTER_BYTES
    assert experience(driver, "200", counter, 10) == 20


@pytest.mark.parametrize(
    "counter",
    [
        FIRST_TOOL_COUNTER - COUNTER_BYTES,
        FIRST_TOOL_COUNTER + 9 * COUNTER_BYTES,
        FIRST_TOOL_COUNTER + 4,
        0x6628,
    ],
)
def test_other_counters_of_the_game_are_left_alone(driver: Path, counter: int) -> None:
    assert experience(driver, "400", counter, 10) == 10


def test_an_amount_that_takes_away_is_left_alone(driver: Path) -> None:
    assert experience(driver, "200", A_TOOL, 10, amount=-1) == -10


@pytest.mark.parametrize("percent", [None, "", "abc", "99", "401", "150.5"])
def test_without_a_good_value_tools_gain_the_mods_default_share(driver: Path, percent: str | None) -> None:
    assert experience(driver, percent, A_TOOL, 100) == experience(driver, "200", A_TOOL, 100)
