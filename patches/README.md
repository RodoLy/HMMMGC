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

### `moderngekko/windows-graphics-backends.patch`

El launcher y el runner de ModernGekko solo ofrecen Vulkan y OpenGL como API
gráfica, y usan Vulkan por omisión. Dolphin en Windows también trae DirectX 11
(`D3D`) y DirectX 12 (`D3D12`), y en Windows ARM64 no compila su backend de
OpenGL. En la laptop Snapdragon donde se probó el paquete, con Vulkan la ventana
del juego se queda en negro (Dolphin escribe `Failed to submit command buffer:
VK_ERROR_INITIALIZATION_FAILED`) y con DirectX 12 el juego corre; sin este
parche no había cómo elegirlo desde el launcher ni desde `config.ini`. El parche:

- agrega a `tools/frontend_config.cpp` la función `GraphicsBackendsFor`, que dice
  qué APIs gráficas ofrece cada plataforma y cuál va primero, que es la que se
  usa por omisión: en Windows ARM64, DirectX 12, DirectX 11 y Vulkan; en Windows
  x64, Vulkan, DirectX 11, DirectX 12 y OpenGL (Vulkan sigue por omisión, como
  antes, porque no hay evidencia de que falle ahí); en lo demás (Linux), Vulkan
  y OpenGL, como siempre;
- hace que el menú del launcher y `config.ini` ofrezcan y acepten esas APIs, por
  su nombre en Dolphin (`D3D12`, `D3D`, `Vulkan`, `OGL`), por el texto del menú
  o por un alias común (`dx12`, `dx11`, `d3d11`), sin importar mayúsculas,
  espacios, guiones ni guiones bajos. Una API que la plataforma no tiene (por
  ejemplo `OpenGL` en Windows ARM64) se rechaza con un mensaje que lista las que
  sí tiene, en vez de pedirle a Dolphin un backend que no existe, caso en el que
  Dolphin se queda con el suyo sin avisar;
- hace que el runner use `DefaultGraphicsBackend()` cuando nadie le dice qué API
  usar. `--graphics` sigue pasando el nombre tal cual a Dolphin.

En Linux no cambia nada: Vulkan sigue por omisión y un `config.ini` con una API
desconocida sigue dando `graphics backend must be Vulkan or OpenGL`. Un
`config.ini` que ya existe conserva su `backend=` (por ejemplo `Vulkan`): hay que
elegir otra API en el launcher o editar el archivo.

**Versiones a las que se aplica:** ModernGekko `edd460a`, la que fija
`scripts/build_and_run_linux.sh` con ModernGekko-Template `c2553a9`. Se aplica
a `lib/ModernGekko`, no a `vendor/dolphin`. Va después de
`skip-assets-hash.patch`, porque los dos cambian `tools/moderngekko_run.cpp`.

**Licencia:** GPL-3.0-or-later, como ModernGekko.

**Aplicar** (hay que recompilar el runner y el launcher):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/windows-graphics-backends.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run moderngekko-launcher
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

### `moderngekko/launcher-keyboard-play.patch`

El launcher no deja pulsar **Play** si no hay un control SDL conectado
(«Connect an SDL-compatible controller before playing»), aunque el juego corre
sin control con las teclas que Dolphin asigna por omisión. En una computadora
sin control había que saltarse el launcher y abrir el juego con
`Play-Keyboard.bat`. El parche:

- hace que **Play** y **Extract and Play** abran la partida en solitario cuando
  no hay control conectado ni perfil de control guardado: el juego usa el
  teclado y el launcher lo dice en su línea de estado («No SDL gamepad detected:
  Play uses the keyboard (netplay needs a gamepad)»);
- deja igual la partida en red: **Host Netplay** y **Join Netplay** siguen
  pidiendo un control, con el mismo mensaje de antes, porque el perfil de
  control nombra los controles de los jugadores;
- no cambia nada cuando hay un control conectado o un perfil guardado.

**Versiones a las que se aplica:** ModernGekko `edd460a`, la que fija
`scripts/build_and_run_linux.sh` con ModernGekko-Template `c2553a9`. Se aplica
a `lib/ModernGekko`, no a `vendor/dolphin`. Va después de
`skip-assets-hash.patch`, porque los dos cambian `tools/moderngekko_launcher.cpp`.

