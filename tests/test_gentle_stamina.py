"""mods/gentle_stamina, run the way the game runs it: its hook called on the entry of fn_800A4D8C with
the change the game asks for, then the change that fn_800A4D8C would make.

mods/gentle_stamina, corrido como lo corre el juego: su hook llamado a la entrada de fn_800A4D8C con el
cambio que pide el juego, y después el cambio que haría fn_800A4D8C.
"""

import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
MOD_SOURCE = REPOSITORY_ROOT / "mods" / "gentle_stamina" / "mod.c"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

DRIVER = r"""
#include <stdio.h>
#include <stdlib.h>
#include MOD_SOURCE_PATH

/* Calls the hook with each change on the command line (player 0) and prints the sum of what is left. */
int main(int count, char** arguments)
{
    moderngekko_get_mod()->on_load(NULL);
    CPUState cpu = {0};
    long total = 0;
    for (int index = 1; index < count; index++) {
        cpu.gpr[4] = 0;
        cpu.gpr[5] = (uint32_t)atoi(arguments[index]);
        spend_less_stamina(&cpu);
        total += (int32_t)cpu.gpr[5];
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
    work = tmp_path_factory.mktemp("gentle_stamina")
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
        + [f"-I{work}", str(work / "driver.c"), "-o", str(program)],
        check=True,
    )
    return program


def spent(driver: Path, percent: str | None, changes: list[int]) -> int:
    environment = {"HM_MM_STAMINA_COST_PERCENT": percent} if percent is not None else {}
    output = subprocess.run(
        [driver, *(str(change) for change in changes)],
        check=True,
        capture_output=True,
        text=True,
        env=environment,
    ).stdout
    return int(output)


# A day of work: hoe strokes (-1), the hammer (-2), the axe (-3), a long run (-5).
A_DAY = [-1] * 60 + [-2] * 20 + [-3] * 10 + [-5] * 4


def test_at_100_work_costs_what_it_costs_in_the_original_game(driver: Path) -> None:
    assert spent(driver, "100", A_DAY) == sum(A_DAY)


@pytest.mark.parametrize("percent", [10, 30, 50, 70, 90])
def test_work_costs_the_chosen_share_counting_what_is_left_of_a_point(driver: Path, percent: int) -> None:
    assert spent(driver, str(percent), A_DAY) == -(-sum(A_DAY) * percent // 100)


def test_two_strokes_of_one_point_cost_one_point_at_half(driver: Path) -> None:
    assert spent(driver, "50", [-1]) == 0
    assert spent(driver, "50", [-1, -1]) == -1


def test_what_gives_stamina_back_is_not_changed(driver: Path) -> None:
    assert spent(driver, "50", [10, 3]) == 13


@pytest.mark.parametrize("percent", [None, "", "abc", "5", "101", "50.5"])
def test_without_a_good_value_work_costs_the_mods_default_share(driver: Path, percent: str | None) -> None:
    assert spent(driver, percent, A_DAY) == spent(driver, "50", A_DAY)
