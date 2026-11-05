"""Tests of the patches in patches/moderngekko. They need none of the code the patches are for.

Pruebas de los parches de patches/moderngekko. No necesitan nada del código al que se aplican.

The patches change code that this project does not own, and they only prove themselves when they
are applied to it, which happens on the build machines of GitHub. What can be checked from outside
is that every patch tells whoever opens it what it is for and what it applies to, that it is
explained in both languages in patches/README.md, and that it can be applied on Windows.
"""

from pathlib import Path

import pytest

REPOSITORY = Path(__file__).resolve().parent.parent
PATCHES_DIRECTORY = REPOSITORY / "patches" / "moderngekko"
PATCHES_README = REPOSITORY / "patches" / "README.md"
PATCH_FILES = sorted(PATCHES_DIRECTORY.glob("*.patch"))

# The line that starts the changes themselves. What comes before it is the header for people.
FIRST_CHANGE_MARKER = "diff --git "
# The README has one part in Spanish and, after it, the same part in English.
ENGLISH_PART_HEADING = "\n## English\n"


def patch_header(patch: Path) -> str:
    """What the patch says about itself, before its first change."""
    return patch.read_text(encoding="utf-8").split(FIRST_CHANGE_MARKER, 1)[0]


def test_there_are_patches_to_check():
    assert PATCH_FILES, f"no patches found in {PATCHES_DIRECTORY}"


@pytest.mark.parametrize("patch", PATCH_FILES, ids=lambda patch: patch.name)
def test_every_patch_starts_with_a_title_in_each_language_and_says_what_it_applies_to(patch):
    english_title, spanish_title, blank_line, *_ = patch_header(patch).splitlines()
    assert english_title.strip() and spanish_title.strip()
    assert blank_line == ""
    header = patch_header(patch)
    assert "Applies to / Se aplica a:" in header
    assert "License / Licencia:" in header
    assert "GPL-" in header
    assert "patches/README.md" in header  # where to read how to apply it


@pytest.mark.parametrize("patch", PATCH_FILES, ids=lambda patch: patch.name)
def test_every_patch_has_changes_and_ends_its_last_line(patch):
    text = patch.read_text(encoding="utf-8")
    assert FIRST_CHANGE_MARKER in text
    assert text.endswith("\n")


@pytest.mark.parametrize("patch", PATCH_FILES, ids=lambda patch: patch.name)
def test_every_patch_uses_unix_line_ends(patch):
    # git apply compares the lines of the patch with the lines of the file. On Windows a patch with
    # CRLF line ends does not match files that end their lines with LF, which is how the workflow
    # keeps them.
    assert b"\r" not in patch.read_bytes()


@pytest.mark.parametrize("patch", PATCH_FILES, ids=lambda patch: patch.name)
def test_every_patch_is_explained_in_both_languages_of_the_readme(patch):
    spanish_part, english_part = PATCHES_README.read_text(encoding="utf-8").split(ENGLISH_PART_HEADING, 1)
    heading = f"### `moderngekko/{patch.name}`"
    assert heading in spanish_part, f"{patch.name} is not explained in Spanish in patches/README.md"
    assert heading in english_part, f"{patch.name} is not explained in English in patches/README.md"


def test_every_patch_the_readme_explains_exists():
    readme = PATCHES_README.read_text(encoding="utf-8")
    explained = {
        line.split("`")[1].removeprefix("moderngekko/")
        for line in readme.splitlines()
        if line.startswith("### `moderngekko/")
    }
    assert explained == {patch.name for patch in PATCH_FILES}