**Licencia:** GPL-3.0-or-later, como ModernGekko.

**Aplicar** (hay que recompilar el launcher):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/launcher-keyboard-play.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-launcher
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

### `moderngekko/close-window-before-config-shutdown.patch`

En Windows, el ejecutable del juego (`moderngekko-run.exe`) termina con el
código de salida `0xC000041D` («excepción fatal en una devolución de llamada
del usuario») cuando se cierra el juego, aunque el juego ya se había apagado
bien: `[staticrecomp] shutdown:` sale en su consola antes del error. En una
laptop Snapdragon ocurrió en 6 de las 7 mediciones registradas y en la
ejecución con `Play.bat` que quedó en el registro. Es un fallo del programa y
no de la laptop. Qué pasa:

- `Runtime::~Runtime()` apaga la configuración de Dolphin
  (`UICommon::Shutdown()`) y solo después, cuando se destruyen los miembros del
  objeto, se destruye la ventana del juego;
- al destruir una ventana que tiene el foco del teclado, Windows le manda
  `WM_KILLFOCUS`, y el procedimiento de la ventana
  (`PlatformWin32::WndProc`) responde poniendo la velocidad de emulación en 1.0
  con `Config::SetCurrent`;
- sin configuración no queda ninguna capa donde escribir: `Config::GetLayer`
  devuelve un puntero nulo y `Set` lo usa sin revisarlo. Como el error ocurre
  dentro de un procedimiento de ventana que Windows llamó desde su núcleo, el
  código de salida es 0xC000041D y no el de una violación de acceso.

Cómo se encontró: `Measure-Speed.bat` anota lo que Windows registra del fallo.
Dio el desplazamiento `0x420A8` dentro de `moderngekko-run.exe`. Leído como
datos, sin correrlo, ese ejecutable tiene ahí la primera instrucción de una
búsqueda en un `std::map` hecha con un `this` nulo (`Config::Layer::Set`); esa
función la llama `WndProc` en su caso de mensaje 8 (`WM_KILLFOCUS`), justo
después de pedir la capa `CurrentRun` a `Config::GetLayer`.

El parche destruye la ventana antes de apagar la configuración, que es el
orden que ya usa el `main` de `DolphinNoGUI` que trae ModernGekko (destruye la
ventana justo después de `Core::Shutdown`, con la configuración viva). También
la destruye antes de apagar la configuración cuando `Init()` de la plataforma
falla. No cambia nada más del cierre.

**Sin probar con el parche puesto:** hace falta compilar el paquete en GitHub y
cerrar el juego en la laptop. El criterio es que `Measure-Speed.bat` termine
con `the game closed with code 0`.

**Versiones a las que se aplica:** ModernGekko `edd460a`, la que fija
`scripts/build_and_run_linux.sh` con ModernGekko-Template `c2553a9`. Se aplica
a `lib/ModernGekko`, no a `vendor/dolphin`. Se aplica antes o después de
`skip-assets-hash.patch`, que también cambia `src/runtime/dolphin_runtime.cpp`,
en otro lugar.

**Licencia:** GPL-3.0-or-later, como ModernGekko.

**Aplicar** (hay que recompilar el runner):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/close-window-before-config-shutdown.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

### `wiicompiled/skip-calls-below-main-memory.patch`

Para el runtime nativo, no para ModernGekko: `scripts/build_native_runtime.sh`
lo aplica solo al traductor de WiiCompiled antes de compilarlo (si ya está
aplicado, lo deja así) y vuelve a compilar el traductor cuando lo acaba de
aplicar.

El traductor sigue las llamadas del juego para encontrar las funciones que
tiene que traducir. El sistema operativo de la GameCube salta al depurador con
una llamada a la dirección `0x60` (`bla 0x60`, en `fn_8016F6B0`), que está por
debajo de la memoria principal (`0x80000000`): ahí no hay código del juego.
Sin el parche, el traductor la toma como una función más y la traducción se
detiene con `Address 0x00000060 is outside the loaded RAM image`. El parche no
sigue las llamadas a direcciones por debajo de `0x80000000`; la llamada misma
se traduce igual, y el juego nunca la hace (solo con un depurador conectado).

