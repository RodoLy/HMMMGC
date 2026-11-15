#!/usr/bin/env python3
"""Draw the launcher's icon: the musical note in a bubble that rises over the launcher's background.

Dibuja el icono del launcher: la nota musical en una burbuja, como las que suben por el fondo del
launcher.

The bubble is drawn as look::bubble draws it (launcher/src/look.cpp): a white radial gradient lit
from the top left, under a white border, with a white note. On the launcher it floats over the
blue checkerboard; an icon goes on any background, so here it is filled with that blue. Each size
of the icon is drawn on its own, large, and then reduced, so the small ones stay sharp, and the
note is a little bigger in them to be read.

Writes launcher/windows/launcher.ico (16 to 256 pixels), which launcher/windows/launcher.rc puts
in HarvestMoonMM.exe, and launcher/windows/launcher-icon.png (256 pixels), to look at. Needs
Pillow (python3 -m pip install pillow); the icon is kept in the repository, so it is only needed
to change it.

Usage / Uso:
    python3 tools/make_launcher_icon.py
"""

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPOSITORY = Path(__file__).resolve().parent.parent
NOTE_FONT = REPOSITORY / "launcher" / "fonts" / "DejaVuSans-Bold-symbols.ttf"
OUTPUT = REPOSITORY / "launcher" / "windows"
SIZES = (16, 24, 32, 48, 64, 128, 256)
DRAWN_AT = 1024  # every size is drawn this big, then reduced

BLUE_LIGHT = (0x1B, 0x8E, 0xEA)  # the launcher's two blues (App::draw_background)
BLUE_DARK = (0x13, 0x80, 0xDC)
BORDER = (0x0F, 0x5F, 0xB0)  # a darker ring around it, so it stands out on a white background
NOTE_SHADOW = (0x17, 0x11, 0x5E, 110)  # the launcher's night blue
# look::bubble's gradient: (distance from the light, opacity of the white), the light at 32% and
# 28% of the bubble, and the distance measured in parts of the diameter * 0.99.
GRADIENT = ((0.0, 0.9), (0.1, 0.9), (0.22, 0.3), (0.6, 0.1), (1.0, 0.4))


def gradient_opacity(t: float) -> float:
    for (start, start_opacity), (end, end_opacity) in zip(GRADIENT, GRADIENT[1:], strict=False):
        if t <= end:
            part = max(0.0, (t - start) / (end - start))
            return start_opacity + (end_opacity - start_opacity) * part
    return GRADIENT[-1][1]


SCALE = DRAWN_AT / 256
MARGIN = 6 * SCALE
DIAMETER = DRAWN_AT - 2 * MARGIN


def glossy_disc() -> Image.Image:
    """The bubble without its note, at DRAWN_AT pixels: the blue, lighter at the top left, with the
    gradient of look::bubble over it, and its rings."""
    margin = MARGIN
    diameter = DIAMETER
    center = DRAWN_AT / 2
    radius = diameter / 2
    disc = Image.new("RGBA", (DRAWN_AT, DRAWN_AT), (0, 0, 0, 0))
    pixels = disc.load()
    light = (center - 0.18 * diameter, center - 0.22 * diameter)
    reach = diameter * math.hypot(0.68, 0.72)
    for y in range(DRAWN_AT):
        for x in range(DRAWN_AT):
            if math.hypot(x + 0.5 - center, y + 0.5 - center) > radius:
                continue
            shade = min(1.0, math.hypot(x - center + radius, y - center + radius) / (2 * diameter))
            blue = tuple(round(a + (b - a) * shade) for a, b in zip(BLUE_LIGHT, BLUE_DARK, strict=True))
            white = gradient_opacity(math.hypot(x - light[0], y - light[1]) / reach)
            pixels[x, y] = (*(round(c + (255 - c) * white * 0.85) for c in blue), 255)

    draw = ImageDraw.Draw(disc)
    ring = 7 * SCALE
    draw.ellipse(
        (margin, margin, DRAWN_AT - margin, DRAWN_AT - margin), outline=BORDER, width=round(ring * 0.6)
    )
    inner = margin + ring * 0.6
    draw.ellipse(
        (inner, inner, DRAWN_AT - inner, DRAWN_AT - inner), outline=(255, 255, 255, 220), width=round(ring)
    )
    return disc


def bubble(disc: Image.Image, size: int) -> Image.Image:
    """The icon at `size` pixels, from the bubble without its note."""
    disc = disc.copy()
    diameter = DIAMETER
    center = DRAWN_AT / 2
    draw = ImageDraw.Draw(disc)

    # The note: bigger in the small sizes, where the bubble's gloss takes most of the room.
    note_share = 0.62 if size >= 48 else 0.72
    font = ImageFont.truetype(str(NOTE_FONT), round(diameter * note_share))
    note = "♪"
    left, top, right, bottom = draw.textbbox((0, 0), note, font=font)
    position = (center - (left + right) / 2 + 0.02 * diameter, center - (top + bottom) / 2 + 0.02 * diameter)
    shadow = Image.new("RGBA", disc.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).text(
        (position[0] + 0.02 * diameter, position[1] + 0.025 * diameter), note, font=font, fill=NOTE_SHADOW
    )
    disc.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(0.012 * diameter)))
    ImageDraw.Draw(disc).text(position, note, font=font, fill=(255, 255, 255, 255))
    return disc.resize((size, size), Image.Resampling.LANCZOS)


def main() -> None:
    disc = glossy_disc()
    icons = [bubble(disc, size) for size in SIZES]
    icons[-1].save(OUTPUT / "launcher-icon.png")
    icons[-1].save(OUTPUT / "launcher.ico", sizes=[(size, size) for size in SIZES], append_images=icons[:-1])
    print(f"wrote {OUTPUT / 'launcher.ico'} and {OUTPUT / 'launcher-icon.png'}")


if __name__ == "__main__":
    main()
