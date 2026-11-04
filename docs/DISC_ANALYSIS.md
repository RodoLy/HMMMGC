# Disc analysis / Análisis del disco

Target: `Harvest Moon: Magical Melody (USA)` · Game ID `G4AEE9` · Disc 1 · Revision 0

[Español](#español) · [English](#english)

---

## Español

### Imagen de disco

| Dato | Valor |
|---|---|
| Formato probado | RVZ (Zstandard, bloques de 128 KiB, sin pérdida) |
| Título interno | `Harvest Moon Magical Melody` |
| Game ID | `G4AEE9` (`G4AE` juego, `E9` = Natsume) |
| Revisión | 0 |
| `sys/main.dol` | 2 819 552 bytes, SHA-1 `5977972cd798d60ecfcf5c1f26a86dcecc36446f` |
| Archivos extraídos | 7 926 archivos, 194 MB (ver `notes/files.sha1`) |

### Layout del DOL

```
entry 0x80003154   bss 0x802AED40 (+0x73A24)
text0  file 0x00000100  addr 0x80003100  size 0x000024E0   <- init / crt0
text1  file 0x000025E0  addr 0x800184C0  size 0x001B6520   <- todo el código (1.7 MB)
data0  file 0x001B8B00  addr 0x800055E0  size 0x0000B500
data1  file 0x001C4000  addr 0x80010AE0  size 0x000079E0
data2  file 0x001CB9E0  addr 0x801CE9E0  size 0x00000060
data3  file 0x001CBA40  addr 0x801CEA40  size 0x00000020
data4  file 0x001CBA60  addr 0x801CEA60  size 0x00020860
data5  file 0x001EC2C0  addr 0x801EF2C0  size 0x000BFA80   <- datos grandes (768 KB)
data6  file 0x002ABD40  addr 0x80321080  size 0x00000C20
data7  file 0x002AC960  addr 0x80322780  size 0x00003C80
```

### Toolchain y SDK

- Compilador: Metrowerks CodeWarrior (hay cadenas del *Metrowerks Target Resident Kernel*, el debugger TRK quedó linkeado).
- Dolphin SDK: build de **Nov 10 2004** (OS, GX) y Apr 2004 (resto). Librerías presentes: AI, AR, ARQ, AX, CARD, DSP, DVD, EXI, GX, OS, PAD, SI, VI.
- Código C++ con asserts activos: el binario conserva nombres de archivos fuente.

Archivos fuente detectados en el DOL:

```
Building.cpp  CDColiseum.cpp  CastControl.cpp  Cell.cpp  Field.cpp
FurnitureArrange.cpp  KusaKeiba.cpp  LiveStock.cpp  LiveStockDb.cpp
Parameter.cpp  Sound.cpp  UnitArrange.cpp  WinRadar.cpp  shipment.cpp
lsrun.cpp  lsrun_bltin.cpp            <- intérprete de scripts (ver abajo)
GCN_Mem_Alloc.c  dvdfs.c  gcaxSndMemory.c  opdvd.c  texPalette.c  vi.c
```

### Sistema de archivos

| Carpeta | Archivos | Tamaño | Contenido |
|---|---|---|---|
| `graphic/` | 6 107 | 163 MB | Modelos, texturas, animaciones |
| `sound/` | 121 | 16 MB | Audio (AX/DSP) |
| `menu/` | 222 | 3.8 MB | Interfaz |
| `script/` | 292 | 1.5 MB | Scripts de eventos `.lsb` |
| `message/` | 217 | 1.3 MB | Texto japonés |
| `message_e/` | 208 | 1.1 MB | Texto inglés |
| `message_s/` | 179 | 1.0 MB | **Texto español** |
| `message_f/` `message_g/` `message_i/` | ~178 c/u | ~1 MB c/u | Francés, alemán, italiano |
| `attribute/` | 32 | 432 KB | Tablas de datos del juego |
| `system/` | 8 | 304 KB | Datos de sistema (`MGSUIEI.BIN`, etc.) |
| `route/` | 2 | 32 KB | Rutas de NPC |
| `opening*.bnr` | 4 | | Banners (JP, E, EU) |

Hallazgos:

- **El disco USA ya incluye los cinco idiomas europeos.** La traducción al
  español no es trabajo nuestro: hay que exponer el selector de idioma.
- Quedaron archivos `vssver.scc` (Visual SourceSafe) en varias carpetas:
  residuos del entorno de desarrollo original.
- Los banners `opening_eu.bnr` y los scripts `*_eu.lsb` sugieren que esta
  build ya contemplaba la versión europea.

### Scripts de eventos (`.lsb`)

Bytecode de un intérprete propio implementado en `lsrun.cpp` /
`lsrun_bltin.cpp` (built-ins). Cabecera observada:

```
ff ff ff ff  00 00 0c 9f  ff ff ff ff  00 00 1c 8b  00 00 00 00  01 ...
```

El formato está documentado en [`SCRIPTS.md`](SCRIPTS.md) y
`python3 tools/lsb.py NOMBRE` desensambla cualquier script.

Familias de scripts: `Eve_A##` (eventos de notas), `Eve_love##`, `Eve_good##`,
`Eve_marry##`, `Eve_child##`, `Eve_conce##`, `Eve_pm_##`, `eve_pw_##`,
`Eve_EP##` (epílogo), `Eve_rm##`, `Eve_title`, `Eve_Marry`, `talk`, `eat`,
`tone`, `dolphin`.

### La boda con Jamie

| Script | Tamaño | Texto (`message_e/event/`) |
|---|---|---|
| `Eve_marry00.lsb` … `Eve_marry21.lsb` | 1–2 KB | `eve_marry00_e.bin` (consejo antes de casarse), `eve_marry02_e.bin` (boda normal) |
| `Eve_rm00.lsb` | 2 639 B | `eve_rm00_e.bin` |
| `Eve_rm01.lsb` / `_e` / `_eu` | 7 307 B | `eve_rm01_e.bin` — **boda con Jamie** |
| `Eve_rm02.lsb` / `_e` / `_eu` | 7 271 B | (usa el texto de `eve_rm01_e.bin`; la elige `fn_80166888` cuando `save+0x227C != 0`, que parece ser el sexo del jugador) |

`eve_rm01_e.bin` contiene la ceremonia con Jamie, el discurso de la Diosa
("Jamie, be happy. I will always be watching over you.") y la tarjeta
"Several Seasons Later...", con la que cierra la ceremonia. El script es 4–7×
más grande que una boda normal por esa ceremonia, no por un final embebido.

Resuelto: el fin de juego no está en los scripts. Lo decide código nativo: con
el flag 230 puesto, `fn_80166888` encadena `Eve_rm00`, `Eve_rm01`/`Eve_rm02` y
`Eve_EP00`-`Eve_EP08`, y al terminar `Eve_EP08` el juego vuelve al título
(estado 2). Evidencia, direcciones y diseño del mod en
[`SCRIPTS.md`](SCRIPTS.md).

---

## English

### Disc image

| Field | Value |
|---|---|
| Format tested | RVZ (Zstandard, 128 KiB blocks, lossless) |
| Internal title | `Harvest Moon Magical Melody` |
| Game ID | `G4AEE9` (`G4AE` game, `E9` = Natsume) |
| Revision | 0 |
| `sys/main.dol` | 2,819,552 bytes, SHA-1 `5977972cd798d60ecfcf5c1f26a86dcecc36446f` |
| Extracted files | 7,926 files, 194 MB (see `notes/files.sha1`) |

### DOL layout

```
entry 0x80003154   bss 0x802AED40 (+0x73A24)
text0  file 0x00000100  addr 0x80003100  size 0x000024E0   <- init / crt0
text1  file 0x000025E0  addr 0x800184C0  size 0x001B6520   <- all game code (1.7 MB)
data0  file 0x001B8B00  addr 0x800055E0  size 0x0000B500
data1  file 0x001C4000  addr 0x80010AE0  size 0x000079E0
data2  file 0x001CB9E0  addr 0x801CE9E0  size 0x00000060
data3  file 0x001CBA40  addr 0x801CEA40  size 0x00000020
data4  file 0x001CBA60  addr 0x801CEA60  size 0x00020860
data5  file 0x001EC2C0  addr 0x801EF2C0  size 0x000BFA80   <- large data (768 KB)
data6  file 0x002ABD40  addr 0x80321080  size 0x00000C20
data7  file 0x002AC960  addr 0x80322780  size 0x00003C80
```

The linker names of these sections (`.init`, `extab`, `.text`, ...) are in
[FUNCTION_MAP.md](FUNCTION_MAP.md).

### Toolchain and SDK

- Compiler: Metrowerks CodeWarrior (the *Metrowerks Target Resident Kernel*
  debug stub is linked in).
- Dolphin SDK: **Nov 10 2004** build (OS, GX) and Apr 2004 (the rest).
  Libraries present: AI, AR, ARQ, AX, CARD, DSP, DVD, EXI, GX, OS, PAD, SI, VI.
- C++ with asserts enabled, so original source file names survive in the
  binary:

```
Building.cpp  CDColiseum.cpp  CastControl.cpp  Cell.cpp  Field.cpp
FurnitureArrange.cpp  KusaKeiba.cpp  LiveStock.cpp  LiveStockDb.cpp
Parameter.cpp  Sound.cpp  UnitArrange.cpp  WinRadar.cpp  shipment.cpp
lsrun.cpp  lsrun_bltin.cpp            <- script interpreter (see below)
GCN_Mem_Alloc.c  dvdfs.c  gcaxSndMemory.c  opdvd.c  texPalette.c  vi.c
```

### Filesystem

| Folder | Files | Size | Contents |
|---|---|---|---|
| `graphic/` | 6,107 | 163 MB | Models, textures, animations |
| `sound/` | 121 | 16 MB | Audio (AX/DSP) |
| `menu/` | 222 | 3.8 MB | User interface |
| `script/` | 292 | 1.5 MB | `.lsb` event scripts |
| `message/` | 217 | 1.3 MB | Japanese text |
| `message_e/` | 208 | 1.1 MB | English text |
| `message_s/` | 179 | 1.0 MB | **Spanish text** |
| `message_f/` `message_g/` `message_i/` | ~178 each | ~1 MB each | French, German, Italian |
| `attribute/` | 32 | 432 KB | Game data tables |
| `system/` | 8 | 304 KB | System data (`MGSUIEI.BIN`, ...) |
| `route/` | 2 | 32 KB | NPC routes |
| `opening*.bnr` | 4 | | Banners (JP, E, EU) |

Key findings:

- **The USA disc already ships all five European languages**, including a
  full Spanish text set in `message_s/`. Localisation is a matter of exposing
  the language selector, not translating.
- Stray `vssver.scc` (Visual SourceSafe) files were left in several folders.
- `opening_eu.bnr` and `*_eu.lsb` scripts show this build already accounted
  for the European release.

### Event scripts (`.lsb`)

Bytecode for an in-house interpreter implemented in `lsrun.cpp` /
`lsrun_bltin.cpp` (built-ins). Header seen:

```
ff ff ff ff  00 00 0c 9f  ff ff ff ff  00 00 1c 8b  00 00 00 00  01 ...
```

The format is documented in [`SCRIPTS.md`](SCRIPTS.md), and
`python3 tools/lsb.py NAME` disassembles any script.

Script families: `Eve_A##` (note events), `Eve_love##`, `Eve_good##`,
`Eve_marry##`, `Eve_child##`, `Eve_conce##`, `Eve_pm_##`, `eve_pw_##`,
`Eve_EP##` (epilogue), `Eve_rm##`, `Eve_title`, `Eve_Marry`, `talk`, `eat`,
`tone`, `dolphin`.

### The Jamie wedding

| Script | Size | Text (`message_e/event/`) |
|---|---|---|
| `Eve_marry00.lsb` … `Eve_marry21.lsb` | 1–2 KB | `eve_marry00_e.bin` (advice before marrying), `eve_marry02_e.bin` (normal wedding) |
| `Eve_rm00.lsb` | 2,639 B | `eve_rm00_e.bin` |
| `Eve_rm01.lsb` / `_e` / `_eu` | 7,307 B | `eve_rm01_e.bin` — **Jamie wedding** |
| `Eve_rm02.lsb` / `_e` / `_eu` | 7,271 B | (uses the text of `eve_rm01_e.bin`; `fn_80166888` picks it when `save+0x227C != 0`, which looks like the player's gender) |

`Eve_rm01.lsb` (plus `_e` / `_eu` variants, 7,307 bytes each) is the Jamie
wedding. Its text file `eve_rm01_e.bin` contains the ceremony, the Goddess's
farewell and the "Several Seasons Later..." card that closes the ceremony. It
is 4–7× larger than a normal wedding script because of that ceremony, not
because an ending is baked in.

Settled: the end of the game is not in the scripts. Native code decides it:
with flag 230 set, `fn_80166888` chains `Eve_rm00`, `Eve_rm01`/`Eve_rm02` and
`Eve_EP00`-`Eve_EP08`, and when `Eve_EP08` finishes the game returns to the
title (state 2). Evidence, addresses and the mod design are in
[`SCRIPTS.md`](SCRIPTS.md).
