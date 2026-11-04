# Building and playing / Compilar y jugar

[Español](#español) · [English](#english)

---

## Español

Hay dos caminos, según tu computadora:

- **Ubuntu o Debian, y Windows dentro de WSL2:** `scripts/build_and_run_linux.sh`
  hace todo el camino: compila ModernGekko, recompila el juego desde tu disco y
  lo abre. Es el camino que está probado con este juego, y el de casi todo este
  documento.
- **Windows en ARM64 (por ejemplo, Snapdragon X), sin WSL:** un paquete zip con
  `Play.bat`, que se arma en GitHub. Es experimental; ver
  [Windows ARM64 nativo](#windows-arm64-nativo-sin-wsl) al final.

### Qué necesitas

- Ubuntu o Debian, o Windows 10/11 con WSL2.
- Tu propio disco de *Harvest Moon: Magical Melody* (USA) en RVZ, ISO o GCM.
- Unos 3 GB libres e internet la primera vez.

### En Windows: instalar WSL2

1. En PowerShell: `wsl --install -d Ubuntu-24.04`. Reinicia si te lo pide.
2. Abre Ubuntu desde el menú Inicio y crea tu usuario y contraseña de Linux.
3. Si ya tenías WSL, corre `wsl --update` para tener ventanas (WSLg) y
   acceso a la GPU.

Dentro de Ubuntu, tus carpetas de Windows están en `/mnt/c`; por ejemplo,
Documentos es `/mnt/c/Users/<tu usuario>/Documents`.

### Compilar y jugar

En Ubuntu, la primera vez, con la ruta de tu disco:

```sh
bash /mnt/c/Users/<tu usuario>/Documents/hm-mm-recomp/scripts/build_and_run_linux.sh \
    "/mnt/c/Users/<tu usuario>/Downloads/Harvest Moon - Magical Melody (USA).rvz"
```

También acepta la ruta como la copia el Explorador (`C:\Users\...`). La
primera vez tarda: descarga unos cientos de MB, compila ModernGekko (15 a 40
minutos) y recompila el juego (unos minutos). Después basta con:

```sh
bash /mnt/c/Users/<tu usuario>/Documents/hm-mm-recomp/scripts/build_and_run_linux.sh
```

### Qué hace el script

| Paso | Qué hace | Se salta cuando |
|---|---|---|
| 1 | Revisa que sea Linux con `apt` y que haya ventanas | nunca |
| 2 | Instala los paquetes que pide ModernGekko-Template | ya están |
| 3 | Busca la GPU: OpenGL por Direct3D 12 en WSL, Vulkan u OpenGL en Linux | nunca |
| 4 | Descarga ModernGekko-Template en `~/ModernGekko-Template`, en la versión probada (`c2553a9`) | ya está |
| 5 | Compila DolRecomp y ModernGekko | ya están compilados |
| 6 | Instala `nodtool` (y Rust si falta) y extrae tu disco con `tools/extract_disc.py` | el disco ya está extraído |
| 7 | Comprueba con `tools/check_disc.py` que sea G4AEE9 revisión 0 | nunca |
| 8 | Recompila el juego si hace falta y lo abre | nunca |

Si se corta a la mitad, vuelve a correrlo: sigue donde se quedó.

### Controles

Sin control configurado, ModernGekko usa el teclado con la asignación por
defecto de Dolphin (función `LoadDefaults` de `GCPadEmu.cpp`). Las flechas y
la tecla X se comprobaron jugando, con pulsaciones reales de teclado; el resto
de la tabla sale de ese código.

| GameCube | Teclado |
|---|---|
| Stick | Flechas (con Shift se queda a media inclinación) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| Cruceta | T (arriba), F (izquierda), G (abajo), H (derecha) |
| C-stick | I (arriba), J (izquierda), K (abajo), L (derecha) |

Los menús del juego se mueven con el stick, no con la cruceta (comprobado en
el menú del título). Un control
USB o Bluetooth no llega a WSL por sí solo (hace falta
[usbipd-win](https://github.com/dorssel/usbipd-win)); en Linux nativo,
ModernGekko genera su configuración cuando lo pones en `config.ini`
(`[Input]`, `controller1=`).

### Dónde queda cada cosa

- `~/ModernGekko-Template`: las herramientas, el disco extraído
  (`extracted/Harvest-Moon-Magical-Melody`) y el módulo recompilado
  (`build/modules`).
- `~/.local/share/moderngekko`: ajustes (`config.ini`), partidas guardadas
  (`GC/USA/Card A`) y mods (`Mods/`).

### Ajustes útiles (`~/.local/share/moderngekko/config.ini`)

- `resolution=3840x2160` para 4K interno (por defecto `1920x1080`; sin GPU, el
  script empieza en `640x528`). Acepta 640x528, 1280x720, 1920x1080,
  2560x1440, 3840x2160, 5120x2880 y 7680x4320.
- `fullscreen=true` para pantalla completa.
- El backend gráfico lo elige el script; para forzarlo:
  `GRAPHICS_BACKEND=Vulkan bash .../build_and_run_linux.sh`.

### Si algo falla

- **`does not contain a CMakeLists.txt file` al compilar:** la descarga se
  cortó a la mitad (Ctrl+C, o la computadora se suspendió). Vuelve a correr
  el script, que termina la descarga, o a mano:
  `git -C ~/ModernGekko-Template submodule update --init --recursive --force`.
  Durante la primera corrida no dejes que la computadora se suspenda.
- **Va muy lento:** el script avisó "no GPU acceleration found", así que el
  juego se dibuja con el procesador. Con una GPU Intel, AMD o NVIDIA,
  actualiza Windows, el driver de la GPU y WSL (`wsl --update`). En laptops
  Windows con procesador ARM (Snapdragon), WSL2 todavía no puede usar la GPU
  ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)) y
  esos pasos no ayudan. Sin GPU, el script escribe `resolution=640x528` en
  `config.ini` la primera vez, para que el procesador tenga menos que
  dibujar. Si ya habías corrido el juego antes, el archivo existe y el script
  no lo toca: cámbialo tú con
  `sed -i 's/^resolution=.*/resolution=640x528/' ~/.local/share/moderngekko/config.ini`.
  La solución de fondo es una versión nativa para Windows ARM64, sin WSL:
  ver [Windows ARM64 nativo](#windows-arm64-nativo-sin-wsl) al final; es
  experimental y todavía no se ha probado con el juego real.
- **"WSL has no window support yet":** corre `wsl --update` y vuelve a abrir
  Ubuntu.
- **`$'\r': command not found`:** el script tiene saltos de línea de
  Windows. El repositorio trae `.gitattributes` para evitarlo; si pasa,
  `sed -i 's/\r$//' scripts/build_and_run_linux.sh` lo arregla.
- **"is not the G4AEE9 revision 0 disc":** borra
  `~/ModernGekko-Template/extracted/Harvest-Moon-Magical-Melody` y vuelve a
  correr el script con la imagen correcta.

### Windows ARM64 nativo (sin WSL)

Para laptops con Windows en ARM (por ejemplo, con Snapdragon X), donde WSL2
todavía no puede usar la GPU, hay un paquete zip que juega de forma nativa: trae
el launcher, el runner, el recompilador (DolRecomp) y un compilador de C
portátil, y no hay que instalar nada. El juego no viene incluido: se recompila
en tu computadora, con tu disco, la primera vez.

- **Para jugar:** sigue [packaging/windows/README.md](../packaging/windows/README.md):
  extraer el zip, doble clic en `Play.bat` y elegir la imagen del disco (o
  `Play-Keyboard.bat` si no tienes control).
- **De dónde sale el zip:** de GitHub Actions. En el repositorio: *Actions* →
  *Windows ARM64 package* → *Run workflow*. Al terminar, el zip está en
  *Artifacts* (hace falta iniciar sesión en GitHub), donde se guarda 14 días. No
  contiene datos del juego.

El workflow ([package-windows-arm64.yml](../.github/workflows/package-windows-arm64.yml))
hace esto en un Windows ARM64 de GitHub:

| Paso | Qué hace |
|---|---|
| 1 | Descarga ModernGekko-Template en el mismo commit que `build_and_run_linux.sh` y aplica los parches de `patches/moderngekko` |
| 2 | Compila con Visual Studio (MSVC) el launcher, el runner, DolRecomp y `moderngekko-port`, con `packaging/windows/branding.cmake`, que los hace de este juego |
| 3 | Descarga llvm-mingw, CMake y Ninja, en versiones fijas y con su hash comprobado (`tools/fetch_windows_toolchain.py`) |
| 4 | Arma la carpeta del paquete y el zip (`tools/package_windows.py`) |
| 5 | Prueba el zip como lo haría un jugador, con un disco inventado en lugar de uno real: que los programas sean ARM64 y arranquen con solo Windows; la prueba por instrucción con el compilador del paquete; `play.ps1` dos veces, con las rutas largas desactivadas como en la mayoría de las computadoras (la primera lee el disco, recompila e instala el módulo; la segunda debe encontrar todo hecho); y que el juego instalado arranque como lo hace el launcher, sin dibujar nada y con solo las carpetas de Windows en la `PATH`, corra unos segundos y se cierre con código 0 |

El paso 2 es el que más tarda (20 a 40 minutos). Sus programas se guardan en el
caché de GitHub con una clave que cambia si cambian las fuentes, los parches o
`branding.cmake`; una corrida posterior solo arma y prueba el paquete
(unos 8 a 15 minutos).

Para compilarlo a mano en Windows ARM64, los pasos son los del workflow. Lo que
conviene saber:

- Rutas cortas, como `C:\mg`: el módulo recompilado añade unos 200 caracteres a
  la ruta de su carpeta y Windows se detiene en 260. Si prefieres rutas largas,
  activa `LongPathsEnabled` en el registro y `git config --global core.longpaths true`.
- Antes de clonar, `git config --global core.autocrlf false`: los parches solo
  se aplican a archivos con saltos de línea LF.
- Visual Studio con las herramientas de C++ para ARM64, y su entorno activo
  (`vcvarsarm64.bat`, o la consola *Developer PowerShell*) al compilar y al
  instalar las dependencias de Python: `capstone` no publica paquete
  precompilado para Windows ARM64, y `pip` lo compila desde el código fuente.
- Solo se ha probado con la imagen `windows-11-arm` de GitHub: MSVC 19.51 para
  los programas y llvm-mingw (Clang 23.1) para el módulo del juego. Con el
  juego real y en una computadora de verdad, todavía no. Para Windows x64
  debería servir la misma receta, pero tampoco se ha probado.

---

## English

There are two ways, depending on your computer:

- **Ubuntu or Debian, and Windows inside WSL2:** `scripts/build_and_run_linux.sh`
  does the whole trip: it builds ModernGekko, recompiles the game from your
  disc and starts it. It is the way tested with this game, and the subject of
  most of this document.
- **Windows on ARM64 (for example, Snapdragon X), with no WSL:** a zip package
  with `Play.bat`, built on GitHub. It is experimental; see
  [Native Windows ARM64](#native-windows-arm64-no-wsl) at the end.

### What you need

- Ubuntu or Debian, or Windows 10/11 with WSL2.
- Your own *Harvest Moon: Magical Melody* (USA) disc as RVZ, ISO or GCM.
- About 3 GB free, and internet access the first time.

### On Windows: install WSL2

1. In PowerShell: `wsl --install -d Ubuntu-24.04`. Restart if asked.
2. Open Ubuntu from the Start menu and create your Linux user and password.
3. If you already had WSL, run `wsl --update` to get windows (WSLg) and GPU
   access.

Inside Ubuntu, your Windows folders live under `/mnt/c`; for example,
Documents is `/mnt/c/Users/<your user>/Documents`.

### Build and play

In Ubuntu, the first time, with the path to your disc:

```sh
bash /mnt/c/Users/<your user>/Documents/hm-mm-recomp/scripts/build_and_run_linux.sh \
    "/mnt/c/Users/<your user>/Downloads/Harvest Moon - Magical Melody (USA).rvz"
```

A path copied from Explorer (`C:\Users\...`) works too. The first run takes
a while: it downloads a few hundred MB, builds ModernGekko (15 to 40 minutes) and
recompiles the game (a few minutes). After that, this is enough:

```sh
bash /mnt/c/Users/<your user>/Documents/hm-mm-recomp/scripts/build_and_run_linux.sh
```

### What the script does

| Step | What it does | Skipped when |
|---|---|---|
| 1 | Checks for Linux with `apt` and a graphical session | never |
| 2 | Installs the packages ModernGekko-Template asks for | already installed |
| 3 | Looks for the GPU: OpenGL through Direct3D 12 in WSL, Vulkan or OpenGL on Linux | never |
| 4 | Downloads ModernGekko-Template into `~/ModernGekko-Template`, at the tested version (`c2553a9`) | already there |
| 5 | Builds DolRecomp and ModernGekko | already built |
| 6 | Installs `nodtool` (and Rust if missing) and extracts your disc with `tools/extract_disc.py` | disc already extracted |
| 7 | Checks with `tools/check_disc.py` that it is G4AEE9 revision 0 | never |
| 8 | Recompiles the game if needed and starts it | never |

If it stops halfway, run it again: it continues where it was.

### Controls

With no controller configured, ModernGekko uses the keyboard with Dolphin's
default mapping (the `LoadDefaults` function of `GCPadEmu.cpp`). The arrow
keys and the X key were checked while playing, with real key presses; the rest
of the table comes from that code.

| GameCube | Keyboard |
|---|---|
| Stick | Arrow keys (Shift keeps it at half tilt) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| D-pad | T (up), F (left), G (down), H (right) |
| C-stick | I (up), J (left), K (down), L (right) |

The game's menus move with the stick, not the D-pad (checked on the title
menu). A USB or Bluetooth
gamepad does not reach WSL by itself (it needs
[usbipd-win](https://github.com/dorssel/usbipd-win)); on native Linux,
ModernGekko generates its mapping once you name it in `config.ini`
(`[Input]`, `controller1=`).

### Where things live

- `~/ModernGekko-Template`: the tools, the extracted disc
  (`extracted/Harvest-Moon-Magical-Melody`) and the recompiled module
  (`build/modules`).
- `~/.local/share/moderngekko`: settings (`config.ini`), saves
  (`GC/USA/Card A`) and mods (`Mods/`).

### Useful settings (`~/.local/share/moderngekko/config.ini`)

- `resolution=3840x2160` for 4K internal resolution (default `1920x1080`;
  without a GPU the script starts at `640x528`). Accepts 640x528, 1280x720,
  1920x1080, 2560x1440, 3840x2160, 5120x2880 and 7680x4320.
- `fullscreen=true` for full screen.
- The script picks the graphics backend; to force one:
  `GRAPHICS_BACKEND=Vulkan bash .../build_and_run_linux.sh`.

### If something goes wrong

- **`does not contain a CMakeLists.txt file` while building:** the download
  was cut off halfway (Ctrl+C, or the computer went to sleep). Run the
  script again, which finishes the download, or by hand:
  `git -C ~/ModernGekko-Template submodule update --init --recursive --force`.
  Keep the computer awake during the first run.
- **It is very slow:** the script said "no GPU acceleration found", so the
  game is drawn by the processor. With an Intel, AMD or NVIDIA GPU, update
  Windows, the GPU driver and WSL (`wsl --update`). On Windows laptops with
  an ARM processor (Snapdragon), WSL2 cannot use the GPU yet
  ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)) and
  those steps do not help. Without a GPU the script writes
  `resolution=640x528` to `config.ini` the first time, so the processor has
  less to draw. If you ran the game before, the file already exists and the
  script leaves it alone: change it yourself with
  `sed -i 's/^resolution=.*/resolution=640x528/' ~/.local/share/moderngekko/config.ini`.
  The real fix is a native Windows ARM64 build, with no WSL: see
  [Native Windows ARM64](#native-windows-arm64-no-wsl) at the end; it is
  experimental and has not been tried with the real game yet.
- **"WSL has no window support yet":** run `wsl --update` and reopen Ubuntu.
- **`$'\r': command not found`:** the script has Windows line endings. The
  repository's `.gitattributes` prevents that; if it happens,
  `sed -i 's/\r$//' scripts/build_and_run_linux.sh` fixes it.
- **"is not the G4AEE9 revision 0 disc":** delete
  `~/ModernGekko-Template/extracted/Harvest-Moon-Magical-Melody` and run the
  script again with the right image.

### Native Windows ARM64 (no WSL)

For laptops with Windows on ARM (for example, with Snapdragon X), where WSL2
cannot use the GPU yet, there is a zip package that plays natively: it has the
launcher, the runner, the recompiler (DolRecomp) and a portable C compiler, and
nothing has to be installed. The game is not included: it is recompiled on your
computer, from your disc, the first time.

- **To play:** follow [packaging/windows/README.md](../packaging/windows/README.md):
  extract the zip, double-click `Play.bat` and choose the disc image (or
  `Play-Keyboard.bat` if you have no controller).
- **Where the zip comes from:** GitHub Actions. In the repository: *Actions* →
  *Windows ARM64 package* → *Run workflow*. When it ends, the zip is under
  *Artifacts* (you need to be signed in to GitHub), where it is kept for 14
  days. It holds no game data.

The workflow ([package-windows-arm64.yml](../.github/workflows/package-windows-arm64.yml))
does this on a Windows ARM64 machine of GitHub:

| Step | What it does |
|---|---|
| 1 | Downloads ModernGekko-Template at the same commit as `build_and_run_linux.sh` and applies the patches in `patches/moderngekko` |
| 2 | Builds the launcher, the runner, DolRecomp and `moderngekko-port` with Visual Studio (MSVC), with `packaging/windows/branding.cmake`, which makes them this game's |
| 3 | Downloads llvm-mingw, CMake and Ninja, at fixed versions and with their hash checked (`tools/fetch_windows_toolchain.py`) |
| 4 | Assembles the package folder and the zip (`tools/package_windows.py`) |
| 5 | Tests the zip as a player would use it, with a made-up disc instead of a real one: that the programs are ARM64 and start with nothing but Windows; the instruction-level test with the package's compiler; `play.ps1` twice, with long paths turned off as on most computers (the first run reads the disc, recompiles and installs the module; the second must find everything done); and that the installed game starts the way the launcher starts it, drawing nothing and with only the folders of Windows on the path, runs for a few seconds and closes with exit code 0 |

Step 2 is the slow one (20 to 40 minutes). Its programs are kept in GitHub's
cache under a key that changes when the sources, the patches or
`branding.cmake` change; a later run only assembles and tests the package
(about 8 to 15 minutes).

To build it by hand on Windows ARM64, follow the workflow's steps. What is
worth knowing:

- Short paths, such as `C:\mg`: the recompiled module adds about 200
  characters to the path of its folder and Windows stops at 260. If you prefer
  long paths, turn on `LongPathsEnabled` in the registry and run
  `git config --global core.longpaths true`.
- Before cloning, `git config --global core.autocrlf false`: the patches only
  apply to files with LF line endings.
- Visual Studio with the C++ tools for ARM64, and its environment active
  (`vcvarsarm64.bat`, or the *Developer PowerShell* console) when building and
  when installing the Python dependencies: `capstone` ships no prebuilt
  package for Windows ARM64, and `pip` builds it from source.
- It has only been tried on GitHub's `windows-11-arm` image: MSVC 19.51 for
  the programs and llvm-mingw (Clang 23.1) for the game's module. Not with the
  real game, and not on a real computer yet. The same recipe should work for
  Windows x64, but that has not been tried either.
