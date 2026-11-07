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

### La imagen, con aurora (2026-11-06)

**Resultado.** El juego dibuja sin Dolphin: los logos del arranque, la intro en
3D (el perro, los animales y el molino) y la pantalla de título con «Push
START». aurora dibuja con WebGPU (Dawn). Se probó en la nube, con Vulkan por
software (lavapipe de Mesa) y un Dawn compilado desde su código, así que la
velocidad de ahí no dice nada de la de una PC.

**Cómo.**

- `graphics/aurora_host`: GXInit abre la ventana y el dispositivo gráfico, y
  GXCopyDisp (la copia de la imagen terminada a la pantalla) cierra el cuadro:
  aurora lo muestra y abre el siguiente.
- `graphics/gx_natives_*.cpp`: 70 funciones de GX del juego son nativas que
  llaman a las de aurora. Reciben los argumentos del juego como parámetros de
  C++, que el runtime saca de los registros según la convención de llamadas de
  PowerPC (`native_functions.h`); lo que el juego pasa por dirección (colores,
  matrices, filtros) se lee de su memoria. Cuatro funciones de GX que solo
  calculan (GXProject, GXGetYScaleFactor, GXAdjustForOverscan y
  GXGetTexBufferSize) siguen traducidas.
- `graphics/gx_draw` y `graphics/vertex_layout`: el juego escribe sus vértices
  valor por valor en el «gather pipe». Se juntan por primitiva y se le dan a
  aurora completa, después de decirle cuánto de cada arreglo de vértices usan
  sus índices: aurora copia eso a la GPU, y el juego nunca lo dice. En una
  lista de visualización los vértices se guardan como bytes, porque la GPU los
  lee con la forma de vértice del momento en que la lista corre, y el juego
  graba las listas de sus modelos al cargarlos, antes de fijar esa forma.
- El juego copia a la pantalla una imagen de 640x448, y el video la centra en
  las 480 líneas con un borde arriba y otro abajo. aurora conserva esas
  proporciones; Dolphin la estira para llenar la ventana.

**El resto de GX.** Además de esas cuatro, el ejecutable trae otras 52
funciones de GX que siguen traducidas, y el código del juego no llama a
ninguna: solo las llaman funciones del SDK que ahora son nativas. La
preparación del estado inicial dentro de GXInit usa casi todas
(GXSetTevSwapMode, GXSetZTexture, las GXPoke*, etc.), y además GXDrawDone y
las listas de visualización llaman a GXFlush, y GXLoadTexObj a
GXLoadTexObjPreLoaded. Se comprobó en el código traducido, contando las
funciones pequeñas que el traductor copia dentro de quien las llama. Si
cambian las nativas, hay que repetir esa revisión.

**Probarlo.** Con el mismo script. aurora baja Dawn ya compilado y sus otras
dependencias cuando se configura el runtime; donde no se puede bajar nada, el
script acepta argumentos extra de cmake para usar copias locales
(`runtime/README.md`). En una máquina sin pantalla se probó así:

