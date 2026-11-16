#!/usr/bin/env python3
"""Make the player's Android app: the package's app without the game (the shell), with the game
compiled for Android added, aligned and signed, with nothing but Python's standard library.

Arma la app de Android del jugador: la app del paquete sin el juego, con el juego compilado para
Android agregado, alineada y firmada, solo con la biblioteca estándar de Python.

An Android phone only installs an app that is signed, and only installs an update of it signed with
the same key. The shell (built by the Android package's workflow) cannot be signed there, because
the game is added on the player's computer, so it is signed here, with a key of the player's that
is made once and kept in the player's folder: every app made on that computer then installs over
the one before, keeping the saves. Android's own tools for this (zipalign, apksigner) need Java and
the Android SDK, which a player does not have, so this does what they do:

  - the app is written as a zip with each stored (uncompressed) file aligned to 4 bytes, as
    Android wants resources.arsc; the game's library is compressed, and the app says that Android
    unpacks it when it installs (android:extractNativeLibs), so it needs no alignment;
  - the key is RSA of 2048 bits, with a self-signed X.509 certificate (made with the ASN.1 below);
  - the signature is Android's APK Signature Scheme v2 (source.android.com/docs/security/features/
    apksigning/v2): a block before the zip's central directory, with the SHA-256 of the app in
    chunks of 1 MiB, signed with RSASSA-PKCS1-v1_5.

The Android package's workflow checks an app made by this with Android's apksigner, and the tests
with apksigtool.

Usage / Uso:
    python3 tools/android_apk.py --shell <shell.apk> --key <key.json> --out <app.apk> \\
        --add lib/arm64-v8a/libmain.so=<libmain.so> [--add assets/settings.json=<file> ...]
"""

import argparse
import datetime
import hashlib
import json
import math
import secrets
import struct
import sys
import zipfile
from collections.abc import Sequence
from pathlib import Path

ALIGNMENT = 4  # of the stored files, as zipalign -p 4 does (resources.arsc needs it)
ALIGNMENT_FIELD = 0xD935  # the zip extra field that Android's tools pad with
KEY_BITS = 2048
PUBLIC_EXPONENT = 65537
CHUNK = 1 << 20  # the v2 scheme digests the app in chunks of 1 MiB
SIGNATURE_RSA_PKCS1_SHA256 = 0x0103  # the v2 scheme's number for RSASSA-PKCS1-v1_5 with SHA-256
V2_BLOCK_ID = 0x7109871A
SIGNING_BLOCK_MAGIC = b"APK Sig Block 42"
END_OF_CENTRAL_DIRECTORY = b"PK\x05\x06"
CERTIFICATE_NAME = "Harvest Moon MM player"  # who the certificate says signed: no name of anyone
CERTIFICATE_YEARS = 30


class ApkError(Exception):
    """The app cannot be made from what was given."""


# -- RSA, with Python's integers -------------------------------------------------------------------


def is_probable_prime(number: int, rounds: int = 40) -> bool:
    """Miller-Rabin with random bases: a composite number passes with chance below 4**-rounds."""
    if number < 2:
        return False
    for small in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if number % small == 0:
            return number == small
    odd, twos = number - 1, 0
    while odd % 2 == 0:
        odd, twos = odd // 2, twos + 1
    for _ in range(rounds):
        witness = pow(2 + secrets.randbelow(number - 3), odd, number)
        if witness in (1, number - 1):
            continue
        for _ in range(twos - 1):
            witness = pow(witness, 2, number)
            if witness == number - 1:
                break
        else:
            return False
    return True


def random_prime(bits: int) -> int:
    while True:
        # The two top bits set, so that the product of two has all its bits; odd.
        candidate = secrets.randbits(bits) | (3 << (bits - 2)) | 1
        if math.gcd(candidate - 1, PUBLIC_EXPONENT) == 1 and is_probable_prime(candidate):
            return candidate


