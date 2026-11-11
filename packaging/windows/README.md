# Harvest Moon: Magical Melody – Windows ARM64 package

[Español](#español) · [English](#english)

---

## Español

Paquete para jugar *Harvest Moon: Magical Melody* (GameCube, versión USA, ID de disco `G4AEE9`)
de forma nativa en computadoras con **Windows en ARM** (por ejemplo, con procesador Snapdragon X),
sin emulador y sin instalar nada.

**El juego no viene incluido.** Necesitas tu propia copia del disco como imagen (`.iso`, `.gcm`,
`.rvz`, `.wbfs`, `.ciso`, `.gcz`, `.wia` o `.tgc`). Este paquete trae solo las herramientas que
recompilan el juego en tu computadora, a partir de tu disco.

> **Estado: experimental.** Las herramientas se compilan en Windows ARM64 y pasan ahí la prueba
> de instrucciones contra el intérprete de Dolphin (ver [VALIDATION.md][validation]). El paquete
> se prueba en GitHub con un disco inventado: extrae el disco, recompila el juego, lo instala y
> lo arranca sin pantalla, y se cierra bien. Además se probó con el juego real en una laptop con
> Snapdragon, con DirectX 12: corre a velocidad completa con el cargador conectado (ver
> «Rendimiento y batería»). Todavía falta confirmar el audio, el control y el teclado en Windows
> ARM64. Si algo falla, el registro `Logs\play.log` (ver más abajo) ayuda a saber por qué.

### Cómo usarlo

1. Descarga el archivo zip. Antes de extraerlo, haz clic derecho sobre él → **Propiedades** y, si
   aparece la casilla **Desbloquear** abajo, márcala y pulsa **Aplicar**. Luego extráelo
   completo (**Extraer todo…**) en una carpeta con ruta corta, por ejemplo `C:\HarvestMoonMM`.
2. Haz doble clic en `Play.bat`. Si Windows muestra «Windows protegió su PC», pulsa
   **Más información** → **Ejecutar de todas formas**.
3. La primera vez se abre una ventana para elegir la imagen de tu disco. También puedes
   arrastrar el archivo de la imagen y soltarlo sobre `Play.bat`.
4. Espera. Se prepara el juego para tu computadora: se leen los archivos del disco y el programa
   del juego se recompila. Se hace **una sola vez** y puede tardar varios minutos. No cierres la
   ventana negra.
5. Se abre el launcher. Pulsa **Play**. Con un control compatible conectado se usa el control; sin
   control se juega con el teclado (ver «Controles»). `Play-Keyboard.bat` abre el juego con el
   teclado sin pasar por el launcher.

Las veces siguientes basta con hacer doble clic en `Play.bat` (o en `Play-Keyboard.bat`). Para
activar o apagar los mods y elegir qué tan rápido va el personaje, abre `Settings.bat` (ver «Mods»).

### Controles

**Con control.** `Play.bat` abre el launcher, que usa un control compatible con SDL (Xbox,
PlayStation, Switch Pro, 8BitDo, …) y lo asigna a un control de GameCube. Si no hay ningún control
conectado, el launcher lo dice en su línea de estado y **Play** abre el juego con el teclado: solo
la partida en red (**Host Netplay** y **Join Netplay**) exige un control («Connect an
SDL-compatible controller before playing»).

**Con teclado.** `Play.bat` sin un control conectado, o `Play-Keyboard.bat` (que no abre el
launcher), abre el juego con la asignación de teclado de Dolphin (el juego corre sobre Dolphin).
Todavía no se puede cambiar desde el paquete:

| GameCube | Teclado |
| --- | --- |
| Palanca | Flechas (con Shift, a media inclinación) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| Cruceta | T (arriba), F (izquierda), G (abajo), H (derecha) |
| Palanca C | I (arriba), J (izquierda), K (abajo), L (derecha); con Ctrl, a media inclinación |

Los menús del juego se mueven con la palanca, no con la cruceta. Las flechas y la tecla X se
probaron jugando en Linux; el resto de la tabla sale del código de Dolphin. En Windows ARM64
todavía no se ha probado.

### Rendimiento y batería

El juego corre a 30 cuadros por segundo, como en la consola. En una laptop con Snapdragon y
DirectX 12, con el cargador conectado, va a velocidad completa. El paquete ya trae los dos ajustes
que lo logran (saltar el bucle en el que el juego espera sin hacer nada y no emular la lentitud del
lector de discos), así que no hay nada que configurar. Las cifras y el porqué están en
[PERFORMANCE.md][performance].

- **Conecta el cargador.** En batería, con el modo de energía normal de Windows, la ciudad se queda
  corta: el juego va a alrededor de 0.8 de su velocidad y se siente lento. En **Configuración →
  Sistema → Energía y batería** elige el modo de energía **Mejor rendimiento** y desactiva el
  ahorro de batería; todavía no se ha medido cuánto ayuda.
- **Al entrar o salir de un lugar** puede haber una pequeña caída de cuadros (menos de un segundo):
  es el juego cargando.
- **Para medir cómo corre en tu computadora**, ejecuta `Play-Keyboard.bat` una vez y luego
  `Measure-Speed.bat`. Abre el juego y, cada pocos segundos, anota en
  `%LOCALAPPDATA%\HarvestMoonMM\Logs\speed.log` la velocidad (1.00 es la completa), cuánto procesador
  usa cada hilo, el reloj del procesador, si la computadora está en batería y el modo de energía de
  Windows; al cerrar el juego agrega un resumen. No contiene datos del juego, pero si el juego falla
  puede traer rutas de tu computadora: revísalo antes de compartirlo.
- **Para cambiar uno de los dos ajustes solo para ti**, crea el archivo
  `%LOCALAPPDATA%\HarvestMoonMM\GameSettings\G4AEE9.ini` con la misma línea; por ejemplo, para
  volver a la lentitud original del disco:

  ```
  [Core]
  FastDiscSpeed = False
  ```

### Mods

Un mod es un pequeño cambio al juego que se carga al arrancarlo. El paquete trae dos:

| Mod | Qué hace |
| --- | --- |
| `jamie_marriage` | Casarse con Jamie ya no termina el juego: después de la ceremonia sigues jugando. Sin el mod, a la ceremonia le siguen el epílogo y la pantalla de título; con él se ve una vez la escena de las notas musicales y el juego vuelve a juego libre. |
| `player_speed` | El personaje camina y corre más rápido: por omisión al 100 % (sin cambio) de la velocidad original. Solo actúa en juego libre; en las escenas el personaje se mueve como en el original. La animación no cambia, así que con valores altos los pies podrían verse resbalar. |

**Para activar o apagar los mods y elegir la velocidad del personaje**, haz doble clic en
`Settings.bat`. Se abre una ventana con una casilla por cada mod y, para el personaje más rápido, un
deslizador y una casilla numérica de 25 % (muy lento) a 300 % (tres veces más rápido), donde 100 % es
la velocidad del juego original. **Guardar** guarda la elección, **Guardar y jugar** la guarda y abre
el juego con el teclado, **Guardar y abrir el launcher** la guarda y abre el launcher, y **Valores
originales** vuelve a todos los mods activos y al 100 %. La elección se aplica la próxima vez que se
arranca el juego con `Play.bat` o `Play-Keyboard.bat`: un juego que ya está abierto no cambia. Detrás
de la ventana se queda abierta una ventana negra; es normal. Mientras no hayas guardado nada, todos
los mods están activos y el personaje va al 100 %.

**Avance rápido.** En la misma ventana, **Velocidad de todo el juego** hace que todo vaya más rápido,
incluidas las escenas y los diálogos, como el avance rápido de un emulador: de 100 % (normal) a 400 %.
No es un mod: `Play.bat` se lo pasa al núcleo de emulación (`EmulationSpeed` de la sección `[Core]` de
`%LOCALAPPDATA%\HarvestMoonMM\Config\Dolphin.ini`). El reloj del juego también corre más rápido y el
sonido puede cortarse; si la computadora no alcanza la velocidad elegida, el juego va tan rápido como
puede. Por ahora se elige antes de jugar; poder activarlo y apagarlo con una tecla mientras juegas
(la barra espaciadora) necesita un cambio en el programa que corre el juego y llegará con un paquete
nuevo.

La elección queda en `%LOCALAPPDATA%\HarvestMoonMM\settings.json`, un archivo que también se puede
editar a mano con un editor de texto:

```json
{
  "mods": { "jamie_marriage": true, "player_speed": true },
  "playerSpeedPercent": 100,
  "gameSpeedPercent": 100
}
```

Un mod en `false` se quita de tu carpeta y el juego se juega sin él; uno que no aparece está activo.
`playerSpeedPercent` es un número entero de 25 a 300 y `gameSpeedPercent` uno de 100 a 400. Un valor que no sirve se ignora (y `Play.bat`
lo avisa) y el resto del archivo se sigue usando; si borras el archivo, vuelven los valores
originales.

`Play.bat` y `Play-Keyboard.bat` construyen los mods activos en tu computadora cada vez que se
ejecutan, con el mismo compilador que recompila el juego (tarda unos segundos), y los dejan en
`%LOCALAPPDATA%\HarvestMoonMM\Mods`. Si no se pueden construir, el juego arranca igual y el motivo
queda en `Logs\play.log`.

**Desde la línea de comandos** (abre una ventana de comandos en la carpeta del paquete):

- `Play.bat -NoMods` (o `Play-Keyboard.bat -NoMods`) juega sin ningún mod, sin mirar `settings.json`:
  quita de tu carpeta los mods del paquete y el juego se juega como viene en el disco. Para
  recuperarlos, ejecuta `Play.bat` sin eso.
- `Play.bat -PlayerSpeed 200` (o `Play-Keyboard.bat -PlayerSpeed 200`) elige la velocidad solo para
  esa vez que juegas y le gana a la que guardó `Settings.bat`: un número entero de 25 a 300. No tiene
  efecto con `-NoMods`, ni si el mod `player_speed` está apagado en los ajustes.

Por encima de 200 el personaje puede atravesar paredes delgadas: con 400 atravesó la puerta de su
casa en las seis pruebas y cayó fuera del mapa, con 300 no, pero el resto de las paredes del juego no
se probaron ([MOVEMENT.md][movement-es]).

> **Estado: el mod de Jamie se probó con las condiciones de la boda escritas a mano, no con una
> partida real** (ver [los resultados][scripts-es]). Si lo pruebas en una partida tuya, copia antes
> la carpeta `%LOCALAPPDATA%\HarvestMoonMM\GC`, que es donde está tu partida guardada.

### Dónde guarda las cosas

Todo lo que crea el paquete está en la carpeta `%LOCALAPPDATA%\HarvestMoonMM` (pégala en la barra
de direcciones del Explorador de archivos): el juego extraído (`games`), el módulo recompilado
(`modules` y `StaticRecompModules`), los mods (`Mods`, y `mods-build` donde se construyen), tus
partidas guardadas, la configuración (incluida tu elección de mods y velocidad, `settings.json`) y el
registro de la última preparación (`Logs\play.log`). Para desinstalar, borra la carpeta del paquete y
esa carpeta.

### Si algo falla

- **Windows bloquea los programas.** El paquete no está firmado digitalmente. Desbloquea el zip
  antes de extraerlo (paso 1). Si tu Windows tiene el *Control inteligente de aplicaciones* en modo
  **Activado**, bloquea todo programa sin firma y este paquete no funcionará mientras siga así.
- **El antivirus pone archivos en cuarentena** (suele pasar con el compilador de `toolchain`).
  Agrega la carpeta del paquete y `%LOCALAPPDATA%\HarvestMoonMM` a las excepciones y extrae el zip
  otra vez.
- **«No se pudo leer la imagen de disco».** Este paquete solo juega la versión USA del juego
  (ID `G4AEE9`). Una imagen de otra región u otra edición no sirve.
- **La preparación falla.** Abre `%LOCALAPPDATA%\HarvestMoonMM\Logs\play.log`. Si lo compartes
  en un reporte de errores, no contiene datos del juego. Si el error habla de rutas demasiado
  largas, mueve el paquete a una carpeta más corta.
- **Dice «No se pudieron preparar los mods».** El juego arranca igual, sin ellos (o con la copia que
  quedó de la vez anterior). `Logs\play.log` tiene lo que dijeron CMake y el compilador.
- **El juego no abre o la imagen sale negra.** Actualiza el controlador de video y, en el
  launcher, cambia el **Graphics backend** (en Windows ARM64: DirectX 12, DirectX 11 o Vulkan; en
  Windows x64 también OpenGL). Un launcher nuevo empieza con DirectX 12 en Windows ARM64, pero un
  `config.ini` que ya existía conserva su elección (por ejemplo Vulkan, con el que la ventana sale
  negra en la laptop Snapdragon donde se probó): elige DirectX 12 una vez. El launcher abre aunque
  no haya control, y **Play** también (se juega con el teclado); solo la partida en red lo pide.
  Después vuelve a abrir el juego como antes.
- **Jugué con un control y ahora quiero el teclado.** El launcher recuerda el control que usaste y
  `Play-Keyboard.bat` también lo usa. Para volver al teclado, cierra el juego y borra
  `%LOCALAPPDATA%\HarvestMoonMM\config.ini` y `%LOCALAPPDATA%\HarvestMoonMM\Config\GCPadNew.ini`
  (se crean otra vez; se pierde la resolución elegida, no tus partidas guardadas).

### Contenido del paquete

| Carpeta o archivo | Para qué sirve |
| --- | --- |
| `Play.bat`, `Play-Keyboard.bat`, `play.ps1`, `game.psd1` | Lo que haces doble clic (con el launcher, o con el teclado), el script que prepara el juego y los datos del juego. |
| `Settings.bat`, `Settings.ps1`, `settings-file.ps1` | La ventana donde activas y apagas los mods y eliges la velocidad del personaje (ver «Mods»), y el código que ella y `play.ps1` comparten para guardar y leer tu elección. |
| `Measure-Speed.bat`, `Measure-Speed.ps1` | Opcionales: abren el juego con el teclado y anotan qué tan rápido corre (ver «Rendimiento y batería»). |
| `bin\` | El launcher, el runner, DolRecomp (el recompilador) y `moderngekko-port` (lo que los une). `bin\source` trae los pocos archivos de código que hacen falta para construir el módulo del juego y los headers de ModernGekko que incluye un mod. |
| `mods\` | El código de los mods y el header con los nombres de las funciones del juego con los que están escritos; `play.ps1` los construye e instala. |
| `toolchain\` | El compilador de C portátil (llvm-mingw), CMake, Ninja y Python, que construyen el módulo. No se instala nada en Windows. |
| `licenses\`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `VERSIONS.txt` | Licencias, componentes de terceros y las versiones exactas con las que se hizo este paquete. |

### Licencia y código fuente

Todo es software libre; los detalles y dónde está el código fuente de cada parte están en
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). El código de este proyecto está en
<https://github.com/RodoLy/HMMMGC> bajo GPL-3.0-or-later.

---

## English

A package to play *Harvest Moon: Magical Melody* (GameCube, USA release, disc ID `G4AEE9`)
natively on computers with **Windows on ARM** (for example with a Snapdragon X processor), with no
emulator and nothing to install.

**The game is not included.** You need your own copy of the disc as an image (`.iso`, `.gcm`,
`.rvz`, `.wbfs`, `.ciso`, `.gcz`, `.wia` or `.tgc`). This package only has the tools that
recompile the game on your computer, from your disc.

> **Status: experimental.** The tools are built on Windows ARM64 and pass the instruction test
> against Dolphin's interpreter there (see [VALIDATION.md][validation]). The package is tested
> on GitHub with a made-up disc: it extracts the disc, recompiles the game, installs it and
> starts it with nothing drawn, and it closes properly. It was also tried with the real game on a
> laptop with a Snapdragon, with DirectX 12: it runs at full speed with the charger connected (see
> "Performance and battery"). The audio, the controller and the keyboard on Windows ARM64 still
> have to be confirmed. If something fails, the log `Logs\play.log` (see below) helps to find out
> why.

### How to use it

1. Download the zip file. Before extracting it, right-click it → **Properties** and, if the
   **Unblock** checkbox is shown at the bottom, tick it and press **Apply**. Then extract it
   completely (**Extract All…**) into a folder with a short path, for example `C:\HarvestMoonMM`.
2. Double-click `Play.bat`. If Windows shows "Windows protected your PC", press **More info** →
   **Run anyway**.
3. The first time, a window asks for the image of your disc. You can also drag the image file
   and drop it on `Play.bat`.
4. Wait. The game is prepared for your computer: the files of the disc are read and the game's
   program is recompiled. This is done **only once** and can take several minutes. Do not close
   the black window.
5. The launcher opens. Press **Play**. With a compatible controller connected the controller is
   used; with none you play with the keyboard (see "Controls"). `Play-Keyboard.bat` opens the game
   with the keyboard without going through the launcher.

The next times, just double-click `Play.bat` (or `Play-Keyboard.bat`).

### Controls

**With a controller.** `Play.bat` opens the launcher, which uses an SDL-compatible controller
(Xbox, PlayStation, Switch Pro, 8BitDo, …) and maps it to a GameCube controller. If no controller
is connected, the launcher says so in its status line and **Play** opens the game with the
keyboard: only netplay (**Host Netplay** and **Join Netplay**) requires a controller ("Connect an
SDL-compatible controller before playing").

**With the keyboard.** `Play.bat` with no controller connected, or `Play-Keyboard.bat` (which does
not open the launcher), opens the game with Dolphin's default keyboard layout (the game runs on
Dolphin). It cannot be changed from the package yet:

| GameCube | Keyboard |
| --- | --- |
| Stick | Arrow keys (with Shift, half tilt) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| D-pad | T (up), F (left), G (down), H (right) |
| C-stick | I (up), J (left), K (down), L (right); with Ctrl, half tilt |

The game's menus move with the stick, not the D-pad. The arrow keys and the X key were tried
while playing on Linux; the rest of the table comes from Dolphin's code. On Windows ARM64 it has
not been tried yet.

### Performance and battery

The game runs at 30 frames per second, like the console. On a laptop with a Snapdragon and
DirectX 12, with the charger connected, it runs at full speed. The package already carries the two
settings that achieve it (skipping the loop in which the game waits with nothing to do, and not
emulating the disc drive's slowness), so there is nothing to configure. The figures and the reason
are in [PERFORMANCE.md][performance].

- **Connect the charger.** On battery, with Windows' normal power mode, the city falls short: the
  game runs at about 0.8 of its speed and feels slow. In **Settings → System → Power & battery**
  choose the **Best performance** power mode and turn battery saver off; how much that helps has
  not been measured yet.
- **When entering or leaving a place** there may be a small drop in frames (under a second): it is
  the game loading.
- **To measure how it runs on your computer**, run `Play-Keyboard.bat` once and then
  `Measure-Speed.bat`. It opens the game and, every few seconds, writes to
  `%LOCALAPPDATA%\HarvestMoonMM\Logs\speed.log` the speed (1.00 is full speed), how much processor
  each thread uses, the processor's clock, whether the computer is on battery and Windows' power
  mode; when the game is closed it adds a summary. It holds no game data, but if the game crashes it
  can carry paths of your computer: check it before sharing it.
- **To change one of the two settings only for yourself**, create the file
  `%LOCALAPPDATA%\HarvestMoonMM\GameSettings\G4AEE9.ini` with the same line; for example, to go
  back to the disc's original slowness:

  ```
  [Core]
  FastDiscSpeed = False
  ```

### Mods

A mod is a small change to the game that is loaded when it starts. The package carries two:

| Mod | What it does |
| --- | --- |
| `jamie_marriage` | Marrying Jamie no longer ends the game: you keep playing after the ceremony. Without the mod, the ceremony is followed by the epilogue and the title screen; with it, the notes scene is shown once and the game goes back to free play. |
| `player_speed` | The character walks and runs faster: by default at 100% (no change) of the original speed. It only acts in free play; in cutscenes the character moves as in the original. The animation is not changed, so at high values the feet might look as if they slide. |

**To turn the mods on and off and choose how fast the character is**, double-click `Settings.bat`.
A window opens with a check box for each mod and, for the faster character, a slider and a number box
from 25% (very slow) to 300% (three times as fast), where 100% is the speed of the original game.
**Save** saves the choice, **Save and play** saves it and opens the game with the keyboard, **Save and
open the launcher** saves it and opens the launcher, and **Defaults** goes back to every mod on and
100%. The choice applies the next time the game is started with `Play.bat` or `Play-Keyboard.bat`: a
game that is already open does not change. A black window stays open behind the settings window; that
is normal. Until you have saved something, every mod is on and the character goes at 100%.

**Fast-forward.** In the same window, **Speed of the whole game** makes everything go faster, scenes
and dialogues included, like the fast-forward of an emulator: from 100% (normal) to 400%. It is not a
mod: `Play.bat` gives it to the emulation core (`EmulationSpeed` in the `[Core]` section of
`%LOCALAPPDATA%\HarvestMoonMM\Config\Dolphin.ini`). The clock of the game runs faster too and the sound
may break up; if the computer cannot reach the speed chosen, the game goes as fast as it can. For now
it is chosen before playing; turning it on and off with a key while you play (the space bar) needs a
change in the program that runs the game and will come with a new package.

The choice is kept in `%LOCALAPPDATA%\HarvestMoonMM\settings.json`, a file that can also be edited by
hand with a text editor:

```json
{
  "mods": { "jamie_marriage": true, "player_speed": true },
  "playerSpeedPercent": 100,
  "gameSpeedPercent": 100
}
```

A mod set to `false` is taken out of your folder and the game is played without it; a mod that is not
listed is on. `playerSpeedPercent` is a whole number from 25 to 300 and `gameSpeedPercent` one from 100 to 400. A value that makes no sense is
ignored (and `Play.bat` says so) and the rest of the file is still used; if you delete the file, the
defaults come back.

`Play.bat` and `Play-Keyboard.bat` build the mods that are on, on your computer, every time they run,
with the same compiler that recompiles the game (it takes a few seconds), and put them in
`%LOCALAPPDATA%\HarvestMoonMM\Mods`. If they cannot be built, the game starts anyway and the reason is
in `Logs\play.log`.

**From the command line** (open a command prompt in the package folder):

- `Play.bat -NoMods` (or `Play-Keyboard.bat -NoMods`) plays without any mod, without looking at
  `settings.json`: it takes the package's mods out of your folder and the game is played as it is on
  the disc. To get them back, run `Play.bat` without that.
- `Play.bat -PlayerSpeed 200` (or `Play-Keyboard.bat -PlayerSpeed 200`) chooses the speed only for that
  session and wins over the one that `Settings.bat` saved: a whole number from 25 to 300. It has no
  effect with `-NoMods`, or if the `player_speed` mod is turned off in the settings.

Above 200 the character may go through thin walls: at 400 it went through the door of its house in all
six tries and fell out of the map, at 300 it did not, but the other walls of the game were not tried
([MOVEMENT.md][movement-en]).

> **Status: the Jamie mod was tried with the conditions of the wedding written by hand, not with a
> real save game** (see [the results][scripts-en]). If you try it in a game of your own, copy the folder
> `%LOCALAPPDATA%\HarvestMoonMM\GC` first: that is where your saved game is.

### Where it keeps things

Everything the package creates is in the folder `%LOCALAPPDATA%\HarvestMoonMM` (paste that into
the address bar of File Explorer): the extracted game (`games`), the recompiled module (`modules`
and `StaticRecompModules`), the mods (`Mods`, and `mods-build` where they are built), your saved
games, the settings (including your choice of mods and speed, `settings.json`), and the log of the
last preparation (`Logs\play.log`). To uninstall, delete the package folder and that folder.

### If something fails

- **Windows blocks the programs.** The package is not digitally signed. Unblock the zip before
  extracting it (step 1). If your Windows has *Smart App Control* set to **On**, it blocks every
  unsigned program and this package will not work while it stays that way.
- **The antivirus quarantines files** (it often happens with the compiler in `toolchain`). Add
  the package folder and `%LOCALAPPDATA%\HarvestMoonMM` to its exclusions and extract the zip again.
- **"The disc image could not be read".** This package only plays the USA release of the game
  (ID `G4AEE9`). An image of another region or edition does not work.
- **The preparation fails.** Open `%LOCALAPPDATA%\HarvestMoonMM\Logs\play.log`. If you share it in
  a bug report, it holds no game data. If the error talks about paths that are too long, move the
  package to a shorter folder.
- **It says "The mods could not be prepared".** The game starts anyway, without them (or with the copy
  left from the previous time). `Logs\play.log` has what CMake and the compiler said.
- **The game does not open or the picture is black.** Update the video driver and, in the
  launcher, change the **Graphics backend** (on Windows ARM64: DirectX 12, DirectX 11 or Vulkan; on
  Windows x64 also OpenGL). A new launcher starts with DirectX 12 on Windows ARM64, but a
  `config.ini` that already existed keeps its choice (for example Vulkan, with which the window
  stays black on the Snapdragon laptop where the package was tried): choose DirectX 12 once. The
  launcher opens even with no controller, and so does **Play** (you play with the keyboard); only
  netplay asks for one. Then open the game again as before.
- **I played with a controller and now I want the keyboard.** The launcher remembers the controller
  you used, and `Play-Keyboard.bat` uses it too. To go back to the keyboard, close the game and
  delete `%LOCALAPPDATA%\HarvestMoonMM\config.ini` and
  `%LOCALAPPDATA%\HarvestMoonMM\Config\GCPadNew.ini` (they are made again; the resolution you
  chose is lost, not your saved games).

### What is in the package

| Folder or file | What it is for |
| --- | --- |
| `Play.bat`, `Play-Keyboard.bat`, `play.ps1`, `game.psd1` | What you double-click (with the launcher, or with the keyboard), the script that prepares the game, and the game's data. |
| `Settings.bat`, `Settings.ps1`, `settings-file.ps1` | The window where you turn the mods on and off and choose how fast the character is (see "Mods"), and the code that it and `play.ps1` share to save and read your choice. |
| `Measure-Speed.bat`, `Measure-Speed.ps1` | Optional: they open the game with the keyboard and write down how fast it runs (see "Performance and battery"). |
| `bin\` | The launcher, the runner, DolRecomp (the recompiler) and `moderngekko-port` (what ties them together). `bin\source` holds the few source files that building the game's module needs, and the ModernGekko headers that a mod includes. |
| `mods\` | The source of the mods, and the header with the names of the game's functions that they are written with; `play.ps1` builds and installs them. |
| `toolchain\` | The portable C compiler (llvm-mingw), CMake, Ninja and Python, which build the module. Nothing is installed in Windows. |
| `licenses\`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `VERSIONS.txt` | Licenses, third-party components, and the exact versions this package was made with. |

### License and source code

Everything is free software; the details, and where the source code of each part is, are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project's code is at
<https://github.com/RodoLy/HMMMGC> under GPL-3.0-or-later.

[validation]: https://github.com/RodoLy/HMMMGC/blob/main/docs/VALIDATION.md
[performance]: https://github.com/RodoLy/HMMMGC/blob/main/docs/PERFORMANCE.md
[scripts-es]: https://github.com/RodoLy/HMMMGC/blob/main/docs/SCRIPTS.md#prueba-en-ejecución-del-mod-2026-11-04
[scripts-en]: https://github.com/RodoLy/HMMMGC/blob/main/docs/SCRIPTS.md#run-time-test-of-the-mod-2026-11-04
[movement-es]: https://github.com/RodoLy/HMMMGC/blob/main/docs/MOVEMENT.md#pruebas
[movement-en]: https://github.com/RodoLy/HMMMGC/blob/main/docs/MOVEMENT.md#tests
