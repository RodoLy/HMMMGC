"""tools/insert_mod_hooks.py on a small translated shard: a hook on a function that the translator inlined
into a caller runs in the inlined copy too.

tools/insert_mod_hooks.py sobre un shard traducido pequeño: un hook en una función que el traductor metió
dentro de otra también corre en esa copia.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from insert_mod_hooks import HookPoint, Shards, insert  # noqa: E402

# A caller, as the translator writes it, with the leaf 0x800A78D0 inlined, and the leaf itself.
SHARD = """#include <cstdint>
#include "guest_abi/guest_abi.h"

extern "C" void func_8001E44C(CpuContext* MKW_RESTRICT ctx)
{
    uint32_t r3 = ctx->gpr[3];
    uint32_t r4 = ctx->gpr[4];
    uint32_t cr = ctx->cr;

loc_8001ED34:
{
    r4 = 1;
    // inline leaf 0x800A78D0 (12 guest instruction(s))
}

loc_inl13_0x800A78D0:
{
    r0 = MemoryInline::FlatRead32(r3);
}

loc_inl13_cont_800A78D0:
{
    return;
}
}

extern "C" void func_800A78D0(CpuContext* MKW_RESTRICT ctx)
{
    uint32_t r3 = ctx->gpr[3];

loc_800A78D0:
{
    return;
}
}
"""


@pytest.fixture
def shards(tmp_path: Path) -> Shards:
    folder = tmp_path / "build_shards" / "base_common"
    folder.mkdir(parents=True)
    (folder / "shard_0.cpp").write_text(SHARD, encoding="utf-8")
    return Shards(tmp_path)


def text_of(shards: Shards) -> str:
    return next(iter(shards.texts.values()))


def test_an_entry_hook_runs_in_the_function_and_in_its_inlined_copy(shards: Shards) -> None:
    assert insert(shards, HookPoint(0x800A78D0, on_return=False))
    text = text_of(shards)
    assert "if (hmmm_mod_hook_entry(0x800A78D0u, ctx)) return;" in text
    copy = text.index("loc_inl13_0x800A78D0:")
    hook = text.index("hmmm_mod_hook_at(0x800A78D0u, ctx);", copy)
    # The caller's registers go to the context before the hook and come back after it.
    assert text.index("ctx->gpr[4] = r4;", copy) < hook < text.index("r4 = ctx->gpr[4];", hook)
    assert hook < text.index("r0 = MemoryInline::FlatRead32(r3);")


def test_running_it_again_adds_nothing(shards: Shards) -> None:
    insert(shards, HookPoint(0x800A78D0, on_return=False))
    first = text_of(shards)
    assert not insert(shards, HookPoint(0x800A78D0, on_return=False))
    assert text_of(shards) == first


def test_a_return_hook_on_an_inlined_function_is_refused(shards: Shards) -> None:
    with pytest.raises(SystemExit, match="inlined"):
        insert(shards, HookPoint(0x800A78D0, on_return=True))