**Versiones a las que se aplica:** WiiCompiled `0d5c402`, la que fija
`scripts/build_native_runtime.sh`.

**Licencia:** GPL-3.0, como WiiCompiled.

**Aplicar a mano:**

```bash
git -C "$WIICOMPILED_DIR" apply "$PWD/patches/wiicompiled/skip-calls-below-main-memory.patch"
```

**Quitar:** el mismo `git apply` con `-R`.

### `aurora/reject-far-interpolation-pairs.patch`

Para el runtime nativo, no para ModernGekko, y este sí se aplica solo:
`scripts/build_native_runtime.sh` lo aplica a la copia de aurora de WiiCompiled
antes de compilar (si ya está aplicado, lo deja así).

La interpolación de cuadros de aurora (60 y 120 fps,
`runtime/src/graphics/frame_interpolation.h`) empareja cada dibujo de un cuadro
con el del cuadro anterior que dibuja lo mismo, y en los cuadros intermedios lo
pone a medio camino. Cuando hay varias copias de una malla (los postes de una
cerca, las plantas), las empareja por cercanía, y acepta cualquier par que se
mueva menos de 1500 unidades del juego por cuadro. Para este juego eso es mucho:
la cámara de la granja está a unas 20 unidades del personaje, y el juego corre a 30 cuadros,
así que cada cuadro abarca el doble de movimiento que en un juego a 60. Al
caminar, un poste que sale de la vista por un borde y otro que entra por el otro
quedan sin pareja y aurora puede juntarlos; entonces el cuadro intermedio dibuja
un poste a la mitad del camino entre los dos, por un instante. Es la explicación
más probable de lo que se vio en la laptop solo con 60 fps: una cerca que
aparece y desaparece al dar un paso, y plantas fantasma semitransparentes. Aquí
no se ha podido reproducir todavía.

El parche agrega a los dos lugares donde aurora prepara un par (el dibujo
normal y el de matrices indexadas) un límite relativo: un dibujo puede moverse
en un cuadro como mucho la cuarta parte de su distancia a la cámara (o 1 unidad,
para lo que está pegado a la cámara, como un cielo). El movimiento normal, el de
la cámara incluido, queda muy por debajo: en una caminata por la granja, menos
de 1 de cada 1000 pares se movía más del 15 % de su distancia. Un par
que pasa el límite no se interpola: el dibujo aparece donde está en el cuadro
del juego.

**Probado** en la laptop con 60 fps: la cerca que aparecía y desaparecía y las
plantas fantasma ya no salen. En Linux compila y el juego corre igual.

**Versiones a las que se aplica:** aurora de WiiCompiled `0d5c402` (el
submódulo `aurora-main`, `33067e3`), la que fija
`scripts/build_native_runtime.sh`.

**Licencia:** MIT, como aurora.

**Aplicar a mano:**

```bash
git -C "$WIICOMPILED_DIR/aurora-main" apply "$PWD/patches/aurora/reject-far-interpolation-pairs.patch"
```

**Quitar:** el mismo `git apply` con `-R`.

### `aurora/clear-whole-picture-with-render-pass.patch`

Para el runtime nativo, como el anterior: `scripts/build_native_runtime.sh` lo
aplica solo a la copia de aurora de WiiCompiled.

Cuando el juego copia la imagen y la borra (`GXCopyTex` con borrado), aurora
borra con su propio borrado de la GPU solo si el rectángulo es todo el EFB; si
es una parte, dibuja un triángulo que cubre esa parte. El juego borra cada
cuadro sus 640x448, y el EFB de aurora mide 640x528: las filas de abajo las
guarda como espacio de trabajo para efectos de otros juegos. Así que ese
borrado siempre era un dibujo. En la laptop (Adreno X1-45, Direct3D 12), en
pantalla ancha a 4x, eso dejaba bloquecitos donde lo que el juego dibujaba
después no se veía y salía el color de borrado: puntos negros en lugares fijos
de la pantalla, a 30 y a 60 fps. En 4:3, a otras resoluciones y en la nube
(Vulkan por software) no pasaba.

