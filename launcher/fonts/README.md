# Fonts / Fuentes

[Español](#español) · [English](#english)

## Español

Las letras del launcher, las de su diseño: **Baloo 2** (ExtraBold) para los
títulos, las teclas y los códigos y números del juego, y **Dosis** (Bold y
SemiBold) para todo lo demás. Son de licencia libre (SIL Open Font License 1.1,
en `OFL-Baloo2.txt` y `OFL-Dosis.txt`) y van dentro del programa
(`launcher/CMakeLists.txt` las convierte en arreglos de C++ con
`embed_file_as_array`).

Google Fonts las publica como fuentes variables (un archivo con todos los
pesos), pero Dear ImGui no sabe elegir un peso de una fuente variable, así que
aquí está cada peso que usa el diseño por separado, hecho con fontTools. Y son
un subconjunto, con solo el latín (con los acentos del español y de otras
lenguas europeas), la puntuación y el interletraje (`kern`): unos 45 KB cada
una.

Los símbolos que esas fuentes no tienen (las notas, las flechas, el engrane, la
flor, la cruz...) los toma el launcher, como un navegador, de las fuentes de la
computadora: en Windows, Arial y Segoe UI Symbol. Por si faltan, aquí está
**DejaVu Sans** (Bold) con solo esos símbolos (`DejaVuSans-Bold-symbols.ttf`,
4 KB), con su licencia en `LICENSE-DejaVu.txt` (la de Bitstream Vera; los
cambios de DejaVu son de dominio público).

- Origen de Baloo 2 y Dosis: `ofl/baloo2/Baloo2[wght].ttf` y
  `ofl/dosis/Dosis[wght].ttf` de https://github.com/google/fonts, en el commit
  `5bb28733cec313b63fbd415b574b5ae35c8aa8b8`.
- SHA-256 de los originales: Baloo 2
  `d47a6852548059b1db49a1319d06d499d546c3fa2237cf9eee9c43c8abb025c2`, Dosis
  `b2238eef0b3464904fed097ef274c704bc63d21407369b7d7a5be7b0821a0e82`.
- Origen de DejaVu Sans: `ttf/DejaVuSans-Bold.ttf` de
  `dejavu-fonts-ttf-2.37.zip`, de
  https://github.com/dejavu-fonts/dejavu-fonts/releases/tag/version_2_37. SHA-256
  del archivo: `e6476c1b80502924294eed40894c5b18e06c181444ca953e5334262df9c27724`.
- Comandos (fontTools 4.67):

```sh
fonttools varLib.instancer "Baloo2[wght].ttf" wght=800 --update-name-table -o Baloo2-ExtraBold.ttf
fonttools varLib.instancer "Dosis[wght].ttf" wght=700 --update-name-table -o Dosis-Bold.ttf
fonttools varLib.instancer "Dosis[wght].ttf" wght=600 --update-name-table -o Dosis-SemiBold.ttf
# y para cada uno de los tres:
pyftsubset Baloo2-ExtraBold.ttf --output-file=Baloo2-ExtraBold-subset.ttf \
  --unicodes="U+0020-007E,U+00A0-00FF,U+0100-017F,U+2010-2027,U+2030-205E,U+20AC" \
  --layout-features="kern" --no-hinting --desubroutinize
pyftsubset DejaVuSans-Bold.ttf --output-file=DejaVuSans-Bold-symbols.ttf \
  --unicodes="U+2190-2193,U+2261,U+25A1,U+25B2-25B3,U+25B6,U+25BC,U+25C0,U+25CB,U+25CE,U+261E,U+2669-266C,U+2699,U+2715,U+271A,U+273F" \
  --layout-features="" --no-hinting
```

## English

The launcher's lettering, its design's: **Baloo 2** (ExtraBold) for the
titles, the keys and the game's codes and numbers, and **Dosis** (Bold and
SemiBold) for everything else. They are under a free license (SIL Open Font
License 1.1, in `OFL-Baloo2.txt` and `OFL-Dosis.txt`) and go inside the program
(`launcher/CMakeLists.txt` turns them into C++ arrays with
`embed_file_as_array`).

Google Fonts publishes them as variable fonts (one file with every weight), but
Dear ImGui cannot choose a weight of a variable font, so each weight the design
uses is here on its own, made with fontTools. And they are a subset, with only
Latin (with the accents of Spanish and of other European languages), the
punctuation and the kerning (`kern`): about 45 KB each.

The symbols those fonts do not have (the notes, the arrows, the gear, the
flower, the cross...) the launcher takes, as a browser does, from the
computer's fonts: on Windows, Arial and Segoe UI Symbol. In case they are
missing, **DejaVu Sans** (Bold) is here with only those symbols
(`DejaVuSans-Bold-symbols.ttf`, 4 KB), with its license in `LICENSE-DejaVu.txt`
(Bitstream Vera's; DejaVu's changes are in the public domain).

- Source of Baloo 2 and Dosis: `ofl/baloo2/Baloo2[wght].ttf` and
  `ofl/dosis/Dosis[wght].ttf` from https://github.com/google/fonts, at commit
  `5bb28733cec313b63fbd415b574b5ae35c8aa8b8`.
- SHA-256 of the originals: Baloo 2
  `d47a6852548059b1db49a1319d06d499d546c3fa2237cf9eee9c43c8abb025c2`, Dosis
  `b2238eef0b3464904fed097ef274c704bc63d21407369b7d7a5be7b0821a0e82`.
- Source of DejaVu Sans: `ttf/DejaVuSans-Bold.ttf` from
  `dejavu-fonts-ttf-2.37.zip`, from
  https://github.com/dejavu-fonts/dejavu-fonts/releases/tag/version_2_37. SHA-256
  of the file: `e6476c1b80502924294eed40894c5b18e06c181444ca953e5334262df9c27724`.
- Commands (fontTools 4.67): the ones above.
