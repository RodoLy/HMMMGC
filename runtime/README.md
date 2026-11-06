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

Opciones de `hmmm-native`: `--watchdog <segundos>` imprime cada tanto dónde
está el juego (0 lo apaga). Con `HMMM_NATIVE_READABLE_REGISTERS=1` corre bajo un
depurador como gdb (ver `fault_handler.h`).

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
| `hardware/` | Un archivo por dispositivo: PI, VI, DSP, DVD, SI, EXI, AI y el estado del chip gráfico. |
| `fault_handler.*`, `guest_symbols.*`, `runtime_log.h` | Diagnóstico: dónde está el juego cuando algo falla. |

Los encabezados de la ABI que comparten el código traducido y el runtime
(`guest_abi/`) y dos archivos de ayuda (`fpu_helpers.cpp`, `ppc_quantized.cpp`)
vienen de WiiCompiled, que es GPL-3.0 como este proyecto; se compilan desde su
copia, no se copian aquí.

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

Options of `hmmm-native`: `--watchdog <seconds>` prints where the game is every
so often (0 turns it off). With `HMMM_NATIVE_READABLE_REGISTERS=1` it runs under
a debugger such as gdb (see `fault_handler.h`).

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
| `hardware/` | One file per device: PI, VI, DSP, DVD, SI, EXI, AI and the graphics chip's status. |
| `fault_handler.*`, `guest_symbols.*`, `runtime_log.h` | Diagnostics: where the game is when something fails. |

The ABI headers that the translated code and the runtime share (`guest_abi/`)
and two helper files (`fpu_helpers.cpp`, `ppc_quantized.cpp`) come from
WiiCompiled, which is GPL-3.0 like this project; they are compiled from its
checkout, not copied here.

The code style (formatting with `clang-format`, names checked with
`clang-tidy`) is in [CONTRIBUTING.md](../CONTRIBUTING.md).
