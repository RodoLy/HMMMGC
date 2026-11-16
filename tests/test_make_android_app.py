"""Tests of tools/make_android_app.py: the parts that do not need a disc or a compiler (the Android
package's workflow makes an app with a real kit).

Pruebas de tools/make_android_app.py: las partes que no necesitan un disco ni un compilador.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from make_android_app import NO_CONFIGURATION, PreparationError, elf_assembly, for_android  # noqa: E402


def test_data_translated_on_windows_become_elf_for_android():
    coff = '.section .rdata,"dr"\n.p2align 4\n.globl kData__init\nkData__init:\n.incbin "C:/x/_init.bin"\n'
    elf = elf_assembly(coff)
    assert '.section .rodata,"a",@progbits' in elf and ".rdata" not in elf
    assert elf.rstrip().endswith('.section .note.GNU-stack,"",@progbits')


def test_data_translated_on_linux_stay_as_they_are():
    linux = '.section .rodata,"a",@progbits\n.globl kData__init\n.section .note.GNU-stack,"",@progbits\n'
    assert elf_assembly(linux) == linux


def test_the_package_s_clang_runs_without_the_computer_s_configuration():
    recipe = {"system": "Android", "compile": ["-O2"], "assemble": ["-O2"], "link": ["-shared"]}
    adapted = for_android(recipe)
    assert adapted["compile"] == [NO_CONFIGURATION, "-O2"]
    assert adapted["link"] == [NO_CONFIGURATION, "-shared"]
    assert recipe["compile"] == ["-O2"]  # the kit's own stays as it is


def test_a_kit_for_the_computer_is_not_taken_for_the_android_one():
    with pytest.raises(PreparationError, match="for Linux"):
        for_android({"system": "Linux", "compile": [], "assemble": [], "link": []})
