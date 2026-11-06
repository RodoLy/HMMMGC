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
   macros `PPC_NATIVE_OVERRIDE` y `PPC_NATIVE_OVERRIDE_VOID` (559, sobre 554
   direcciones distintas), y su dirección se tradujo a
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
| VI (video) | 11 | 10 | 0 | Retrazo vertical y búfer de pantalla. |
| PAD (controles) | 8 | 5 | 0 | |
| AI / DSP (hardware de audio) | 12 | 7 | 0 | Aquí reemplaza WiiCompiled el audio: el DMA de AI, los buzones del DSP y el microcódigo de AX, en nativo. |
| AX / AXFX / MIX (biblioteca de audio) | 26 | 0 | 0 | Código que corre en el CPU: se traduce tal cual (ver abajo). |
| AR / ARQ (ARAM) | 8 | 0 | 0 | Memoria de audio de GameCube; Wii no la tiene. Copias de memoria. |
| MTX (matrices) | 4 | 0 | 0 | Código normal: se puede traducir tal cual. |
| **Total** | **276** | **107** | **0** | |

Todas tienen nombre, cada una con su prueba contra `main.dol` (ver «Otros
nombres del SDK por lectura del código» en [FUNCTION_MAP.md](FUNCTION_MAP.md)).
El 2026-11-05 faltaban 39, y la zona del audio contaba de menos: solo dos de
sus funciones tenían nombre, así que la herramienta no veía la biblioteca
entera. Con sus nombres, el audio suma 38 puntos de entrada en vez de 12, y VI
pasa de 6 a 10 en la columna de WiiCompiled, porque sus cuatro nombres nuevos
son de funciones que WiiCompiled ya reemplaza.

**El audio.** WiiCompiled no reemplaza AX: deja que su código corra traducido y
reemplaza lo que está debajo, el DMA de AI y el DSP, con un microcódigo de AX
propio que lee los bloques de parámetros de cada voz. Ese es el trabajo para
GameCube: el bloque de Wii no tiene la misma forma (su `mixerCtrl` es de 32
bits, así que todo lo que sigue se corre, y tiene mezclas para el control
remoto), y el de GameCube es el que describe «La zona del audio» en
[FUNCTION_MAP.md](FUNCTION_MAP.md).

### Qué sigue

1. Una capa de GameCube propia, en este repositorio: las piezas de
   WiiCompiled que sirven (GX, PAD, VI, AX, parte de OS) con las direcciones de
   este juego, y lo que falta (CARD, ARAM, el resto de OS y DVD). WiiCompiled es
   GPL-3.0 como este proyecto; lo que se tome de él se marca con su origen.
2. Compilar aurora con Dawn. No se puede desde el contenedor de esta sesión
   (las dependencias de Dawn se bajan de servidores de Google que no responden
   aquí): GitHub Actions o la computadora del jugador.
3. Llegar a la pantalla de título, y comparar cuadro por cuadro con la versión
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
   `PPC_NATIVE_OVERRIDE` and `PPC_NATIVE_OVERRIDE_VOID` macros (559, on 554
   different addresses), and their address was turned into
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
| VI (video) | 11 | 10 | 0 | Vertical retrace and frame buffer. |
| PAD (controllers) | 8 | 5 | 0 | |
| AI / DSP (audio hardware) | 12 | 7 | 0 | This is where WiiCompiled replaces the audio: the AI DMA, the DSP mailboxes and the AX microcode, natively. |
| AX / AXFX / MIX (audio library) | 26 | 0 | 0 | Code that runs on the CPU: it is translated as it is (see below). |
| AR / ARQ (ARAM) | 8 | 0 | 0 | GameCube audio memory; the Wii has none. Memory copies. |
| MTX (matrices) | 4 | 0 | 0 | Ordinary code: it can be translated as it is. |
| **Total** | **276** | **107** | **0** | |

All of them have a name, each with its test against `main.dol` (see "Other SDK
names by reading the code" in [FUNCTION_MAP.md](FUNCTION_MAP.md)). On
2026-11-05, 39 were missing, and the audio zone was undercounted: only two of
its functions had a name, so the tool did not see the whole library. With its
names, audio adds up to 38 entry points instead of 12, and VI goes from 6 to 10
in the WiiCompiled column, because its four new names are of functions that
WiiCompiled already replaces.

**Audio.** WiiCompiled does not replace AX: it lets its code run translated and
replaces what lies below it, the AI DMA and the DSP, with an AX microcode of
its own that reads the parameter block of each voice. That is the GameCube
work: the Wii block does not have the same shape (its `mixerCtrl` is 32 bits
wide, so everything after it moves, and it has mixes for the Wii Remote), and
the GameCube one is the one that "The audio zone" in
[FUNCTION_MAP.md](FUNCTION_MAP.md) describes.

### What comes next

1. A GameCube layer of our own, in this repository: the pieces of WiiCompiled
   that serve (GX, PAD, VI, AX, part of OS) at this game's addresses, and what
   is missing (CARD, ARAM, the rest of OS and DVD). WiiCompiled is GPL-3.0 like
   this project; whatever is taken from it is marked with where it came from.
2. Build aurora with Dawn. It cannot be done from this session's container
   (Dawn's dependencies are downloaded from Google servers that do not answer
   here): GitHub Actions or the player's computer.
3. Reach the title screen, and compare it frame by frame with the version on
   ModernGekko, which is still the one that is played in the meantime.
