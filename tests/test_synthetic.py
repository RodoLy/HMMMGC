"""The hand-written encoders in synthetic.py must agree with capstone.

Los codificadores de synthetic.py deben coincidir con capstone."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import synthetic  # noqa: E402
from ppc import Instruction, format_instruction  # noqa: E402

ADDRESS = 0x80004000


def text_of(word: int, address: int = ADDRESS) -> str:
    return " ".join(format_instruction(Instruction(address, word)).split())


def test_encoders_match_capstone():
    expected = {
        synthetic.lis(4, 0x8020): "lis r4, -0x7fe0",
        synthetic.li(3, 10): "li r3, 0xa",
        synthetic.addi(4, 4, -0x5DBC): "addi r4, r4, -0x5dbc",
        synthetic.ori(2, 2, 0xA780): "ori r2, r2, 0xa780",
        synthetic.lwz(3, -0x6FE4, 13): "lwz r3, -0x6fe4(r13)",
        synthetic.lmw(30, 8, 1): "lmw r30, 8(r1)",
        synthetic.cmplwi(3, 2): "cmplwi r3, 2",
        synthetic.slwi(0, 3, 2): "slwi r0, r3, 2",
        synthetic.lwzx(0, 4, 0): "lwzx r0, r4, r0",
        synthetic.mr(31, 3): "mr r31, r3",
        synthetic.mtctr(0): "mtctr r0",
        synthetic.BCTR: "bctr",
        synthetic.BLR: "blr",
        synthetic.b(ADDRESS, ADDRESS + 0x30): "b 0x80004030",
        synthetic.bl(ADDRESS, ADDRESS - 0x100): "bl 0x80003f00",
        synthetic.bgt(ADDRESS, ADDRESS + 0x30): "bgt 0x80004030",
    }
    for word, text in expected.items():
        assert text_of(word) == text, f"0x{word:08X}"


def test_psq_l_encoder_matches_capstone():
    # capstone prints psq_l as "psq_l f1, -8(r5), 0, 0"
    assert text_of(synthetic.psq_l(1, -8, 5)).startswith("psq_l f1, -8(r5)")


def test_high_adjusted_pairs_rebuild_the_address():
    for address in (0x80202AB4, 0x8021A244, 0x80008000, 0x80007FFC):
        high = synthetic.high_adjusted(address) << 16
        low = synthetic.low(address)
        signed_low = low - 0x10000 if low & 0x8000 else low
        assert (high + signed_low) & 0xFFFFFFFF == address
