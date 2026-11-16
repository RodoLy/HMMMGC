#!/usr/bin/env python3
"""Draw the launcher's icon: a black musical note in a soap bubble.

Dibuja el icono del launcher: una nota musical negra en una burbuja de jabón.

The bubble is see-through, as the ones that rise over the launcher's background
(look::bubble, launcher/src/look.cpp): clear in the middle and more solid towards its edge, with
the colours of a soap film (pale blue at the top, blue and lilac in the middle, purple and pink
at the bottom), the light's white shine at the top left and a smaller one at the bottom right.
The note is black, with a soft white glow so that it can be read on a dark background too. Each
size of the icon is drawn on its own, large, and then reduced, so the small ones stay sharp, and
the note is a little bigger in them to be read.

Writes launcher/windows/launcher.ico (16 to 256 pixels), which launcher/windows/launcher.rc puts
in HarvestMoonMM.exe, launcher/windows/launcher-icon.png (256 pixels), to look at, and
launcher/macos/HarvestMoonMM.icns (16 to 1024 pixels), the icon of the Mac app
(tools/package_native_macos.py puts it in the app), and the Android app's icon in each density
(android/app/src/main/res/mipmap-*/ic_launcher.png). Needs
Pillow (python3 -m pip install pillow); the icon is kept in the repository, so it is only needed
to change it.

Usage / Uso:
    python3 tools/make_launcher_icon.py
"""

import colorsys
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPOSITORY = Path(__file__).resolve().parent.parent
NOTE_FONT = REPOSITORY / "launcher" / "fonts" / "DejaVuSans-Bold-symbols.ttf"
OUTPUT = REPOSITORY / "launcher" / "windows"
MACOS_OUTPUT = REPOSITORY / "launcher" / "macos"
MACOS_SIZE = 1024  # the largest size of a Mac icon; Pillow writes the smaller ones from it
ANDROID_RESOURCES = REPOSITORY / "android" / "app" / "src" / "main" / "res"
ANDROID_SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}  # a launcher icon's
SIZES = (16, 24, 32, 48, 64, 128, 256)
DRAWN_AT = 1024  # every size is drawn this big, then reduced

SCALE = DRAWN_AT / 256
MARGIN = 6 * SCALE
DIAMETER = DRAWN_AT - 2 * MARGIN

# How solid the film is from the middle (0) to the edge (1): a bubble is mostly see-through, and
# its edge, seen at a slant, catches more colour.
CLEAR_MIDDLE = 0.45
SOLID_EDGE = 0.9
FILM_SATURATION = 0.6
FILM_HUE_TOP = 0.58  # pale blue
FILM_HUE_BOTTOM = 0.82  # purple and pink
FILM_WAVES = 0.05
EDGE_LINE = (70, 70, 110, 150)  # a thin line around it, so it stands out on a white background
NOTE = (0x10, 0x10, 0x18, 255)
NOTE_GLOW = (255, 255, 255, 120)


def shine(x: float, y: float, center: tuple[float, float], size: float) -> float:
    """The white of a round shine of `size` at `center`, from 1 in its middle to 0 at its edge."""
    t = math.hypot(x - center[0], y - center[1]) / size
    return max(0.0, 1.0 - t) ** 1.6


def soap_bubble() -> Image.Image:
    """The bubble without its note, at DRAWN_AT pixels."""
    center = DRAWN_AT / 2
    radius = DIAMETER / 2
    bubble = Image.new("RGBA", (DRAWN_AT, DRAWN_AT), (0, 0, 0, 0))
    pixels = bubble.load()
    big_shine = (center - 0.22 * DIAMETER, center - 0.24 * DIAMETER)
    small_shine = (center + 0.25 * DIAMETER, center + 0.26 * DIAMETER)
    for y in range(DRAWN_AT):
        for x in range(DRAWN_AT):
            dx, dy = x + 0.5 - center, y + 0.5 - center
            distance = math.hypot(dx, dy) / radius
            if distance > 1:
                continue
            # The film's colour: pale blue at the top, blue and lilac in the middle and purple and
            # pink at the bottom, with soft waves around the bubble and a little more colour at
            # its edge.
            down = (dy / radius + 1) / 2
            angle = math.atan2(dy, dx)
            hue = (
                FILM_HUE_TOP
                + (FILM_HUE_BOTTOM - FILM_HUE_TOP) * down
                + FILM_WAVES * math.sin(3 * angle + 4 * distance)
            )
            saturation = FILM_SATURATION * (0.45 + 0.55 * down) * (0.8 + 0.2 * distance)
            red, green, blue = colorsys.hsv_to_rgb(hue % 1.0, saturation, 1.0)
            solid = CLEAR_MIDDLE + (SOLID_EDGE - CLEAR_MIDDLE) * distance**3
            white = max(
                shine(x, y, big_shine, 0.2 * DIAMETER), 0.7 * shine(x, y, small_shine, 0.09 * DIAMETER)
            )
            # The shine is white and solid; the film shows through where there is none.
            red, green, blue = (c + (1 - c) * white for c in (red, green, blue))
            alpha = solid + (1 - solid) * white
            pixels[x, y] = (round(red * 255), round(green * 255), round(blue * 255), round(alpha * 255))

    ImageDraw.Draw(bubble).ellipse(
        (MARGIN, MARGIN, DRAWN_AT - MARGIN, DRAWN_AT - MARGIN), outline=EDGE_LINE, width=round(2 * SCALE)
    )
    return bubble


def icon(bubble: Image.Image, size: int) -> Image.Image:
    """The icon at `size` pixels, from the bubble without its note."""
    picture = bubble.copy()
    center = DRAWN_AT / 2

    # The note: bigger in the small sizes, where it has to be read.
    note_share = 0.6 if size >= 48 else 0.7
    font = ImageFont.truetype(str(NOTE_FONT), round(DIAMETER * note_share))
    note = "♪"
    left, top, right, bottom = ImageDraw.Draw(picture).textbbox((0, 0), note, font=font)
    position = (center - (left + right) / 2 + 0.02 * DIAMETER, center - (top + bottom) / 2 + 0.02 * DIAMETER)
    glow = Image.new("RGBA", picture.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).text(
        position, note, font=font, fill=NOTE_GLOW, stroke_width=round(0.012 * DIAMETER), stroke_fill=NOTE_GLOW
    )
    picture.alpha_composite(glow.filter(ImageFilter.GaussianBlur(0.015 * DIAMETER)))
    ImageDraw.Draw(picture).text(position, note, font=font, fill=NOTE)
    return picture.resize((size, size), Image.Resampling.LANCZOS)


def main() -> None:
    bubble = soap_bubble()
    icons = [icon(bubble, size) for size in SIZES]
    icons[-1].save(OUTPUT / "launcher-icon.png")
    icons[-1].save(OUTPUT / "launcher.ico", sizes=[(size, size) for size in SIZES], append_images=icons[:-1])
    MACOS_OUTPUT.mkdir(exist_ok=True)
    icon(bubble, MACOS_SIZE).save(MACOS_OUTPUT / "HarvestMoonMM.icns")
    for density, size in ANDROID_SIZES.items():
        folder = ANDROID_RESOURCES / f"mipmap-{density}"
        folder.mkdir(parents=True, exist_ok=True)
        icon(bubble, size).save(folder / "ic_launcher.png")
    written = (
        OUTPUT / "launcher.ico",
        OUTPUT / "launcher-icon.png",
        MACOS_OUTPUT / "HarvestMoonMM.icns",
        ANDROID_RESOURCES,
    )
    print("wrote " + ", ".join(str(path) for path in written))


if __name__ == "__main__":
    main()
