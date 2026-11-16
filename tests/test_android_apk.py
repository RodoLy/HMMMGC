"""Tests of tools/android_apk.py with a made-up shell app. The signature is checked here the way
Android checks it (the v2 block's digest of the app, and the RSA signature with the public key);
the Android package's workflow also checks a real app with Android's own apksigner.

Pruebas de tools/android_apk.py con una app base inventada. La firma se comprueba como la comprueba
Android.
"""

import hashlib
import json
import os
import struct
import sys
import zipfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import android_apk  # noqa: E402
from android_apk import (  # noqa: E402
    SHA256_DIGEST_INFO,
    SIGNING_BLOCK_MAGIC,
    ApkError,
    make_app,
    v2_digest,
)


@pytest.fixture(scope="module")
def key_file(tmp_path_factory) -> Path:
    """One key for the module: making one takes a second or two."""
    path = tmp_path_factory.mktemp("key") / "android-key.json"
    android_apk.load_or_make_key(path)
    return path


@pytest.fixture
def shell(tmp_path) -> Path:
    path = tmp_path / "shell.apk"
    with zipfile.ZipFile(path, "w") as app:
        app.writestr("AndroidManifest.xml", os.urandom(3000), compress_type=zipfile.ZIP_DEFLATED)
        app.writestr("resources.arsc", os.urandom(4097), compress_type=zipfile.ZIP_STORED)
        app.writestr("classes.dex", os.urandom(50_000), compress_type=zipfile.ZIP_DEFLATED)
        app.writestr("res/icon.png", os.urandom(1001), compress_type=zipfile.ZIP_STORED)
    return path


def data_offset(path: Path, entry: zipfile.ZipInfo) -> int:
    with path.open("rb") as file:
        file.seek(entry.header_offset)
        header = file.read(30)
    name_length, extra_length = struct.unpack("<HH", header[26:30])
    return entry.header_offset + 30 + name_length + extra_length


def read_v2_signer(app: bytes) -> tuple[bytes, bytes, bytes, int, int]:
    """The signed data, the signature, the public key, and where the block starts and ends."""
    end = app.rfind(b"PK\x05\x06")
    (directory_start,) = struct.unpack_from("<I", app, end + 16)
    assert app[directory_start - 16 : directory_start] == SIGNING_BLOCK_MAGIC
    (block_size,) = struct.unpack_from("<Q", app, directory_start - 24)
    block_start = directory_start - block_size - 8
    pair_length, pair_id = struct.unpack_from("<QI", app, block_start + 8)
    assert pair_id == 0x7109871A

    def prefixed(at: int) -> tuple[bytes, int]:
        (length,) = struct.unpack_from("<I", app, at)
        return app[at + 4 : at + 4 + length], at + 4 + length

    signers, _ = prefixed(block_start + 8 + 12)
    signer = signers[4:]  # one signer, length-prefixed
    (signed_length,) = struct.unpack_from("<I", signer, 0)
    signed_data = signer[4 : 4 + signed_length]
    rest = signer[4 + signed_length :]
    (signatures_length,) = struct.unpack_from("<I", rest, 0)
    signature = rest[4 + 4 + 4 + 4 : 4 + signatures_length]
    public_key = rest[4 + signatures_length + 4 :]
    return signed_data, signature, public_key, block_start, directory_start


def test_the_app_has_the_shell_and_the_game_with_its_stored_files_aligned(shell, key_file, tmp_path):
    library = tmp_path / "libmain.so"
    library.write_bytes(os.urandom(300_000))
    app = make_app(shell, {"lib/arm64-v8a/libmain.so": library}, key_file, tmp_path / "game.apk")
    with zipfile.ZipFile(app) as opened:
        assert opened.read("lib/arm64-v8a/libmain.so") == library.read_bytes()
        entries = {entry.filename: entry for entry in opened.infolist()}
        assert entries["lib/arm64-v8a/libmain.so"].compress_type == zipfile.ZIP_DEFLATED
        assert entries["resources.arsc"].compress_type == zipfile.ZIP_STORED
        for entry in opened.infolist():
            if entry.compress_type == zipfile.ZIP_STORED:
                assert data_offset(app, entry) % 4 == 0, entry.filename
        assert opened.testzip() is None


def test_the_v2_signature_is_of_the_app_and_verifies_with_the_public_key(shell, key_file, tmp_path):
    library = tmp_path / "libmain.so"
    library.write_bytes(os.urandom(2_500_000))  # more than one chunk of 1 MiB
    app = make_app(shell, {"lib/arm64-v8a/libmain.so": library}, key_file, tmp_path / "game.apk").read_bytes()
    signed_data, signature, public_key, block_start, directory_start = read_v2_signer(app)

    # The digest is of the app without the block, with the end record pointing where the block starts.
    end = app.rfind(b"PK\x05\x06")
    end_record = bytearray(app[end:])
    struct.pack_into("<I", end_record, 16, block_start)
    digest = v2_digest([app[:block_start], app[directory_start:end], bytes(end_record)])
    assert digest in signed_data

    key = json.loads(key_file.read_text())
    n, e = int(key["n"], 16), int(key["e"], 16)
    assert android_apk.public_key_info({"n": n, "e": e}) == public_key
    recovered = pow(int.from_bytes(signature, "big"), e, n).to_bytes(len(signature), "big")
    assert recovered.endswith(SHA256_DIGEST_INFO + hashlib.sha256(signed_data).digest())
    assert recovered.startswith(b"\x00\x01\xff")


def test_the_key_is_kept_and_used_again(shell, key_file, tmp_path):
    before = key_file.read_text()
    first = make_app(shell, {}, key_file, tmp_path / "one.apk").read_bytes()
    second = make_app(shell, {}, key_file, tmp_path / "two.apk").read_bytes()
    assert key_file.read_text() == before
    assert read_v2_signer(first)[2] == read_v2_signer(second)[2]


def test_a_shell_that_is_signed_or_that_has_the_game_already_is_refused(shell, key_file, tmp_path):
    library = tmp_path / "libmain.so"
    library.write_bytes(b"x")
    with zipfile.ZipFile(shell, "a") as app:
        app.writestr("lib/arm64-v8a/libmain.so", b"old")
    with pytest.raises(ApkError, match="already has lib/arm64-v8a/libmain.so"):
        make_app(shell, {"lib/arm64-v8a/libmain.so": library}, key_file, tmp_path / "game.apk")
    with zipfile.ZipFile(shell, "a") as app:
        app.writestr("META-INF/CERT.RSA", b"signature")
    with pytest.raises(ApkError, match="signed already"):
        make_app(shell, {}, key_file, tmp_path / "game.apk")


def test_the_primes_are_primes():
    assert android_apk.is_probable_prime(2**127 - 1)
    assert not android_apk.is_probable_prime(2**127 + 1)
    assert not android_apk.is_probable_prime(561)  # a Carmichael number