```sh
Xvfb :99 -screen 0 1024x768x24 &
DISPLAY=:99 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
    build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

### Los controles, con aurora (2026-11-06)

PADInit, PADRead, PADReset, PADRecalibrate y PADControlMotor son nativas que
llaman a la biblioteca PAD de aurora, que lee los gamepads de la PC por medio
de SDL (`input/pad_natives.cpp`). Mientras no haya un gamepad en el primer
puerto, el teclado juega como control 1 con las teclas que Dolphin trae de
fábrica (la tabla está en `runtime/README.md`). Con eso el juego sale de la
pantalla de título al presionar START y avisa que no encuentra la memory card.

### La memory card, con aurora (2026-11-06)

**Resultado.** Las 15 funciones de CARD a las que llama el juego son nativas
que llaman a la biblioteca CARD de aurora (`storage/card_natives.cpp`), que
guarda la tarjeta del slot A como una carpeta de archivos `.gci`, en el
formato de las carpetas GCI de Dolphin. Con la tarjeta vacía, el juego empieza
una partida nueva: las preguntas del inicio y la escena en la que el
protagonista llega al pueblo y conoce al alcalde. Con una copia de una partida
real, el diario la carga y el juego sigue en la casa del jugador; guardar desde
el diario reescribe el `.gci`, y la partida guardada se vuelve a cargar.

**Dónde queda.** En la carpeta del jugador (`player_folder.h`): por defecto
`HarvestMoonMM/Native` dentro de la carpeta de datos de la plataforma
(`%LOCALAPPDATA%` en Windows), con la tarjeta en `USA/Card A`. Mientras el
runtime nativo esté en desarrollo tiene su propia tarjeta, aparte de las
partidas de la versión sobre ModernGekko, para no dañarlas; un `.gci` se puede
copiar de una a la otra (`runtime/README.md`). aurora guarda ahí también sus
ajustes, y sus cachés en `Cache/`.

**Cómo.** Las estructuras que el juego pasa por dirección (`CARDFileInfo`,
`CARDStat`) se copian campo por campo, y los nombres de los archivos salen del
ID del disco en `0x80000000`, como en el SDK. El juego solo usa las funciones
que esperan su resultado, así que nunca hay que llamar a una función suya
desde el runtime. Una de ellas, CARDSetStatus, todavía no tiene nombre en
`symbols.txt`: es la que llama a CARDSetStatusAsync con `__CARDSyncCallback` y
luego espera en `__CARDSync`.

### El sonido (2026-11-06)

**Resultado.** La música y los efectos suenan: la intro, la pantalla de
título, los menús y la granja, grabados en la máquina de pruebas y escuchados
por el jugador. En 165 s de juego salieron 165 s de sonido, sin huecos.

**Cómo.** El juego usa AX, la biblioteca de sonido de Nintendo, que le manda
al DSP de audio un programa propio y, cada 5 ms, una lista de comandos. El
runtime no ejecuta el código del DSP: como Dolphin, reconoce el programa por un
hash de su código y corre una versión en C++ (`audio/`), portada de la
emulación de alto nivel de AX de Dolphin. Esa versión lee las voces de la
memoria principal y sus muestras de la ARAM (ADPCM o PCM), las convierte a
32 kHz y las mezcla, y deja el cuadro de sonido en la memoria principal. El DMA
de audio lo toca desde ahí a 32 kHz e interrumpe al empezar cada búfer; en esa
interrupción el juego avanza su música (una vez cada 5 ms) y pide el cuadro
siguiente.

**El ritmo.** El juego atiende las interrupciones donde las vuelve a activar,
a veces decenas de milisegundos tarde. Por eso el DMA espera a que el juego
tome la interrupción de cada búfer y luego se pone al día: el sonido sale a
ráfagas, pero nunca se toca un búfer que el juego no ha llenado. La salida
(SDL) guarda un margen de 60 ms y toca hasta medio por ciento más rápido o más
lento para mantenerlo, como el mezclador de Dolphin, porque el reloj de la PC
y el de la tarjeta de sonido nunca van igual.

### Qué sigue

1. Un paquete para Windows (también ARM64) que se pueda probar en la PC del
   jugador, con GPU y tarjeta de sonido de verdad.
2. GX: comparar cuadro por cuadro con la versión sobre ModernGekko (que sigue
   siendo la que se juega mientras tanto).

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

### The picture, with aurora (2026-11-06)

**Result.** The game draws without Dolphin: the boot logos, the 3D intro (the
dog, the animals and the windmill) and the title screen with "Push START".
aurora draws with WebGPU (Dawn). It was tested in the cloud, with software
Vulkan (Mesa's lavapipe) and a Dawn built from source, so the speed there says
nothing about the speed on a PC.

**How.**

- `graphics/aurora_host`: GXInit opens the window and the graphics device, and
  GXCopyDisp (the copy of the finished picture to the screen) closes the
  frame: aurora shows it and opens the next one.
- `graphics/gx_natives_*.cpp`: 70 of the game's GX functions are natives that
  call aurora's. They receive the game's arguments as C++ parameters, which the
  runtime takes from the registers following the PowerPC calling convention
  (`native_functions.h`); what the game passes by address (colors, matrices,
  filters) is read from its memory. Four GX functions that only compute
  (GXProject, GXGetYScaleFactor, GXAdjustForOverscan and GXGetTexBufferSize)
  stay translated.
- `graphics/gx_draw` and `graphics/vertex_layout`: the game writes its
  vertices value by value to the gather pipe. They are collected per
  primitive and handed to aurora whole, after telling it how much of each
  vertex array their indices use: aurora copies that much to the GPU, and the
  game never says it. In a display list the vertices are kept as bytes,
  because the GPU reads them with the vertex layout of the moment the list
  runs, and the game records the lists of its models when it loads them,
  before setting that layout.
- The game copies a 640x448 picture to the screen, and the video interface
  centres it in the 480 lines with a border above and below. aurora keeps
  those proportions; Dolphin stretches it to fill the window.

**The rest of GX.** Besides those four, the executable has 52 more GX
functions that stay translated, and the game's code calls none of them: only
SDK functions that are natives now do. The setup of the initial state inside
GXInit uses nearly all of them (GXSetTevSwapMode, GXSetZTexture, the GXPoke*
functions, etc.), and GXDrawDone and the display list functions call GXFlush,
and GXLoadTexObj calls GXLoadTexObjPreLoaded. This was checked in the
translated code, counting the small functions that the translator copies into
their callers. If the natives change, that check has to be repeated.

**Trying it.** With the same script. aurora downloads Dawn already built and
its other dependencies when the runtime is configured; where nothing can be
downloaded, the script takes extra cmake arguments to use local copies
(`runtime/README.md`). On a machine without a screen it was tested like this:

```sh
Xvfb :99 -screen 0 1024x768x24 &
DISPLAY=:99 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
    build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