El parche borra con el borrado propio de la GPU cuando el rectángulo tiene
toda la imagen que el juego muestra: todo el EFB menos esas filas de abajo,
que se borran también. Magical Melody nunca dibuja en ellas (sus viewports son
de 640x448 y de 256x256 arriba a la izquierda, para las sombras). Un borrado
más chico (el del mapa de sombras) sigue siendo un dibujo.

**Probado** en la laptop, en pantalla ancha a 4x: con versiones de prueba,
los puntos salían del color de borrado (rojo, al cambiarlo), no se iban con
un ancho redondeado a 64 ni a 320 píxeles, y se fueron al borrar toda la
imagen así, y también al escribir la profundidad de ese dibujo desde el
shader. Se eligió el primero: el borrado propio de la GPU es además el más
barato en una GPU por bloques. En Linux el juego se ve igual.

**Versiones a las que se aplica:** las mismas que el parche anterior.

**Licencia:** MIT, como aurora.

**Aplicar a mano:**

```bash
git -C "$WIICOMPILED_DIR/aurora-main" apply "$PWD/patches/aurora/clear-whole-picture-with-render-pass.patch"
```

**Quitar:** el mismo `git apply` con `-R`.

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

### `moderngekko/windows-graphics-backends.patch`

ModernGekko's launcher and runner only offer Vulkan and OpenGL as the graphics
API, and use Vulkan by default. Dolphin on Windows also has DirectX 11 (`D3D`)
and DirectX 12 (`D3D12`), and on Windows ARM64 it does not build its OpenGL
backend. On the Snapdragon laptop where the package was tried, with Vulkan the
game window stays black (Dolphin writes `Failed to submit command buffer:
VK_ERROR_INITIALIZATION_FAILED`) and with DirectX 12 the game runs; without this
patch there was no way to choose it from the launcher or from `config.ini`. The
patch:

- adds the `GraphicsBackendsFor` function to `tools/frontend_config.cpp`, which
  says which graphics APIs each platform offers and which one goes first, the one
  used by default: on Windows ARM64, DirectX 12, DirectX 11 and Vulkan; on
  Windows x64, Vulkan, DirectX 11, DirectX 12 and OpenGL (Vulkan stays the
  default, as it was, because there is no evidence that it fails there); on
  everything else (Linux), Vulkan and OpenGL, as always;
- makes the launcher menu and `config.ini` offer and accept those APIs, by their
  name in Dolphin (`D3D12`, `D3D`, `Vulkan`, `OGL`), by the text of the menu or
  by a common alias (`dx12`, `dx11`, `d3d11`), whatever the case, spaces, hyphens
  or underscores. An API that the platform does not have (for example `OpenGL` on
  Windows ARM64) is rejected with a message that lists the ones it does have,
  instead of asking Dolphin for a backend that does not exist, in which case
  Dolphin keeps its own without saying so;
- makes the runner use `DefaultGraphicsBackend()` when nobody says which API to
  use. `--graphics` still passes the name as it is to Dolphin.

Nothing changes on Linux: Vulkan stays the default and a `config.ini` with an
unknown API still gives `graphics backend must be Vulkan or OpenGL`. A
`config.ini` that already exists keeps its `backend=` (for example `Vulkan`):
choose another API in the launcher or edit the file.

**Applies to:** ModernGekko `edd460a`, the version `scripts/build_and_run_linux.sh`
pins through ModernGekko-Template `c2553a9`. It applies to `lib/ModernGekko`,
not to `vendor/dolphin`. It goes after `skip-assets-hash.patch`, because both
change `tools/moderngekko_run.cpp`.

**License:** GPL-3.0-or-later, like ModernGekko.

