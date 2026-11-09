# Paquete nativo / Native package

[Español](#español) · [English](#english)

---

## Español

Cómo llega la versión nativa (sin Dolphin, [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md))
a un jugador sin que el paquete lleve nada del juego.

### Qué lleva el paquete y qué no

El programa del juego (`hmmm-native`) es el código del juego traducido a C++ y
compilado junto con el runtime, así que no se puede repartir. El paquete lleva
todo lo demás, y el programa se hace en la computadora de cada jugador, con su
propio disco, la primera vez que juega:

- el runtime ya compilado, sin el juego: el **kit del runtime**;
- el traductor de WiiCompiled, un compilador de C++ (llvm-mingw) y Python;
- las herramientas de este repositorio que preparan el juego, los símbolos
  (`config/G4AEE9/symbols.txt`), el código del runtime que el traductor lee y
  el de los mods;
- nodtool, para convertir a ISO una imagen comprimida (RVZ y similares).

Ningún archivo del paquete sale del disco.

### Preparar el juego (`tools/prepare_game.py`)

Hace en la computadora del jugador lo mismo que
`scripts/build_native_runtime.sh`, pero con el kit en lugar de compilar el
runtime. Lo corre el launcher, y se puede correr a mano:

```sh
python3 tools/prepare_game.py --disc <imagen> --kit <kit> --translator <Translator.Cli> \
    --out <carpeta> [--toolchain <llvm-mingw>] [--nodtool <nodtool>] [--jobs <n>]
```

| Paso | Qué hace |
|---|---|
| `disc` | Si la imagen está comprimida (RVZ, WIA, GCZ, CISO, WBFS), nodtool la convierte a ISO en `<carpeta>/disc/G4AEE9.iso`. Comprueba que el disco sea Harvest Moon: Magical Melody de EE. UU. (`G4AEE9`, revisión 0) y que su `main.dol` tenga el SHA-1 que espera el port, y lo saca de la imagen. |
| `translate` | `tools/write_translator_project.py` escribe el proyecto y el traductor traduce `main.dol` a C++. |
| `adapt` | La cuadrícula de cultivos (`tools/size_crop_grid.py`) y los lugares donde actúan los mods (`tools/insert_mod_hooks.py`). |
| `compile` | Compila el juego traducido con los comandos del kit, en todos los procesadores a la vez. |
| `link` | Lo enlaza con el runtime del kit: el programa. |
| `finish` | Copia junto al programa lo que necesita (en Windows, las DLL de Dawn) y escribe `prepared.json`. |

Lo que imprime es para el launcher: una línea que empieza con `@` es un paso
(`@stage`, `@progress`, `@error`, `@done`, explicados en el script) y las demás
son detalle para el registro. Con un `@error` dice por qué (`wrong_game`,
`wrong_version`, `modified_game`, `compressed`, `tool_failed`, ...), para que
el launcher se lo explique al jugador en su idioma. La carpeta de trabajo (unos
300 MB) se borra al terminar.

`prepared.json` dice qué imagen lee el juego (la del jugador o la convertida),
con qué mods se marcó y con qué versión del paquete se hizo: el launcher
vuelve a preparar el juego cuando cambia la versión del paquete.

En la nube (2 procesadores) la preparación completa, desde el RVZ, tarda unos 3
minutos: la conversión 3 s, el traductor unos 20 s y la compilación el resto.
Un procesador de 8 núcleos compila unas cuatro veces más rápido.

### El launcher (`launcher/`)

La ventana que abre el jugador (`HarvestMoonMM.exe`), en C++ con el
renderizador 2D de SDL3 y Dear ImGui, sin nada del juego. Su aspecto sigue al
del juego, dibujado con figuras: el cielo, las nubes, las colinas y la cerca de
la pantalla de título, un letrero de madera con el nombre, botones naranjas
como el recuadro de la fecha y paneles como los cuadros de diálogo, con una
letra redondeada (M PLUS Rounded 1c, libre; `launcher/fonts/README.md`). Se usa
con el ratón, el teclado o un control, y habla español o inglés (como el
sistema, o lo que se elija).

| Pantalla | Qué hace |
|---|---|
| Bienvenida | Pide la imagen del disco del jugador (`.iso`, `.gcm` o `.rvz`; también se puede soltar sobre la ventana o pasar al programa). Si el paquete se actualizó, avisa que hay que preparar el juego otra vez. |
| Preparando | Corre `tools/prepare_game.py` y muestra sus pasos con palomitas, una barra de progreso, cuánto lleva y, si se quiere, los detalles. Se puede cancelar. Si algo falla, explica por qué (otro juego, otra versión, una imagen comprimida que no se pudo convertir, ...) y deja abrir el registro (`prepare.log`). |
| Principal | Jugar, Opciones, Mods, Controles y Salir; abajo, el disco y la versión. |
| Opciones | Resolución interna, pantalla completa, pantalla ancha, cuadros por segundo, zoom de la cámara, velocidad de todo el juego e idioma. |
| Mods | Como un gestor de mods: a la izquierda la lista con un interruptor por mod, a la derecha la descripción, los ajustes (la velocidad del personaje) y el aviso del mod elegido (con Jamie: no desactivarlo después de casarse con Jamie). Lo leen de los manifiestos `mods/<id>/mod.json`. |
| Controles | Los controles conectados y el teclado, con la asignación automática del juego (la de aurora, según el tipo de control: Xbox, PlayStation, Nintendo, GameCube) y un control de GameCube dibujado que se ilumina con lo que se presiona. |

Las elecciones se guardan en `settings.json`, en la carpeta del jugador (el
mismo archivo y los mismos nombres que usaban los paquetes anteriores). «Jugar»
arranca el programa preparado con esas opciones y esconde el launcher; cuando
el juego se cierra, el launcher vuelve, y si el juego terminó por un error lo
dice y deja abrir `native.log`. En Windows los programas se arrancan sin
ventana de consola, y la preparación en un *job* que termina todo lo que
empezó si se cancela o si se cierra el launcher.

Dónde queda cada cosa del jugador: `%LOCALAPPDATA%\HarvestMoonMM` en Windows
(`~/.local/share/HarvestMoonMM` en Linux): `settings.json`, y en `Native` las
partidas, `native.log`, `prepare.log` y, en `Native/Game`, el programa
preparado, `prepared.json` y, si hizo falta convertir el disco, su ISO.

Compilarlo (Linux; para Windows, con el *toolchain* de llvm-mingw):

```sh
cmake -S launcher -B build/launcher -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/launcher
```

### El kit del runtime (`tools/make_runtime_kit.py`)

El runtime se compila como siempre, pero con un sustituto del juego traducido
(`runtime/kit/stub_game`, que solo tiene `InitializeDataSections`) y con
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. El script lee esa compilación:

- de `compile_commands.json`, cómo se compiló el sustituto, que es como se
  compila el juego traducido (son el mismo target de CMake);
- de Ninja, el comando que enlazó el programa, con cada objeto y biblioteca en
  orden.

Copia los objetos y bibliotecas al kit (`lib/`), los encabezados con los que se
compila el juego traducido (`include/`: la ABI de WiiCompiled y los nombres de
las funciones del juego para los mods) y las DLL que van junto al programa
(`bin/`), y escribe `recipe.json`: esos comandos con las rutas de esta
computadora cambiadas por marcadores (`{kit}`, `{translation}`,
`{game_objects}`, `{output}`). Una ruta que no sabe cambiar lo detiene, así que
ninguna receta depende de la computadora que la hizo. Las opciones de
información de depuración (`-g`) no pasan a la receta: hacían los objetos del
juego varias veces más grandes y nadie las usa en la computadora del jugador.

### Probarlo en Linux

```sh
cmake -S runtime -B build/native/runtime-kit -G Ninja \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DWIICOMPILED_DIR=<WiiCompiled> -DTRANSLATED_GAME_DIR=$PWD/runtime/kit/stub_game \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/native/runtime-kit
python3 tools/make_runtime_kit.py --build-directory build/native/runtime-kit \
    --wiicompiled-directory <WiiCompiled> --out build/native/kit
python3 tools/prepare_game.py --disc <tu disco> --kit build/native/kit \
    --translator <WiiCompiled>/translator/src/Translator.Cli/bin/Release/net8.0/Translator.Cli.dll \
    --out build/native/game --nodtool "$(command -v nodtool)"
build/native/game/hmmm-native --disc build/native/game/disc/G4AEE9.iso
```

Se probó así con el RVZ del juego: el programa preparado llega a la pantalla de
título y los mods quedan marcados.

---

## English

How the native version (without Dolphin, [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md))
reaches a player without the package carrying anything of the game.

### What the package carries and what it does not

The game's program (`hmmm-native`) is the game's code translated to C++ and
compiled together with the runtime, so it cannot be handed out. The package
carries everything else, and the program is made on each player's computer,
from their own disc, the first time they play:

- the runtime already compiled, without the game: the **runtime kit**;
- WiiCompiled's translator, a C++ compiler (llvm-mingw) and Python;
- the tools of this repository that prepare the game, the symbols
  (`config/G4AEE9/symbols.txt`), the runtime's code that the translator reads
  and the mods' code;
- nodtool, to convert a compressed image (RVZ and the like) to ISO.

No file of the package comes from the disc.

### Preparing the game (`tools/prepare_game.py`)

It does on the player's computer what `scripts/build_native_runtime.sh` does,
but with the kit instead of compiling the runtime. The launcher runs it, and it
can be run by hand:

```sh
python3 tools/prepare_game.py --disc <image> --kit <kit> --translator <Translator.Cli> \
    --out <folder> [--toolchain <llvm-mingw>] [--nodtool <nodtool>] [--jobs <n>]
```

| Step | What it does |
|---|---|
| `disc` | If the image is compressed (RVZ, WIA, GCZ, CISO, WBFS), nodtool converts it to ISO in `<folder>/disc/G4AEE9.iso`. It checks that the disc is Harvest Moon: Magical Melody for the USA (`G4AEE9`, revision 0) and that its `main.dol` has the SHA-1 the port expects, and takes it out of the image. |
| `translate` | `tools/write_translator_project.py` writes the project and the translator translates `main.dol` to C++. |
| `adapt` | The crop grid (`tools/size_crop_grid.py`) and the places where the mods act (`tools/insert_mod_hooks.py`). |
| `compile` | Compiles the translated game with the kit's commands, on every processor at once. |
| `link` | Links it with the kit's runtime: the program. |
| `finish` | Copies beside the program what it needs (on Windows, Dawn's DLLs) and writes `prepared.json`. |

What it prints is for the launcher: a line that starts with `@` is a step
(`@stage`, `@progress`, `@error`, `@done`, explained in the script) and the
rest is detail for the log. With an `@error` it says why (`wrong_game`,
`wrong_version`, `modified_game`, `compressed`, `tool_failed`, ...), so that
the launcher can explain it to the player in their language. The work folder
(about 300 MB) is deleted at the end.

`prepared.json` says which image the game reads (the player's or the
converted one), which mods it was marked for and which version of the package
made it: the launcher prepares the game again when the package's version
changes.

In the cloud (2 processors) the whole preparation, from the RVZ, takes about 3
minutes: the conversion 3 s, the translator about 20 s and the compile the
rest. An 8-core processor compiles about four times as fast.

### The launcher (`launcher/`)

The window the player opens (`HarvestMoonMM.exe`), in C++ with SDL3's 2D
renderer and Dear ImGui, with nothing of the game. Its look follows the
game's, drawn with shapes: the sky, the clouds, the hills and the fence of the
title screen, a wooden sign with the name, orange buttons like the box of the
date and panels like the dialogue boxes, with a rounded font (M PLUS Rounded
1c, free; `launcher/fonts/README.md`). It is used with the mouse, the keyboard
or a controller, and speaks Spanish or English (as the system, or as chosen).

| Screen | What it does |
|---|---|
| Welcome | Asks for the player's disc image (`.iso`, `.gcm` or `.rvz`; it can also be dropped on the window or given to the program). If the package was updated, it says that the game has to be prepared again. |
| Preparing | Runs `tools/prepare_game.py` and shows its steps with check marks, a progress bar, the time so far and, on request, the details. It can be cancelled. If something fails, it explains why (another game, another version, a compressed image that could not be converted, ...) and offers to open the log (`prepare.log`). |
| Main | Play, Options, Mods, Controls and Exit; below, the disc and the version. |
| Options | Internal resolution, full screen, widescreen, frames per second, camera zoom, speed of the whole game and language. |
| Mods | Like a mod manager: on the left the list with a switch for each mod, on the right the chosen mod's description, settings (the character's speed) and warning (with Jamie: not to turn it off after marrying Jamie). They come from the manifests `mods/<id>/mod.json`. |
| Controls | The connected controllers and the keyboard, with the game's automatic mapping (aurora's, by the kind of controller: Xbox, PlayStation, Nintendo, GameCube) and a drawn GameCube controller that lights up with what is pressed. |

