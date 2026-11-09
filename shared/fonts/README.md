# Fonts / Fuentes

[Español](#español) · [English](#english)

## Español

Las letras del launcher: M PLUS Rounded 1c (Rounded M+), una fuente redondeada
como la de los menús del juego, de licencia libre (SIL Open Font License 1.1,
en `OFL.txt`). Van dentro del programa (`shared/shared.cmake` las convierte
en un arreglo de C++).

Son un subconjunto de los archivos de Google Fonts, con solo los caracteres que
usa el launcher: el latín (con los acentos del español y de otras lenguas
europeas), la puntuación, unas flechas y figuras, la estrella, las palomitas y
las notas musicales. Así pesan unos 70 KB cada una en vez de 3,5 MB.

- Origen: `ofl/mplusrounded1c/MPLUSRounded1c-Medium.ttf` y
  `MPLUSRounded1c-ExtraBold.ttf` de https://github.com/google/fonts, en el commit
  `5bb28733cec313b63fbd415b574b5ae35c8aa8b8`.
- SHA-256 de los originales: Medium
  `adfde1b6bae58719c4e0144612a94232e72fc5ca655c4722165fe88d06521a70`, ExtraBold
  `8e7c15901dca87f1451b356dda594f7d092ba252a5dcc47da74523a242493c36`.
- Comando (fontTools 4.67), para cada una:

```sh
pyftsubset MPLUSRounded1c-Medium.ttf --output-file=RoundedMplus1c-Medium-subset.ttf \
  --unicodes="U+0020-007E,U+00A0-00FF,U+0100-017F,U+2010-2027,U+2030-205E,U+20AC,U+2190-2193,U+25A0-25A1,U+25B2-25B3,U+25B6,U+25BC,U+25C0,U+25CB,U+25CF,U+2605,U+2713-2715,U+266A-266B" \
  --layout-features="kern" --no-hinting --desubroutinize
```

## English

The launcher's lettering: M PLUS Rounded 1c (Rounded M+), a rounded font like
the one of the game's menus, under a free license (SIL Open Font License 1.1,
in `OFL.txt`). They go inside the program (`shared/shared.cmake` turns them
into a C++ array).

They are a subset of Google Fonts' files, with only the characters the launcher
uses: Latin (with the accents of Spanish and other European languages),
punctuation, a few arrows and shapes, the star, the check marks and the musical
notes. That way each weighs about 70 KB instead of 3.5 MB.

- Source: `ofl/mplusrounded1c/MPLUSRounded1c-Medium.ttf` and
  `MPLUSRounded1c-ExtraBold.ttf` from https://github.com/google/fonts, at commit
  `5bb28733cec313b63fbd415b574b5ae35c8aa8b8`.
- SHA-256 of the originals: Medium
  `adfde1b6bae58719c4e0144612a94232e72fc5ca655c4722165fe88d06521a70`, ExtraBold
  `8e7c15901dca87f1451b356dda594f7d092ba252a5dcc47da74523a242493c36`.
- Command (fontTools 4.67), for each one: the `pyftsubset` line above.
