"""mods/cheats on a fake memory: each cheat, on and off, with its hooks called as the game calls them.

mods/cheats sobre una memoria falsa: cada truco, encendido y apagado, con sus hooks llamados como los
llama el juego.
"""

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
MOD_SOURCE = REPOSITORY_ROOT / "mods" / "cheats" / "mod.c"
SETTINGS_HEADER = (REPOSITORY_ROOT / "shared" / "settings.h").read_text(encoding="utf-8")
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

DRIVER = r"""
#include <stdio.h>
#include MOD_SOURCE_PATH

#define SAVE 0x80400000u
static uint8_t memory[0x10000];
static uint32_t main_state = 0;

static uint64_t fake_read(CPUState* state, uint32_t address, uint8_t size)
{
    (void)state; (void)size;
    if (address == SAVE_DATA_POINTER_ADDRESS) {
        return SAVE;
    }
    if (address == MAIN_STATE_ADDRESS) {
        return main_state;
    }
    const uint32_t at = address - SAVE;
    return ((uint32_t)memory[at] << 24) | ((uint32_t)memory[at + 1] << 16) | ((uint32_t)memory[at + 2] << 8) |
           memory[at + 3];
}

static void fake_write(CPUState* state, uint32_t address, uint64_t value, uint8_t size)
{
    (void)state; (void)size;
    const uint32_t at = address - SAVE;
    memory[at] = (uint8_t)(value >> 24);
    memory[at + 1] = (uint8_t)(value >> 16);
    memory[at + 2] = (uint8_t)(value >> 8);
    memory[at + 3] = (uint8_t)value;
}

static void set_triple(uint32_t offset, uint32_t value, uint32_t maximum)
{
    fake_write(NULL, SAVE + offset, value, 4);
    fake_write(NULL, SAVE + offset + 8, maximum, 4);
}

int main(void)
{
    moderngekko_get_mod()->on_load(NULL);
    CPUState cpu = {0};
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;

    cpu.gpr[5] = (uint32_t)-3;
    keep_the_stamina(&cpu);
    printf("stamina_change %d\n", (int)(int32_t)cpu.gpr[5]);
    cpu.gpr[5] = 10;
    keep_the_stamina(&cpu);
    printf("stamina_given_back %d\n", (int)(int32_t)cpu.gpr[5]);

    cpu.fpr[1] = 3.3333;
    stop_the_clock(&cpu);
    printf("clock_adds %d\n", (int)cpu.fpr[1]);

    set_triple(MONEY, 4517, 9999999);
    set_triple(FRIENDSHIP + 20 * TRIPLE_BYTES, 58, 1000);
    main_state = 1; /* an event */
    keep_the_money_and_the_friendships(&cpu);
    printf("money_in_an_event %u\n", (unsigned)fake_read(NULL, SAVE + MONEY, 4));
    main_state = 0; /* free play */
    keep_the_money_and_the_friendships(&cpu);
    printf("money %u\n", (unsigned)fake_read(NULL, SAVE + MONEY, 4));
    printf("friendship %u\n", (unsigned)fake_read(NULL, SAVE + FRIENDSHIP + 20 * TRIPLE_BYTES, 4));
    return 0;
}
"""


@pytest.fixture(scope="module")
def driver(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """The program, built once."""
    if C_COMPILER is None:
        pytest.skip("needs a C compiler")
    work = tmp_path_factory.mktemp("cheats")
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


def facts(driver: Path, cheats: str) -> dict[str, int]:
    output = subprocess.run(
        [driver], check=True, capture_output=True, text=True, env={"HM_MM_CHEATS": cheats}
    ).stdout
    return {name: int(value) for name, value in (line.split() for line in output.splitlines())}


ALL = "infiniteStamina,infiniteMoney,stopTime,maxFriendship"


def test_with_every_cheat_on(driver: Path) -> None:
    assert facts(driver, ALL) == {
        "stamina_change": 0,
        "stamina_given_back": 10,
        "clock_adds": 0,
        "money_in_an_event": 4517,
        "money": 999999,
        "friendship": 1000,
    }


def test_with_no_cheat_on_nothing_changes(driver: Path) -> None:
    assert facts(driver, "") == {
        "stamina_change": -3,
        "stamina_given_back": 10,
        "clock_adds": 3,
        "money_in_an_event": 4517,
        "money": 4517,
        "friendship": 58,
    }


def test_each_cheat_is_on_by_its_own_name(driver: Path) -> None:
    assert facts(driver, "stopTime")["clock_adds"] == 0
    assert facts(driver, "stopTime")["money"] == 4517
    assert facts(driver, "infiniteMoney,maxFriendshipX")["friendship"] == 58


def test_the_names_are_those_of_the_settings():
    names = re.findall(r'"(\w+)"', re.search(r"CHEAT_NAMES\[\] = \{([^}]*)\}", SETTINGS_HEADER).group(1))
    assert ",".join(names) == ALL
    source = MOD_SOURCE.read_text(encoding="utf-8")
    assert set(re.findall(r'is_in_the_list\(list, "(\w+)"\)', source)) == set(names)


def test_the_cheats_mod_is_not_listed_in_the_mods_tab():
    assert json.loads((REPOSITORY_ROOT / "mods" / "cheats" / "mod.json").read_text(encoding="utf-8"))[
        "hidden"
    ]
