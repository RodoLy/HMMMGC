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

### Primer arranque sin Dolphin (2026-11-06)

[`runtime/`](../runtime) es el runtime nativo: el código del juego, traducido a
C++ por el traductor de WiiCompiled (en la versión que fija
`scripts/build_native_runtime.sh`), enlazado con código propio en lugar del
núcleo de Dolphin. El script hace todo a partir del disco extraído; el C++ del
juego se escribe en `build/native` y nunca se sube.

**Resultado.** El juego corre nativo desde su primera instrucción (`__start`) y
hace toda su inicialización: OSInit (interrupciones, EXI, SRAM, controles), el
arranque del DSP de audio, ARAM, DVDInit, VIInit y GXInit, y entra en su bucle
de cuadros a 59.94 campos por segundo. Lee del disco más de 500 archivos
(logos, menús, la pantalla de título) y, tras un rato sin que nadie toque
nada, carga una escena: un guion de evento, música y el modelo del niño en un
campo de primavera, que parece la demostración de la pantalla de título.
Todavía no hay imagen, sonido ni control: los comandos de gráficos se guardan
en el FIFO, pero nadie los dibuja.

**Cómo.**

- Memoria: el traductor convierte cada acceso del juego en un acceso a una
  reserva plana de 4 GiB del host. El runtime pone ahí la memoria principal (con
  sus tres vistas: con caché, sin caché y física), la caché bloqueada y la
  ventana de los registros de hardware (`guest_memory.cpp`).
- Hardware (`hardware/`): un modelo pequeño de cada dispositivo que usa el
  arranque: el controlador de interrupciones (PI); el video (VI), con sus
  interrupciones de retrazo a 59.94 Hz; el DSP (buzones, DMA de ARAM y los
  saludos del ROM del DSP que espera OSInit); el DVD, que lee el disco
  extraído; el EXI (reloj y SRAM, sin memory cards); el SI (sin controles); el
  contador de muestras del audio, y el estado del chip gráfico (dibujo
  terminado y tokens). El primer acceso a cada registro queda en el registro
  con la función del juego que lo hizo, y así cada corrida dice qué hardware
  falta.
- Sistema operativo (`os/guest_os.cpp`): cinco funciones del OS son nativas:
  OSDisableInterrupts, OSEnableInterrupts, OSRestoreInterrupts, SelectThread y
  OSLoadContext. El código traducido no se puede detener en cualquier
  instrucción, así que las interrupciones se atienden donde el juego acepta ser
  interrumpido: cuando las vuelve a activar y mientras el planificador espera.
  Los manejadores del juego corren traducidos, tal cual. Este juego no crea
  hilos (nunca llama a OSCreateThread), así que no hace falta otra pila.
- El traductor lee los reemplazos nativos de `runtime/src` (los marcadores
  `PPC_NATIVE_OVERRIDE_VOID`), los deja fuera de la traducción y les manda las
  llamadas con los registros que leen. Para saber cuáles lee busca
  `<parámetro>->gpr[N]` con el nombre de parámetro del marcador; si el
  marcador nombra otro, no pasa los argumentos.
  `tests/test_native_runtime_sources.py` lo comprueba.

**Probarlo** (Linux con clang, cmake, ninja y el SDK de .NET 8):

```sh
scripts/build_native_runtime.sh
build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

### Qué sigue

1. Imagen: GX con aurora. Hay que compilar aurora con Dawn, y no se puede
   desde el contenedor de esta sesión (las dependencias de Dawn se bajan de
   servidores de Google que no responden aquí): GitHub Actions o la
   computadora del jugador. WiiCompiled reemplaza las funciones de GX por
   llamadas a aurora; lo que se tome de él (GPL-3.0, como este proyecto) se
   marca con su origen.
2. Controles: que el SI conteste como un control de GameCube, con el teclado o
   un gamepad.
3. Memory card: que el EXI tenga una tarjeta, guardada en un archivo `.gci`.
4. Sonido: el DSP con el microcódigo de AX hecho en nativo, como WiiCompiled.
5. Comparar cuadro por cuadro con la versión sobre ModernGekko, que sigue
   siendo la que se juega mientras tanto.

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

### First boot without Dolphin (2026-11-06)

[`runtime/`](../runtime) is the native runtime: the game's code, translated to
C++ by WiiCompiled's translator (at the version that
`scripts/build_native_runtime.sh` pins), linked with code of our own instead of
the Dolphin core. The script does everything from the extracted disc; the
game's C++ is written to `build/native` and is never committed.

**Result.** The game runs natively from its first instruction (`__start`) and
does all of its start-up: OSInit (interrupts, EXI, SRAM, controllers), the
audio DSP's start, ARAM, DVDInit, VIInit and GXInit, and it enters its frame
loop at 59.94 fields per second. It reads more than 500 files from the disc
(logos, menus, the title screen) and, after a while with nobody touching
anything, it loads a scene: an event script, music and the boy's model in a
spring field, which looks like the title screen's demo. There is no picture,
sound or controller yet: the graphics commands are stored in the FIFO, but
nothing draws them.

**How.**

- Memory: the translator turns every access of the game into an access to a
  flat 4 GiB host reservation. The runtime maps there the main memory (with its
  three views: cached, uncached and physical), the locked cache and the window
  of the hardware registers (`guest_memory.cpp`).
- Hardware (`hardware/`): a small model of each device the start-up uses: the
  interrupt controller (PI); the video (VI), with its retrace interrupts at
  59.94 Hz; the DSP (mailboxes, ARAM DMA and the greetings of the DSP's ROM
  that OSInit waits for); the DVD, which reads the extracted disc; the EXI
  (clock and SRAM, no memory cards); the SI (no controllers); the audio sample
  counter, and the graphics chip's status (draw done and tokens). The first
  access to each register is logged with the game function that made it, so
  every run tells which hardware is missing.
- Operating system (`os/guest_os.cpp`): five functions of the OS are native:
  OSDisableInterrupts, OSEnableInterrupts, OSRestoreInterrupts, SelectThread
  and OSLoadContext. Translated code cannot be stopped at any instruction, so
  interrupts are taken where the game accepts being interrupted: when it turns
  them back on, and while the scheduler waits. The game's handlers run
  translated, as they are. This game creates no threads (it never calls
  OSCreateThread), so no second stack is needed.
- The translator reads the native replacements from `runtime/src` (the
  `PPC_NATIVE_OVERRIDE_VOID` markers), leaves them out of the translation and
  sends the calls to them with the registers they read. To know which ones it
  looks for `<parameter>->gpr[N]` with the marker's parameter name; if the
  marker names another one, it passes no arguments.
  `tests/test_native_runtime_sources.py` checks it.

**Trying it** (Linux with clang, cmake, ninja and the .NET 8 SDK):

```sh
scripts/build_native_runtime.sh
build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

### What comes next

1. Picture: GX with aurora. It needs aurora built with Dawn, which cannot be
   done from this session's container (Dawn's dependencies are downloaded from
   Google servers that do not answer here): GitHub Actions or the player's
   computer. WiiCompiled replaces the GX functions with calls to aurora;
   whatever is taken from it (GPL-3.0, like this project) is marked with where
   it came from.
2. Controllers: make the SI answer like a GameCube controller, from the
   keyboard or a gamepad.
3. Memory card: give the EXI a card, kept in a `.gci` file.
4. Sound: the DSP with the AX microcode done natively, as WiiCompiled does.
5. Compare frame by frame with the version on ModernGekko, which is still the
   one that is played in the meantime.
