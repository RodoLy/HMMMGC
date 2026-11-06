# Native runtime / Runtime nativo

[Español](#español) · [English](#english)

---

## Español

Este documento es la lista de trabajo para que el juego corra **sin el núcleo
de Dolphin** (pista A de [PLAN.md](PLAN.md)), con
[WiiCompiled](https://github.com/patchzyy/wiicompiled) como base: su traductor
convierte el código del juego en C++ y su runtime reemplaza las funciones del
SDK de Nintendo (gráficos, sistema, disco, controles, audio) por código nativo,
con [aurora](https://github.com/encounter/aurora) para los gráficos.

### Cómo se midió

1. `tools/find_library_entry_points.py <biblioteca>` lista, para cada
   biblioteca del SDK, las funciones que el resto del juego llama: son las que
   un runtime nativo tiene que dar. Las demás son internas de la biblioteca.
2. Las funciones que el runtime de WiiCompiled reemplaza se leyeron de sus
   macros `PPC_NATIVE_OVERRIDE` (552 en total), y su dirección se tradujo a
   nombre con el mapa de Mario Kart Wii (`projects/mkwii/MAP.txt`, donde se
   escriben `GX::SetViewport`; aquí se escriben `GXSetViewport`).
3. Se cruzaron las dos listas por nombre.

### Resultado (2026-11-05)

| Biblioteca | Lo que el juego llama | Ya lo da WiiCompiled | Sin nombre todavía | Nota |
|---|---:|---:|---:|---|
| GX (gráficos) | 75 | 62 | 0 | La parte más difícil ya está casi toda. |
| OS (sistema) | 65 | 19 | 0 | Hilos, alarmas, montón, arena y reinicio. |
| DVD (disco) | 19 | 3 | 0 | Lectura de archivos del disco extraído. |
| CARD (memory card) | 17 | 0 | 0 | Wii no la usa: es nueva. Guardar en archivos `.gci`. |
| EXI (bus de la memory card) | 16 | 1 | 0 | Desaparece si CARD es nativa. |
| SI (bus de los controles) | 15 | 0 | 0 | Desaparece si PAD es nativa. |
| VI (video) | 11 | 6 | 0 | Retrazo vertical y búfer de pantalla. |
| PAD (controles) | 8 | 5 | 0 | |
| AI / AX / DSP (audio) | 12 | 5 | 0 | El juego usa MusyX sobre AX; WiiCompiled trae un AX nativo. AX cuenta de menos (ver abajo). |
| AR / ARQ (ARAM) | 8 | 0 | 0 | Memoria de audio de GameCube; Wii no la tiene. Copias de memoria. |
| MTX (matrices) | 4 | 0 | 0 | Código normal: se puede traducir tal cual. |
| **Total** | **250** | **101** | **0** | |

Las 39 que no tenían nombre el 2026-11-05 ya lo tienen, cada una con su prueba
contra `main.dol` (ver «Otros nombres del SDK por lectura del código» en
[FUNCTION_MAP.md](FUNCTION_MAP.md)).

**AX cuenta de menos.** La herramienta saca el tramo de cada biblioteca de los
nombres, y el de AX lo marcan solo dos funciones, así que la fila de audio
cuenta una sola función de AX. Medida a mano
(`tools/find_library_entry_points.py AX --range 0x8017FEDC-0x801877FC`), la
zona de AX y AXFX tiene 28 puntos de entrada, y 27 siguen sin nombre.

### Qué sigue

1. Nombrar los 27 puntos de entrada sin nombre de la zona de AX y AXFX.
2. Una capa de GameCube propia, en este repositorio: las piezas de
   WiiCompiled que sirven (GX, PAD, VI, AX, parte de OS) con las direcciones de
   este juego, y lo que falta (CARD, ARAM, el resto de OS y DVD). WiiCompiled es
   GPL-3.0 como este proyecto; lo que se tome de él se marca con su origen.
3. Compilar aurora con Dawn. No se puede desde el contenedor de esta sesión
   (las dependencias de Dawn se bajan de servidores de Google que no responden
   aquí): GitHub Actions o la computadora del jugador.
4. Llegar a la pantalla de título, y comparar cuadro por cuadro con la versión
   sobre ModernGekko, que sigue siendo la que se juega mientras tanto.

---

## English

This document is the work list for running the game **without the Dolphin
core** (track A of [PLAN.md](PLAN.md)), with
[WiiCompiled](https://github.com/patchzyy/wiicompiled) as the base: its
translator turns the game's code into C++ and its runtime replaces the
functions of Nintendo's SDK (graphics, system, disc, input, audio) with native
code, with [aurora](https://github.com/encounter/aurora) for the graphics.

### How it was measured

1. `tools/find_library_entry_points.py <library>` lists, for each SDK library,
   the functions that the rest of the game calls: they are the ones a native
   runtime has to provide. The others are internal to the library.
2. The functions that WiiCompiled's runtime replaces were read from its
   `PPC_NATIVE_OVERRIDE` macros (552 in all), and their address was turned into
   a name with the Mario Kart Wii map (`projects/mkwii/MAP.txt`, which writes
   `GX::SetViewport`; here it is written `GXSetViewport`).
3. The two lists were matched by name.

### Result (2026-11-05)

| Library | What the game calls | Already in WiiCompiled | Not named yet | Note |
|---|---:|---:|---:|---|
| GX (graphics) | 75 | 62 | 0 | The hardest part is nearly all there. |
| OS (system) | 65 | 19 | 0 | Threads, alarms, heap, arena and reset. |
| DVD (disc) | 19 | 3 | 0 | Reading files of the extracted disc. |
| CARD (memory card) | 17 | 0 | 0 | The Wii does not use it: it is new. Save to `.gci` files. |
| EXI (memory card bus) | 16 | 1 | 0 | Goes away if CARD is native. |
| SI (controller bus) | 15 | 0 | 0 | Goes away if PAD is native. |
| VI (video) | 11 | 6 | 0 | Vertical retrace and frame buffer. |
| PAD (controllers) | 8 | 5 | 0 | |
| AI / AX / DSP (audio) | 12 | 5 | 0 | The game uses MusyX on AX; WiiCompiled brings a native AX. AX is undercounted (see below). |
| AR / ARQ (ARAM) | 8 | 0 | 0 | GameCube audio memory; the Wii has none. Memory copies. |
| MTX (matrices) | 4 | 0 | 0 | Ordinary code: it can be translated as it is. |
| **Total** | **250** | **101** | **0** | |

The 39 that had no name on 2026-11-05 have one now, each with its test against
`main.dol` (see "Other SDK names by reading the code" in
[FUNCTION_MAP.md](FUNCTION_MAP.md)).

**AX is undercounted.** The tool takes each library's stretch from the names,
and only two functions mark the one of AX, so the audio row counts a single AX
function. Measured by hand
(`tools/find_library_entry_points.py AX --range 0x8017FEDC-0x801877FC`), the AX
and AXFX zone has 28 entry points, and 27 are still unnamed.

### What comes next

1. Name the 27 unnamed entry points of the AX and AXFX zone.
2. A GameCube layer of our own, in this repository: the pieces of WiiCompiled
   that serve (GX, PAD, VI, AX, part of OS) at this game's addresses, and what
   is missing (CARD, ARAM, the rest of OS and DVD). WiiCompiled is GPL-3.0 like
   this project; whatever is taken from it is marked with where it came from.
3. Build aurora with Dawn. It cannot be done from this session's container
   (Dawn's dependencies are downloaded from Google servers that do not answer
   here): GitHub Actions or the player's computer.
4. Reach the title screen, and compare it frame by frame with the version on
   ModernGekko, which is still the one that is played in the meantime.