### The controllers, with aurora (2026-11-06)

PADInit, PADRead, PADReset, PADRecalibrate and PADControlMotor are natives
that call aurora's PAD library, which reads the PC's gamepads through SDL
(`input/pad_natives.cpp`). While there is no gamepad on the first port, the
keyboard plays as controller 1 with Dolphin's default keys (the table is in
`runtime/README.md`). With that the game leaves the title screen when START is
pressed, and reports that it cannot find the memory card.

### The memory card, with aurora (2026-11-06)

**Result.** The 15 CARD functions that the game calls are natives that call
aurora's CARD library (`storage/card_natives.cpp`), which keeps the card in
slot A as a folder of `.gci` files, in the format of Dolphin's GCI folders.
With the card empty, the game starts a new game: the opening questions and
the scene where the hero arrives in town and meets the mayor. With a copy of
a real save, the diary loads it and the game goes on in the player's house;
saving from the diary rewrites the `.gci`, and the saved game loads again.

**Where it is.** In the player folder (`player_folder.h`): by default
`HarvestMoonMM/Native` in the platform's data folder (`%LOCALAPPDATA%` on
Windows), with the card in `USA/Card A`. While the native runtime is in
progress it has a card of its own, apart from the saves of the version on
ModernGekko, so that it cannot damage them; a `.gci` can be copied from one to
the other (`runtime/README.md`). aurora also keeps its settings there, and its
caches in `Cache/`.

**How.** The structures that the game passes by address (`CARDFileInfo`,
`CARDStat`) are copied field by field, and the file names come from the disc
ID at `0x80000000`, as in the SDK. The game only uses the functions that wait
for their result, so the runtime never has to call a function of the game.
One of them, CARDSetStatus, has no name in `symbols.txt` yet: it is the one
that calls CARDSetStatusAsync with `__CARDSyncCallback` and then waits in
`__CARDSync`.

### The sound (2026-11-06)

**Result.** The music and the sound effects play: the intro, the title
screen, the menus and the farm, recorded on the test machine and listened to
by the player. In 165 s of play, 165 s of sound came out, with no gaps.

**How.** The game uses AX, Nintendo's sound library, which gives the audio
DSP a program of its own and, every 5 ms, a command list. The runtime does not
run the DSP's code: like Dolphin, it recognizes the program by a hash of its
code and runs a version in C++ (`audio/`), ported from Dolphin's high-level
emulation of AX. That version reads the voices from main memory and their
samples from ARAM (ADPCM or PCM), converts them to 32 kHz and mixes them, and
leaves the frame of sound in main memory. The audio DMA plays it from there at
32 kHz and interrupts when each buffer starts; in that interrupt the game
advances its music (once every 5 ms) and asks for the next frame.

**The pace.** The game takes interrupts where it turns them back on,
sometimes tens of milliseconds late. So the audio DMA waits for the game to
take each buffer's interrupt and then catches up: the sound comes out in
bursts, but a buffer the game has not filled is never played. The output
(SDL) keeps a 60 ms margin and plays up to half a percent faster or slower to
hold it, as Dolphin's mixer does, because the PC's clock and the sound card's
never run quite together.

### What comes next

1. A package for Windows (ARM64 too) that can be tried on the player's PC,
   with a real GPU and sound card.
2. GX: compare frame by frame with the version on ModernGekko (which is still
   the one that is played in the meantime).