The choices are kept in `settings.json`, in the player's folder (the same file
and the same names as the earlier packages used). "Play" starts the prepared
program with those options and hides the launcher; when the game closes, the
launcher comes back, and if the game ended with an error it says so and offers
to open `native.log`. On Windows the programs are started without a console
window, and the preparation in a job that ends everything it started if it is
cancelled or if the launcher is closed.

Where the player's things go: `%LOCALAPPDATA%\HarvestMoonMM` on Windows
(`~/.local/share/HarvestMoonMM` on Linux): `settings.json`, and in `Native` the
saves, `native.log`, `prepare.log` and, in `Native/Game`, the prepared
program, `prepared.json` and, if the disc had to be converted, its ISO.

Building it (Linux; for Windows, with the llvm-mingw toolchain):

```sh
cmake -S launcher -B build/launcher -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/launcher
```

### The runtime kit (`tools/make_runtime_kit.py`)

The runtime is built as usual, but with a stand-in of the translated game
(`runtime/kit/stub_game`, which only has `InitializeDataSections`) and with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. The script reads that build:

- from `compile_commands.json`, how the stand-in was compiled, which is how the
  translated game is compiled (they are the same CMake target);
- from Ninja, the command that linked the program, with every object and
  library in order.

It copies the objects and libraries into the kit (`lib/`), the headers the
translated game is compiled with (`include/`: WiiCompiled's ABI and the names
of the game's functions for the mods) and the DLLs that go beside the program
(`bin/`), and writes `recipe.json`: those commands with the paths of this
computer replaced by placeholders (`{kit}`, `{translation}`,
`{game_objects}`, `{output}`). A path it does not know how to replace stops it,
so no recipe depends on the computer that made it. The debugging-information
options (`-g`) do not go into the recipe: they made the game's objects several
times bigger, and nobody uses them on the player's computer.

### Trying it on Linux

```sh
cmake -S runtime -B build/native/runtime-kit -G Ninja \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DWIICOMPILED_DIR=<WiiCompiled> -DTRANSLATED_GAME_DIR=$PWD/runtime/kit/stub_game \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/native/runtime-kit
python3 tools/make_runtime_kit.py --build-directory build/native/runtime-kit \
    --wiicompiled-directory <WiiCompiled> --out build/native/kit
python3 tools/prepare_game.py --disc <your disc> --kit build/native/kit \
    --translator <WiiCompiled>/translator/src/Translator.Cli/bin/Release/net8.0/Translator.Cli.dll \
    --out build/native/game --nodtool "$(command -v nodtool)"
build/native/game/hmmm-native --disc build/native/game/disc/G4AEE9.iso
```

It was tried this way with the game's RVZ: the prepared program reaches the
title screen and the mods are marked.