def new_key() -> dict:
    """A new RSA key: the modulus n, the public exponent e and the private exponent d."""
    while True:
        p, q = random_prime(KEY_BITS // 2), random_prime(KEY_BITS // 2)
        n = p * q
        if p != q and n.bit_length() == KEY_BITS:
            d = pow(PUBLIC_EXPONENT, -1, (p - 1) * (q - 1))
            return {"n": n, "e": PUBLIC_EXPONENT, "d": d}


# The DER of SHA-256's identifier, which PKCS#1 v1.5 puts before the hash (RFC 8017, 9.2).
SHA256_DIGEST_INFO = bytes.fromhex("3031300d060960864801650304020105000420")


def sign(key: dict, data: bytes) -> bytes:
    """RSASSA-PKCS1-v1_5 with SHA-256 (RFC 8017, 8.2.1)."""
    size = (key["n"].bit_length() + 7) // 8
    digest_info = SHA256_DIGEST_INFO + hashlib.sha256(data).digest()
    padded = b"\x00\x01" + b"\xff" * (size - len(digest_info) - 3) + b"\x00" + digest_info
    return pow(int.from_bytes(padded, "big"), key["d"], key["n"]).to_bytes(size, "big")


# -- ASN.1 (DER) and the certificate ---------------------------------------------------------------


def der(tag: int, content: bytes) -> bytes:
    length = len(content)
    if length < 0x80:
        encoded_length = bytes([length])
    else:
        octets = length.to_bytes((length.bit_length() + 7) // 8, "big")
        encoded_length = bytes([0x80 | len(octets)]) + octets
    return bytes([tag]) + encoded_length + content


def sequence(*parts: bytes) -> bytes:
    return der(0x30, b"".join(parts))


def integer(value: int) -> bytes:
    return der(0x02, value.to_bytes(value.bit_length() // 8 + 1, "big", signed=False))


def object_identifier(dotted: str) -> bytes:
    numbers = [int(part) for part in dotted.split(".")]
    encoded = bytes([40 * numbers[0] + numbers[1]])
    for number in numbers[2:]:
        groups = [number & 0x7F]
        while number > 0x7F:
            number >>= 7
            groups.append(0x80 | (number & 0x7F))
        encoded += bytes(reversed(groups))
    return der(0x06, encoded)


NULL = der(0x05, b"")
RSA_ENCRYPTION = object_identifier("1.2.840.113549.1.1.1")
SHA256_WITH_RSA = object_identifier("1.2.840.113549.1.1.11")
COMMON_NAME = object_identifier("2.5.4.3")


def public_key_info(key: dict) -> bytes:
    """The key's SubjectPublicKeyInfo (RFC 5280), as the v2 scheme carries it."""
    rsa_public_key = sequence(integer(key["n"]), integer(key["e"]))
    return sequence(sequence(RSA_ENCRYPTION, NULL), der(0x03, b"\x00" + rsa_public_key))


def certificate(key: dict, made: datetime.datetime) -> bytes:
    """A self-signed X.509 v3 certificate of the key, valid for CERTIFICATE_YEARS."""
    name = sequence(der(0x31, sequence(COMMON_NAME, der(0x0C, CERTIFICATE_NAME.encode()))))

    def time(moment: datetime.datetime) -> bytes:  # GeneralizedTime, which any year can use
        return der(0x18, moment.strftime("%Y%m%d%H%M%SZ").encode())

    until = made.replace(year=made.year + CERTIFICATE_YEARS)
    to_be_signed = sequence(
        der(0xA0, integer(2)),  # version 3
        integer(int.from_bytes(hashlib.sha256(key["n"].to_bytes(256, "big")).digest()[:8], "big")),  # serial
        sequence(SHA256_WITH_RSA, NULL),
        name,
        sequence(time(made), time(until)),
        name,
        public_key_info(key),
    )
    return sequence(
        to_be_signed, sequence(SHA256_WITH_RSA, NULL), der(0x03, b"\x00" + sign(key, to_be_signed))
    )


def load_or_make_key(path: Path) -> dict:
    """The player's key, kept in `path`; made the first time. It must not change: Android only
    installs an update signed with the key of the app that is there."""
    if path.is_file():
        stored = json.loads(path.read_text(encoding="utf-8"))
        return {
            "n": int(stored["n"], 16),
            "e": int(stored["e"], 16),
            "d": int(stored["d"], 16),
            "certificate": bytes.fromhex(stored["certificate"]),
        }
    key = new_key()
    key["certificate"] = certificate(key, datetime.datetime.now(datetime.UTC).replace(microsecond=0))
    path.parent.mkdir(parents=True, exist_ok=True)
    stored = {
        "about": "The key that signs the Android app made on this computer. Keep it: an update of the app "
        "only installs over the one on the phone when it is signed with this key.",
        "n": format(key["n"], "x"),
        "e": format(key["e"], "x"),
        "d": format(key["d"], "x"),
        "certificate": key["certificate"].hex(),
    }
    path.write_text(json.dumps(stored, indent=2) + "\n", encoding="utf-8")
    return key


# -- The zip -------------------------------------------------------------------------------------


def aligned_extra(offset: int, name: bytes) -> bytes:
    """The extra field that puts the data of a stored file whose local header starts at `offset`
    on a multiple of ALIGNMENT, as zipalign writes it."""
    data_start = offset + 30 + len(name) + 6
    padding = -data_start % ALIGNMENT
    return struct.pack("<HHH", ALIGNMENT_FIELD, 2 + padding, ALIGNMENT) + bytes(padding)


def write_unsigned(shell: Path, additions: dict[str, Path], out: Path) -> None:
    """The shell's files, then the additions (compressed), with every stored file aligned."""
    with zipfile.ZipFile(shell) as source, zipfile.ZipFile(out, "w") as app:
        names = [entry.filename for entry in source.infolist()]
        for name in additions:
            if name in names:
                raise ApkError(f"the shell already has {name}")
        if any(
            name.startswith("META-INF/") and name.upper().endswith((".SF", ".RSA", ".EC")) for name in names
        ):
            raise ApkError(f"{shell} is signed already: the shell must be the unsigned one")
        for entry in source.infolist():
            data = source.read(entry)
            copy = zipfile.ZipInfo(entry.filename, date_time=entry.date_time)
            copy.compress_type = entry.compress_type
            copy.external_attr = entry.external_attr
            if entry.compress_type == zipfile.ZIP_STORED:
                copy.extra = aligned_extra(app.fp.tell(), entry.filename.encode())
            app.writestr(copy, data)
        for name, path in additions.items():
            added = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            added.compress_type = zipfile.ZIP_DEFLATED
            added.external_attr = 0o644 << 16
            app.writestr(added, path.read_bytes(), compresslevel=9)


def zip_sections(data: bytes) -> tuple[int, int]:
    """Where the central directory starts and where the end-of-central-directory record starts."""
    end = data.rfind(END_OF_CENTRAL_DIRECTORY, max(0, len(data) - 0x10000 - 22))
    if end < 0:
        raise ApkError("the app is not a zip")
    (directory_start,) = struct.unpack_from("<I", data, end + 16)
    return directory_start, end


def length_prefixed(data: bytes) -> bytes:
    return struct.pack("<I", len(data)) + data


def v2_digest(sections: Sequence[bytes]) -> bytes:
    """The v2 scheme's digest: SHA-256 of each 1 MiB chunk of the three sections, then of those."""
    chunk_digests = []
    for section in sections:
        for start in range(0, len(section), CHUNK):
            piece = section[start : start + CHUNK]
            chunk_digests.append(hashlib.sha256(b"\xa5" + struct.pack("<I", len(piece)) + piece).digest())
    return hashlib.sha256(b"\x5a" + struct.pack("<I", len(chunk_digests)) + b"".join(chunk_digests)).digest()


def sign_v2(unsigned: bytes, key: dict) -> bytes:
    """`unsigned` with the APK Signing Block of the v2 scheme before its central directory."""
    directory_start, end = zip_sections(unsigned)
    entries, directory, end_record = unsigned[:directory_start], unsigned[directory_start:end], unsigned[end:]
    digest = v2_digest([entries, directory, end_record])

    signed_data = (
        length_prefixed(
            length_prefixed(struct.pack("<I", SIGNATURE_RSA_PKCS1_SHA256) + length_prefixed(digest))
        )
        + length_prefixed(length_prefixed(key["certificate"]))
        + length_prefixed(b"")  # no additional attributes
    )
    signatures = length_prefixed(
        length_prefixed(
            struct.pack("<I", SIGNATURE_RSA_PKCS1_SHA256) + length_prefixed(sign(key, signed_data))
        )
    )
    signer = length_prefixed(signed_data) + signatures + length_prefixed(public_key_info(key))
    value = length_prefixed(length_prefixed(signer))  # the signers: one
    pair = struct.pack("<Q", 4 + len(value)) + struct.pack("<I", V2_BLOCK_ID) + value
    block_size = len(pair) + 8 + len(SIGNING_BLOCK_MAGIC)
    block = struct.pack("<Q", block_size) + pair + struct.pack("<Q", block_size) + SIGNING_BLOCK_MAGIC

    # The end record now says that the central directory starts after the block.
    moved_end = bytearray(end_record)
    struct.pack_into("<I", moved_end, 16, directory_start + len(block))
    return entries + block + directory + bytes(moved_end)


def make_app(shell: Path, additions: dict[str, Path], key_file: Path, out: Path) -> Path:
    if not shell.is_file():
        raise ApkError(f"there is no shell app at {shell}")
    for name, path in additions.items():
        if not path.is_file():
            raise ApkError(f"{name}: {path} is not there")
    key = load_or_make_key(key_file)
    out.parent.mkdir(parents=True, exist_ok=True)
    unsigned = out.with_name(out.name + ".unsigned")
    write_unsigned(shell, additions, unsigned)
    out.write_bytes(sign_v2(unsigned.read_bytes(), key))
    unsigned.unlink()
    return out


def addition(text: str) -> tuple[str, Path]:
    name, separator, path = text.partition("=")
    if not separator or not name or not path:
        raise argparse.ArgumentTypeError(f"expected NAME_IN_THE_APP=FILE, got {text!r}")
    return name, Path(path)


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--shell", type=Path, required=True, help="the unsigned app without the game")
    parser.add_argument(
        "--key", type=Path, required=True, help="the player's key (made when it is not there)"
    )
    parser.add_argument("--out", type=Path, required=True, help="the app to write")
    parser.add_argument("--add", type=addition, action="append", default=[], help="NAME_IN_THE_APP=FILE")
    arguments = parser.parse_args(argv)
    try:
        app = make_app(arguments.shell, dict(arguments.add), arguments.key, arguments.out)
    except (ApkError, OSError, zipfile.BadZipFile, json.JSONDecodeError, KeyError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    print(f"app in {app} ({app.stat().st_size / 2**20:.0f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
