# Patches / Parches

[Español](#español) · [English](#english)

---

## Español

Parches opcionales para componentes de terceros que usa este proyecto.
`scripts/build_and_run_linux.sh` usa ModernGekko tal cual; nada de aquí se
aplica solo. El workflow que arma el paquete de Windows
(`.github/workflows/package-windows-arm64.yml`) sí aplica todos, en el orden en
que aparecen aquí.

### `moderngekko/lockstep-verifier.patch`

Mejora el verificador lockstep que ya trae ModernGekko: mientras se juega,
compara cada bloque de código recompilado con el intérprete de CPU de Dolphin
(cómo funciona y qué resultados da: [docs/VALIDATION.md](../docs/VALIDATION.md)).
Agrega estas variables de entorno, que se usan junto a `STATICRECOMP_LOCKSTEP=1`:

| Variable | Qué hace |
|---|---|
| `STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` | El intérprete sigue más allá de la primera vez que llega a la dirección donde terminó el bloque recompilado, y el bloque pasa si el estado recompilado es igual al del intérprete en *cualquier* llegada. Así desaparecen las diferencias que solo vienen de detenerse en iteraciones distintas de un mismo bucle. Cada bloque que pasó en una llegada posterior sale como `[lockstep] LATE-MATCH` y se cuenta en `late_matches`. |
| `STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1` | No compara los bits FPRF, FR y FI del FPSCR (máscara `0x0007F000`), que cada instrucción de punto flotante reescribe y el juego no lee. |
| `STATICRECOMP_LOCKSTEP_COVERAGE_FILE=ruta` | Al cerrar el juego escribe la dirección de cada instrucción que se comparó y salió igual; `tools/lockstep_coverage.py` lo convierte en porcentajes. |
| `STATICRECOMP_LOCKSTEP_INJECT_FAULT=0xDIRECCIÓN` | Control negativo del propio verificador: cambia un bit de `r3` en el estado final recompilado del bloque que empieza en esa dirección. Si el verificador no lo reporta, no sirve. |

**Versiones a las que se aplica:** ModernGekko `edd460a`, `vendor/dolphin`
(RecompCore) `10f7ac4`, las que fija `scripts/build_and_run_linux.sh` con
ModernGekko-Template `c2553a9`.

**Licencia:** el parche modifica archivos de Dolphin, que son
GPL-2.0-or-later, y se distribuye con esa misma licencia.

**Aplicar** (un minuto de compilación; el resto de ModernGekko no se vuelve a
compilar):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/lockstep-verifier.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

### `moderngekko/export-lockstep-hook-on-mingw.patch`

Arregla que el verificador lockstep se apague solo cuando el módulo del juego
se compila en Windows con un compilador MinGW (por ejemplo llvm-mingw, el
compilador portátil pensado para el paquete de Windows). El verificador busca
por nombre la función `ppc_set_mem_write_journal` del módulo, y
`GXRuntime/src/core/cpu.c` la marcaba `dllexport` solo para MSVC. Con MinGW, en
cuanto el enlazador ve un `dllexport` (el de `staticrecomp_get_module`) deja de
exportar todo lo demás por su cuenta, así que la función quedaba fuera y el
verificador avisaba `module lacks ppc_set_mem_write_journal export` y se
apagaba. El parche usa `_WIN32`, como ya hace `module_export.c`. En MSVC, Linux
y macOS no cambia nada.

**Versiones a las que se aplica:** las mismas que el parche anterior.

**Licencia:** modifica un archivo de GXRuntime, que es GPL-3.0-or-later, y se
distribuye con esa misma licencia.

**Aplicar:** `cpu.c` se compila dentro del módulo de cada juego, así que basta
con aplicarlo antes de compilar el módulo; las herramientas no se recompilan.

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/export-lockstep-hook-on-mingw.patch"
```

**Quitar:** el mismo `git apply` con `-R`.

### `moderngekko/relocatable-port.patch`

`moderngekko-port`, la herramienta que compila el módulo de un juego, lee la
plantilla del módulo, el código de GXRuntime y los ajustes del juego desde la
carpeta de fuentes donde se compiló, y esa ruta queda escrita dentro del
ejecutable. Si el ejecutable se copia a otro equipo, por ejemplo dentro de un
paquete, esas carpetas ya no existen y no puede compilar nada. El parche hace
que las busque, en este orden: en la carpeta de la variable de entorno
`MODERNGEKKO_SOURCE_DIR`; en una carpeta `source/` junto al ejecutable (si
contiene `vendor/dolphin/module-template`); y, si no, en la ruta con la que se
compiló, como hasta ahora. Quien compila ModernGekko en su máquina no nota
ningún cambio.

`source/` tiene la misma distribución que el código fuente. Lo que usa la
herramienta es `vendor/dolphin/module-template`, `vendor/dolphin/GXRuntime`,
`vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp` y, para los ajustes del
juego, `vendor/dolphin/Data/Sys/GameSettings`.

**Versiones a las que se aplica:** ModernGekko `edd460a`, la que fija
`scripts/build_and_run_linux.sh` con ModernGekko-Template `c2553a9`. Se aplica
a `lib/ModernGekko`, no a `vendor/dolphin`.

**Licencia:** GPL-3.0-or-later, como ModernGekko.

**Aplicar** (hay que recompilar `moderngekko-port`):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/relocatable-port.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-port
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

### `moderngekko/skip-assets-hash.patch`

Cada vez que ModernGekko inspecciona un juego (`InspectGame`) calcula un SHA-256
de todos los archivos de su carpeta `files/`, que en este juego son unos 1.4 GB.
Lo hace en cada arranque: el launcher varias veces, el runner dos y
`moderngekko-port` en cada orden. En un contenedor de la nube una sola
inspección tardó 13 s; con el parche, 0.02 s. En la laptop Snapdragon donde se
probó el paquete, el juego tardó unos 70 s en empezar a correr, y parte de ese
tiempo es esta lectura (no se midió cuánta).

Ese hash solo se usa en dos casos: cuando una compilación fija el juego exacto
con `MODERNGEKKO_REQUIRED_ASSETS_SHA256`, y cuando se comparan los jugadores de
una partida en red. El parche:

- agrega a `InspectGame` la opción `InspectOptions::hash_assets` y a
  `RuntimeConfig` el campo `hash_game_assets`; el valor por omisión es el de
  siempre (calcular el hash), así que quien use la biblioteca no nota nada;
- hace que los programas (`tools/game_inspection.hpp`) solo calculen el hash si
  la compilación define `MODERNGEKKO_REQUIRED_ASSETS_SHA256`, que es la única
  que lo compara; `moderngekko-port` nunca lo necesita;
- no cambia la partida en red: la sesión sigue calculándolo al empezar, porque
  los jugadores comparan ese hash.

**Versiones a las que se aplica:** ModernGekko `edd460a`, la que fija
`scripts/build_and_run_linux.sh` con ModernGekko-Template `c2553a9`. Se aplica
a `lib/ModernGekko`, no a `vendor/dolphin`. Va después de
`relocatable-port.patch`, porque los dos cambian `tools/moderngekko_port.cpp`.

**Licencia:** GPL-3.0-or-later, como ModernGekko.

**Aplicar** (hay que recompilar el runner, `moderngekko-port` y el launcher):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/skip-assets-hash.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run moderngekko-port moderngekko-launcher
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

---

## English

Optional patches for third-party components this project uses.
`scripts/build_and_run_linux.sh` uses ModernGekko as it is; nothing here
applies itself. The workflow that builds the Windows package
(`.github/workflows/package-windows-arm64.yml`) does apply all of them, in the
order they appear here.

### `moderngekko/lockstep-verifier.patch`

Improves the lockstep verifier that ModernGekko already ships: while the game
plays, it compares every block of recompiled code with Dolphin's CPU
interpreter (how it works and what it found:
[docs/VALIDATION.md](../docs/VALIDATION.md)). It adds these environment
variables, used together with `STATICRECOMP_LOCKSTEP=1`:

| Variable | What it does |
|---|---|
| `STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` | The interpreter runs on past the first time it reaches the address where the recompiled block ended, and the block passes if the recompiled state equals the interpreter's state at *any* arrival. That removes the differences that only come from stopping on different iterations of the same loop. Each block that passed at a later arrival is printed as `[lockstep] LATE-MATCH` and counted in `late_matches`. |
| `STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1` | Does not compare the FPSCR bits FPRF, FR and FI (mask `0x0007F000`), which every floating-point instruction rewrites and the game does not read. |
| `STATICRECOMP_LOCKSTEP_COVERAGE_FILE=path` | When the game closes, writes the address of every instruction that was compared and came out equal; `tools/lockstep_coverage.py` turns it into percentages. |
| `STATICRECOMP_LOCKSTEP_INJECT_FAULT=0xADDRESS` | A negative control for the verifier itself: flips a bit of `r3` in the recompiled end state of the block that starts at that address. If the verifier does not report it, it is worthless. |

**Applies to:** ModernGekko `edd460a`, `vendor/dolphin` (RecompCore)
`10f7ac4`, the versions `scripts/build_and_run_linux.sh` pins through
ModernGekko-Template `c2553a9`.

**License:** the patch modifies Dolphin files, which are GPL-2.0-or-later,
and is distributed under the same license.

**Apply** (a minute of compiling; the rest of ModernGekko is not rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/lockstep-verifier.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.

### `moderngekko/export-lockstep-hook-on-mingw.patch`

Fixes the lockstep verifier switching itself off when the game's module is
built on Windows with a MinGW compiler (llvm-mingw, for example, the portable
compiler meant for the Windows package). The verifier looks up the module's
`ppc_set_mem_write_journal` function by name, and
`GXRuntime/src/core/cpu.c` marked it `dllexport` for MSVC only. With MinGW, as
soon as the linker sees one `dllexport` (the one on `staticrecomp_get_module`)
it stops exporting everything else on its own, so the function was left out and
the verifier printed `module lacks ppc_set_mem_write_journal export` and
disabled itself. The patch tests `_WIN32`, as `module_export.c` already does.
Nothing changes on MSVC, Linux or macOS.

**Applies to:** the same versions as the previous patch.

**License:** it modifies a GXRuntime file, which is GPL-3.0-or-later, and is
distributed under the same license.

**Apply:** `cpu.c` is compiled into each game's module, so applying it before
the module is built is enough; the tools are not rebuilt.

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/export-lockstep-hook-on-mingw.patch"
```

**Remove:** the same `git apply` with `-R`.

### `moderngekko/relocatable-port.patch`

`moderngekko-port`, the tool that builds a game's module, reads the module
template, the GXRuntime code and the game settings from the source folder it
was built in, and that path is written inside the executable. If the
executable is copied to another machine, in a package for example, those
folders no longer exist and it cannot build anything. The patch makes it look
for them in this order: in the folder named by the `MODERNGEKKO_SOURCE_DIR`
environment variable; in a `source/` folder beside the executable (if it holds
`vendor/dolphin/module-template`); and otherwise in the path it was built
with, as before. Whoever builds ModernGekko on their own machine sees no
change.

`source/` is laid out like the source tree. What the tool uses is
`vendor/dolphin/module-template`, `vendor/dolphin/GXRuntime`,
`vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp` and, for the game
settings, `vendor/dolphin/Data/Sys/GameSettings`.

**Applies to:** ModernGekko `edd460a`, the version `scripts/build_and_run_linux.sh`
pins through ModernGekko-Template `c2553a9`. It applies to `lib/ModernGekko`,
not to `vendor/dolphin`.

**License:** GPL-3.0-or-later, like ModernGekko.

**Apply** (`moderngekko-port` has to be rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/relocatable-port.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-port
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.

### `moderngekko/skip-assets-hash.patch`

Every time ModernGekko inspects a game (`InspectGame`) it computes a SHA-256 of
all the files in its `files/` folder, which for this game are about 1.4 GB. It
does it at every start: the launcher several times, the runner twice and
`moderngekko-port` on every command. In a cloud container one inspection took
13 s; with the patch, 0.02 s. On the Snapdragon laptop where the package was
tried, the game took about 70 s to start running, and part of that time is this
reading (how much was not measured).

That hash is only used in two cases: when a build pins the exact game with
`MODERNGEKKO_REQUIRED_ASSETS_SHA256`, and when the players of a netplay session
are compared. The patch:

- adds the `InspectOptions::hash_assets` option to `InspectGame` and the
  `hash_game_assets` field to `RuntimeConfig`; the default is what it always
  was (compute the hash), so whoever uses the library sees no change;
- makes the programs (`tools/game_inspection.hpp`) compute the hash only if the
  build defines `MODERNGEKKO_REQUIRED_ASSETS_SHA256`, the only one that
  compares it; `moderngekko-port` never needs it;
- leaves netplay as it was: the session still computes it when it starts,
  because the players compare that hash.

**Applies to:** ModernGekko `edd460a`, the version `scripts/build_and_run_linux.sh`
pins through ModernGekko-Template `c2553a9`. It applies to `lib/ModernGekko`,
not to `vendor/dolphin`. It goes after `relocatable-port.patch`, because both
change `tools/moderngekko_port.cpp`.

**License:** GPL-3.0-or-later, like ModernGekko.

**Apply** (the runner, `moderngekko-port` and the launcher have to be rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/skip-assets-hash.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run moderngekko-port moderngekko-launcher
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.
