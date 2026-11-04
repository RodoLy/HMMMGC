# Third-party notices / Avisos de terceros

[Español](#español) · [English](#english)

---

## Español

Este paquete junta software libre de varios autores. El juego **no** está incluido. La tabla dice
qué es cada parte, dónde está en el paquete, con qué licencia se distribuye y dónde está su código
fuente. Los textos de las licencias de ModernGekko y de Dolphin están en la carpeta `licenses`; los
de las herramientas de `toolchain` están dentro de la carpeta de cada una. Las versiones y los
commits exactos con los que se hizo el paquete están en `VERSIONS.txt`.

**Código fuente de los programas de `bin`** (GPL): los repositorios de la tabla, en el commit que
dice `VERSIONS.txt`, junto con los parches de la carpeta `patches` de
<https://github.com/RodoLy/HMMMGC>. Si algo de eso no se puede conseguir allí, pídelo abriendo un
*issue* en ese repositorio.

## English

This package puts together free software from several authors. The game is **not** included. The
table says what each part is, where it is in the package, the license it is distributed under, and
where its source code is. The license texts of ModernGekko and Dolphin are in the `licenses`
folder; those of the tools in `toolchain` are inside each tool's folder. The exact versions and
commits this package was made with are in `VERSIONS.txt`.

**Source code of the programs in `bin`** (GPL): the repositories in the table, at the commit that
`VERSIONS.txt` gives, together with the patches in the `patches` folder of
<https://github.com/RodoLy/HMMMGC>. If any of that cannot be obtained there, ask for it by opening
an issue in that repository.

---

| Component / Componente | In the package / En el paquete | License / Licencia | Source / Fuente |
| --- | --- | --- | --- |
| This project: scripts and tools / Este proyecto: scripts y herramientas | `Play.bat`, `play.ps1`, `game.psd1` | GPL-3.0-or-later | <https://github.com/RodoLy/HMMMGC> |
| ModernGekko: launcher, runner, `moderngekko-port` | `bin\` | GPL-3.0 | <https://github.com/ExpansionPak/ModernGekko> |
| DolRecomp: the recompiler / el recompilador | `bin\dolrecomp.exe` | GPL-3.0 | <https://github.com/ExpansionPak/DolRecomp> |
| Dolphin: core, disc reader and video, inside ModernGekko / núcleo, lector de discos y video, dentro de ModernGekko | `bin\` | GPL-2.0-or-later, and the libraries it bundles under their own licenses (`licenses\Dolphin-externals-licenses.md`) | <https://github.com/dolphin-emu/dolphin> |
| llvm-mingw: clang, lld, LLVM and the MinGW-w64 headers and libraries | `toolchain\llvm-mingw` | Apache-2.0 with LLVM exceptions, and the licenses of MinGW-w64 (see `LICENSE.TXT` in that folder) | <https://github.com/mstorsjo/llvm-mingw> |
| CMake | `toolchain\cmake` | BSD-3-Clause (see `doc\cmake\LICENSE.rst`) | <https://github.com/Kitware/CMake> |
| Ninja | `toolchain\ninja` | Apache-2.0 | <https://github.com/ninja-build/ninja> |
| Python | `toolchain\python` | PSF License (see `LICENSE.txt` in that folder) | <https://www.python.org/> |
| Microsoft Visual C++ runtime / el runtime de Visual C++ (`vcruntime140*.dll`, `msvcp140*.dll`) | `bin\` | Microsoft's terms for redistributable files / Términos de Microsoft para archivos redistribuibles | <https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files> |