**Apply** (the runner and the launcher have to be rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/windows-graphics-backends.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run moderngekko-launcher
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.

### `moderngekko/launcher-keyboard-play.patch`

The launcher does not let you press **Play** if no SDL controller is connected
("Connect an SDL-compatible controller before playing"), even though the game
runs without a controller, with the keys that Dolphin maps by default. On a
computer with no controller you had to skip the launcher and open the game with
`Play-Keyboard.bat`. The patch:

- makes **Play** and **Extract and Play** open the solo game when no controller
  is connected and no controller profile is saved: the game uses the keyboard
  and the launcher says so in its status line ("No SDL gamepad detected: Play
  uses the keyboard (netplay needs a gamepad)");
- leaves netplay as it was: **Host Netplay** and **Join Netplay** still ask for
  a controller, with the same message as before, because the controller profile
  names the controllers of the players;
- changes nothing when a controller is connected or a profile is saved.

**Applies to:** ModernGekko `edd460a`, the version `scripts/build_and_run_linux.sh`
pins through ModernGekko-Template `c2553a9`. It applies to `lib/ModernGekko`,
not to `vendor/dolphin`. It goes after `skip-assets-hash.patch`, because both
change `tools/moderngekko_launcher.cpp`.

**License:** GPL-3.0-or-later, like ModernGekko.

**Apply** (the launcher has to be rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/launcher-keyboard-play.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-launcher
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.

### `moderngekko/close-window-before-config-shutdown.patch`

On Windows, the game's executable (`moderngekko-run.exe`) ends with exit code
`0xC000041D` ("fatal exception in a user callback") when the game is closed,
even though the game had already shut down cleanly: `[staticrecomp] shutdown:`
appears on its console before the error. On a Snapdragon laptop it happened in
6 of the 7 recorded measurements and in the `Play.bat` run that was left in the
log. It is a fault of the program, not of the laptop. What happens:

- `Runtime::~Runtime()` shuts Dolphin's configuration down
  (`UICommon::Shutdown()`) and only afterwards, when the members of the object
  are destroyed, is the game window destroyed;
- when a window that has the keyboard focus is destroyed, Windows sends it
  `WM_KILLFOCUS`, and the window procedure (`PlatformWin32::WndProc`) answers
  by setting the emulation speed to 1.0 with `Config::SetCurrent`;
- with no configuration there is no layer left to write to:
  `Config::GetLayer` returns a null pointer and `Set` uses it without checking.
  Because the error happens inside a window procedure that Windows called from
  its kernel, the exit code is 0xC000041D and not that of an access violation.

How it was found: `Measure-Speed.bat` writes down what Windows records about
the crash. It gave offset `0x420A8` inside `moderngekko-run.exe`. Read as data,
without running it, that executable has there the first instruction of a
`std::map` lookup made with a null `this` (`Config::Layer::Set`); that function
is called by `WndProc` in its case for message 8 (`WM_KILLFOCUS`), right after
asking `Config::GetLayer` for the `CurrentRun` layer.

The patch destroys the window before it shuts the configuration down, which is
the order the `DolphinNoGUI` `main` that ships with ModernGekko already uses (it
destroys the window right after `Core::Shutdown`, with the configuration still
alive). It also destroys the window before shutting the configuration down when
the platform's `Init()` fails. Nothing else about the shutdown changes.

**Not tried with the patch in place:** the package has to be built on GitHub
and the game closed on the laptop. The criterion is that `Measure-Speed.bat`
ends with `the game closed with code 0`.

**Applies to:** ModernGekko `edd460a`, the version `scripts/build_and_run_linux.sh`
pins through ModernGekko-Template `c2553a9`. It applies to `lib/ModernGekko`,
not to `vendor/dolphin`. It applies before or after `skip-assets-hash.patch`,
which also changes `src/runtime/dolphin_runtime.cpp`, in another place.

**License:** GPL-3.0-or-later, like ModernGekko.

**Apply** (the runner has to be rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko" apply "$PWD/patches/moderngekko/close-window-before-config-shutdown.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.


### `wiicompiled/skip-calls-below-main-memory.patch`

For the native runtime, not for ModernGekko: `scripts/build_native_runtime.sh`
applies it by itself to WiiCompiled's translator before building it (if it is
already applied, it is left as it is) and builds the translator again when it
has just applied it.

The translator follows the game's calls to find the functions it has to
translate. The GameCube's operating system jumps to the debugger with a call to
address `0x60` (`bla 0x60`, in `fn_8016F6B0`), which is below main memory
(`0x80000000`): there is no game code there. Without the patch, the translator
takes it as one more function and the translation stops with
`Address 0x00000060 is outside the loaded RAM image`. The patch does not follow
calls to addresses below `0x80000000`; the call itself is translated as before,
and the game never makes it (only with a debugger attached).

**Applies to:** WiiCompiled `0d5c402`, the one `scripts/build_native_runtime.sh`
pins.

**License:** GPL-3.0, like WiiCompiled.

**Apply by hand:**

```bash
git -C "$WIICOMPILED_DIR" apply "$PWD/patches/wiicompiled/skip-calls-below-main-memory.patch"
```

**Remove:** the same `git apply` with `-R`.

### `aurora/reject-far-interpolation-pairs.patch`

For the native runtime, not for ModernGekko, and this one does apply itself:
`scripts/build_native_runtime.sh` applies it to WiiCompiled's copy of aurora
before building (if it is already applied, it is left as it is).

aurora's frame interpolation (60 and 120 fps,
`runtime/src/graphics/frame_interpolation.h`) pairs each draw of a frame with
the draw of the frame before that draws the same thing, and puts it halfway in
the frames in between. When there are several copies of one mesh (the posts of
a fence, the plants), it pairs them by nearness, and accepts any pair that
moves less than 1500 game units per frame. For this game that is a lot: the
farm's camera is about 20 units from the character, and the game runs at 30 frames,
so each frame covers twice the movement of a game at 60. While walking, a post
that leaves the view at one edge and another that enters at the other edge are
left without partners and aurora can pair them; the frame in between then draws
a post halfway between the two, for an instant. It is the likeliest
explanation of what was seen on the laptop only at 60 fps: a fence that
appears and disappears when taking a step, and see-through ghost plants. It
has not been reproduced here yet.

The patch adds to the two places where aurora prepares a pair (the ordinary
draw and the one with indexed matrices) a relative limit: a draw may move in
one frame at most a quarter of its distance from the camera (or 1 unit, for
what sits at the camera, like a sky). Ordinary movement, the camera's
included, stays far below: on a walk across the farm, fewer than 1 in 1000
pairs moved more than 15% of their distance. A pair past the limit is not
interpolated: the draw shows where it is in the game's frame.

**Tried** on the laptop at 60 fps: the fence that appeared and disappeared and
the ghost plants no longer show. On Linux it builds and the game runs as before.

**Applies to:** WiiCompiled's aurora `0d5c402` (the `aurora-main` submodule,
`33067e3`), the one `scripts/build_native_runtime.sh` pins.

**License:** MIT, like aurora.

**Apply by hand:**

```bash
git -C "$WIICOMPILED_DIR/aurora-main" apply "$PWD/patches/aurora/reject-far-interpolation-pairs.patch"
```

**Remove:** the same `git apply` with `-R`.

### `aurora/clear-whole-picture-with-render-pass.patch`

For the native runtime, like the one before: `scripts/build_native_runtime.sh`
applies it by itself to WiiCompiled's copy of aurora.

When the game copies the picture and clears it (`GXCopyTex` with a clear),
aurora clears with the GPU's own clear only when the rectangle is the whole
EFB; for a part of it, it draws a triangle over that part. The game clears its
640x448 every frame, and aurora's EFB is 640x528: it keeps the rows below as
scratch for other games' effects. So that clear was always a draw. On the
laptop (Adreno X1-45, Direct3D 12), in widescreen at 4x, it left small blocks
where what the game drew afterwards did not show and the clear color did:
black dots at fixed places of the screen, at 30 and at 60 fps. It did not
happen in 4:3, at other resolutions, or in the cloud (software Vulkan).

The patch clears with the GPU's own clear when the rectangle holds all of the
picture the game shows: all of the EFB but those rows below, which are
cleared too. Magical Melody never draws to them (its viewports are 640x448,
and 256x256 at the top left for the shadows). A smaller clear (the shadow
map's) is still a draw.

**Tried** on the laptop, in widescreen at 4x: with test builds, the dots came
out in the clear color (red, when it was changed), did not go away with the
width rounded to 64 or to 320 pixels, and went away when the whole picture was
cleared this way, and also when that draw wrote its depth from the shader. The
first was chosen: the GPU's own clear is also the cheapest on a tiled GPU. On
Linux the game looks the same.

**Applies to:** the same versions as the patch before.

**License:** MIT, like aurora.

**Apply by hand:**

```bash
git -C "$WIICOMPILED_DIR/aurora-main" apply "$PWD/patches/aurora/clear-whole-picture-with-render-pass.patch"
```

**Remove:** the same `git apply` with `-R`.
