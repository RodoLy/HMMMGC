# Native runtime / Runtime nativo

[Español](#español) · [English](#english)

---

## Español

El runtime que corre el juego sin Dolphin. El traductor de
[WiiCompiled](https://github.com/patchzyy/wiicompiled) convierte el código de
`main.dol` en C++; este runtime le da lo que el código traducido espera de la
consola: la memoria, los registros especiales del CPU, el hardware y las partes
del sistema operativo que no pueden ser código traducido. El estado y los
resultados están en [docs/NATIVE_RUNTIME.md](../docs/NATIVE_RUNTIME.md).

### Compilar

```sh
scripts/build_native_runtime.sh
build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

Hace falta Linux con git, el SDK de .NET 8, clang, cmake, ninja y el disco
extraído en `orig/G4AEE9` (`tools/extract_disc.py`). El script baja WiiCompiled
en la versión que usa el runtime, traduce el juego en `build/native/translation`
y compila. El C++ traducido sale de tu disco: no se sube nunca.

La imagen sale de [aurora](https://github.com/encounter/aurora) (MIT), que
viene en la copia de WiiCompiled. Al configurar el runtime, aurora baja Dawn ya
compilado, SDL3 y unas bibliotecas pequeñas; para la ventana, SDL necesita los
paquetes de desarrollo de X11 (o Wayland) del sistema, y para dibujar hace
falta un controlador de Vulkan. Donde no se puede bajar nada, los argumentos
que se le pasen al script van a cmake y pueden apuntar a copias locales:
`-DDAWN_SOURCE_DIR=… -DDAWN_BUILD_DIR=…` (un Dawn compilado desde su código) y
`-DFETCHCONTENT_SOURCE_DIR_<NOMBRE>=…` para las demás (SDL, FMT, XXHASH,
IMGUI, TRACY, ABSEIL-CPP).

Opciones de `hmmm-native`: `--watchdog <segundos>` imprime cada tanto dónde
está el juego (0 lo apaga), y `--user-folder <carpeta>` cambia la carpeta del
jugador (ver abajo). Con `HMMM_NATIVE_READABLE_REGISTERS=1` corre bajo un
depurador como gdb (ver `fault_handler.h`).

### Partidas guardadas

El runtime guarda lo del jugador en su carpeta: `HarvestMoonMM\Native` dentro
de `%LOCALAPPDATA%` en Windows, y `~/.local/share/HarvestMoonMM/Native` en
Linux. La memory card del slot A es la carpeta `USA/Card A`, con un archivo
`.gci` por cada archivo de la tarjeta, como en las carpetas GCI de Dolphin.

Mientras el runtime nativo esté en desarrollo usa su propia tarjeta, aparte de
la de la versión sobre ModernGekko (`HarvestMoonMM\GC\USA\Card A`), para no
dañar tus partidas. Para probar una, copia `E9-G4AE-HARVESTMOON.gci` de esa
carpeta a la del runtime nativo; la original no se toca.

### Sonido

El sonido sale por el dispositivo de reproducción por defecto, por medio de
SDL. Si no hay ninguno, el juego sigue sin sonido. En una máquina sin tarjeta
de sonido, `SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=<archivo>` lo
escribe a un archivo: estéreo de 16 bits sin encabezado, a la frecuencia del
dispositivo que dice el registro (44.1 kHz si nada dice otra cosa).

### Controles

Un gamepad conectado a la PC es el control 1. Si no hay ninguno, el teclado
juega como control 1, con las mismas teclas que Dolphin trae de fábrica:

| Control de GameCube | Tecla |
|---|---|
| Stick de control | Flechas |
| Stick C | I, J, K, L |
| Cruceta | T, F, G, H |
| A, B, X, Y | X, Z, C, S |
| Z | D |
| L, R | Q, W |
| START | Enter |

### Qué hay en `src/`

| Archivo | Qué hace |
|---|---|
| `main.cpp` | Arranca todo y salta al punto de entrada del juego. |
| `console_boot.*` | Lo que dejan el ROM de arranque y el apploader: los datos de la parte baja de la memoria, la tabla de archivos (FST) y bi2. |
| `extracted_disc.*` | Lee el disco extraído por desplazamiento, como la unidad. |
| `guest_memory.*` | El espacio de direcciones del juego (4 GiB planos) y los accesos lentos. |
| `cpu_registers.*` | Registros especiales, base de tiempo, decrementador, DMA de la caché bloqueada. |
| `function_registry.cpp` | Qué función del host corre en cada dirección del juego. |
| `native_functions.h` | Cómo se declara un reemplazo nativo de una función del juego. |
| `abi_hooks.cpp` | El "gather pipe" de gráficos y unos ganchos de los encabezados de WiiCompiled. |
| `os/guest_os.*` | Interrupciones y el planificador del OS, en nativo. |
| `hardware/` | Un archivo por dispositivo: PI, VI, DSP (con el DMA de audio), DVD, SI, EXI, AI, la memoria de audio (ARAM) y el estado del chip gráfico. |
| `audio/dsp_programs.*` | Lo que corre en el DSP: su ROM de arranque, y el programa que carga, reconocido por un hash de su código. |
| `audio/ax_microcode.*`, `audio/ax_voice.*` | El programa de AX del DSP en C++: las listas de comandos y las voces (decodificar, convertir a 32 kHz, mezclar). |
| `audio/audio_output.*` | La salida de sonido por SDL, con un margen y un control de velocidad. |
| `graphics/aurora_host.*` | La ventana y el dispositivo gráfico, por medio de aurora; un cuadro termina en cada copia a la pantalla. |
| `graphics/gx_draw.*`, `graphics/vertex_layout.*` | Las primitivas y las listas de visualización del juego, entregadas a aurora. |
| `graphics/gx_natives_*.cpp` | Las funciones de GX del juego reemplazadas por las de aurora, por tema (geometría, transformaciones, luces, texturas, TEV, píxeles, copias). |
| `graphics/guest_values.h` | Lee los valores que el juego pasa por dirección (colores, matrices). |
| `input/pad_natives.cpp` | Las funciones de control del juego (PAD) reemplazadas por las de aurora: gamepads por SDL y el teclado. |
| `storage/card_natives.cpp` | Las funciones de memory card del juego (CARD) reemplazadas por las de aurora: una carpeta de archivos `.gci`. |
| `player_folder.*` | La carpeta del jugador: la memory card y los ajustes y cachés de aurora. |
| `mods/native_mods.*` | Los mods de `mods/`, compilados dentro: lee sus descriptores y corre sus *patch* y *hook* donde `tools/insert_mod_hooks.py` marcó el código traducido. `../mod_api/` es el header que usan en lugar del de ModernGekko ([mods/README.md](../mods/README.md)). |
| `fault_handler.*`, `guest_symbols.*`, `runtime_log.h` | Diagnóstico: dónde está el juego cuando algo falla. |

Los encabezados de la ABI que comparten el código traducido y el runtime
(`guest_abi/`) y dos archivos de ayuda (`fpu_helpers.cpp`, `ppc_quantized.cpp`)
vienen de WiiCompiled, que es GPL-3.0 como este proyecto, y aurora viene en su
copia; se compilan desde ahí, no se copian aquí. `audio/ax_microcode.*` y
`audio/ax_voice.*` están portados de la emulación de alto nivel de AX de
Dolphin (GPL-2.0-or-later), con la misma aritmética.

El estilo del código (formato con `clang-format`, nombres revisados con
`clang-tidy`) está en [CONTRIBUTING.md](../CONTRIBUTING.md).

---

## English

The runtime that runs the game without Dolphin.
[WiiCompiled](https://github.com/patchzyy/wiicompiled)'s translator turns the
code of `main.dol` into C++; this runtime gives it what the translated code
expects from the console: memory, the CPU's special registers, the hardware and
the parts of the operating system that cannot be translated code. The status
and the results are in [docs/NATIVE_RUNTIME.md](../docs/NATIVE_RUNTIME.md).

### Building

```sh
scripts/build_native_runtime.sh
build/native/runtime/hmmm-native --disc orig/G4AEE9 --symbols config/G4AEE9/symbols.txt
```

It needs Linux with git, the .NET 8 SDK, clang, cmake, ninja and the disc
extracted in `orig/G4AEE9` (`tools/extract_disc.py`). The script downloads
WiiCompiled at the version the runtime uses, translates the game into
`build/native/translation` and compiles. The translated C++ comes from your
disc: it is never committed.

The picture comes from [aurora](https://github.com/encounter/aurora) (MIT),
which comes in WiiCompiled's checkout. When the runtime is configured, aurora
downloads Dawn already built, SDL3 and a few small libraries; for the window,
SDL needs the system's X11 (or Wayland) development packages, and drawing
needs a Vulkan driver. Where nothing can be downloaded, the arguments given to
the script go to cmake and can point at local copies:
`-DDAWN_SOURCE_DIR=… -DDAWN_BUILD_DIR=…` (a Dawn built from source) and
`-DFETCHCONTENT_SOURCE_DIR_<NAME>=…` for the others (SDL, FMT, XXHASH, IMGUI,
TRACY, ABSEIL-CPP).

Options of `hmmm-native`: `--watchdog <seconds>` prints where the game is every
so often (0 turns it off), and `--user-folder <folder>` changes the player
folder (see below). With `HMMM_NATIVE_READABLE_REGISTERS=1` it runs under a
debugger such as gdb (see `fault_handler.h`).

### Saved games

The runtime keeps the player's things in its folder: `HarvestMoonMM\Native`
in `%LOCALAPPDATA%` on Windows, and `~/.local/share/HarvestMoonMM/Native` on
Linux. The memory card in slot A is the folder `USA/Card A`, with one `.gci`
file for each file on the card, as in Dolphin's GCI folders.

While the native runtime is in progress it uses a card of its own, apart from
the one of the version on ModernGekko (`HarvestMoonMM\GC\USA\Card A`), so
that it cannot damage your saves. To try one, copy `E9-G4AE-HARVESTMOON.gci`
from that folder to the native runtime's; the original is not touched.

### Sound

The sound comes out of the default playback device, through SDL. If there is
none, the game goes on without sound. On a machine without a sound card,
`SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=<file>` writes it to a file:
16-bit stereo with no header, at the device's rate that the log gives
(44.1 kHz unless something says otherwise).

### Controls

A gamepad connected to the PC is controller 1. If there is none, the keyboard
plays as controller 1, with the same keys that Dolphin has by default:

| GameCube controller | Key |
|---|---|
| Control Stick | Arrow keys |
| C Stick | I, J, K, L |
| D-Pad | T, F, G, H |
| A, B, X, Y | X, Z, C, S |
| Z | D |
| L, R | Q, W |
| START | Enter |

### What is in `src/`

| File | What it does |
|---|---|
| `main.cpp` | Starts everything and jumps to the game's entry point. |
| `console_boot.*` | What the boot ROM and the apploader leave: the low-memory globals, the file system table (FST) and bi2. |
| `extracted_disc.*` | Reads the extracted disc by offset, as the drive does. |
| `guest_memory.*` | The game's address space (a flat 4 GiB) and the slow accesses. |
| `cpu_registers.*` | Special registers, time base, decrementer, the locked cache's DMA. |
| `function_registry.cpp` | Which host function runs at each address of the game. |
| `native_functions.h` | How a native replacement of a game function is declared. |
| `abi_hooks.cpp` | The graphics gather pipe and a few hooks of WiiCompiled's headers. |
| `os/guest_os.*` | Interrupts and the OS scheduler, natively. |
| `hardware/` | One file per device: PI, VI, DSP (with the audio DMA), DVD, SI, EXI, AI, the audio memory (ARAM) and the graphics chip's status. |
| `audio/dsp_programs.*` | What runs on the DSP: its boot ROM, and the program it loads, recognized by a hash of its code. |
| `audio/ax_microcode.*`, `audio/ax_voice.*` | The DSP's AX program in C++: the command lists and the voices (decode, convert to 32 kHz, mix). |
| `audio/audio_output.*` | The sound output through SDL, with a margin and a speed control. |
| `graphics/aurora_host.*` | The window and the graphics device, through aurora; a frame ends at each copy to the screen. |
| `graphics/gx_draw.*`, `graphics/vertex_layout.*` | The game's primitives and display lists, handed to aurora. |
| `graphics/gx_natives_*.cpp` | The game's GX functions replaced by aurora's, by topic (geometry, transforms, lights, textures, TEV, pixels, copies). |
| `graphics/guest_values.h` | Reads the values the game passes by address (colors, matrices). |
| `input/pad_natives.cpp` | The game's controller functions (PAD) replaced by aurora's: gamepads through SDL, and the keyboard. |
| `storage/card_natives.cpp` | The game's memory card functions (CARD) replaced by aurora's: a folder of `.gci` files. |
| `player_folder.*` | The player folder: the memory card, and aurora's settings and caches. |
| `mods/native_mods.*` | The mods of `mods/`, compiled in: reads their descriptors and runs their patches and hooks where `tools/insert_mod_hooks.py` marked the translated code. `../mod_api/` is the header they use instead of ModernGekko's ([mods/README.md](../mods/README.md)). |
| `fault_handler.*`, `guest_symbols.*`, `runtime_log.h` | Diagnostics: where the game is when something fails. |

The ABI headers that the translated code and the runtime share (`guest_abi/`)
and two helper files (`fpu_helpers.cpp`, `ppc_quantized.cpp`) come from
WiiCompiled, which is GPL-3.0 like this project, and aurora comes in its
checkout; they are compiled from there, not copied here. `audio/ax_microcode.*`
and `audio/ax_voice.*` are ported from Dolphin's high-level emulation of AX
(GPL-2.0-or-later), with the same arithmetic.

The code style (formatting with `clang-format`, names checked with
`clang-tidy`) is in [CONTRIBUTING.md](../CONTRIBUTING.md).
