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
> de instrucciones contra el intérprete de Dolphin (ver [VALIDATION.md][validation]), pero este
> paquete todavía no se ha probado con el juego real en una computadora de verdad. Si algo
> falla, el registro `Logs\play.log` (ver más abajo) ayuda a saber por qué.

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
5. Se abre el launcher. Conecta antes tu control (ver «Controles») y pulsa **Play**. Si no tienes
   control, usa `Play-Keyboard.bat` en lugar de `Play.bat`: abre el juego directamente y se juega
   con el teclado.

Las veces siguientes basta con hacer doble clic en `Play.bat` (o en `Play-Keyboard.bat`).

### Controles

**Con control.** `Play.bat` abre el launcher, que usa un control compatible con SDL (Xbox,
PlayStation, Switch Pro, 8BitDo, …) y lo asigna a un control de GameCube. El launcher no deja
pulsar **Play** mientras no haya un control conectado («Connect an SDL-compatible controller
before playing»).

**Con teclado.** `Play-Keyboard.bat` abre el juego sin el launcher, con la asignación de
teclado de Dolphin (el juego corre sobre Dolphin). Todavía no se puede cambiar desde el paquete:

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

### Dónde guarda las cosas

Todo lo que crea el paquete está en la carpeta `%LOCALAPPDATA%\HarvestMoonMM` (pégala en la barra
de direcciones del Explorador de archivos): el juego extraído (`games`), el módulo recompilado
(`modules` y `StaticRecompModules`), tus partidas guardadas, la configuración y el registro de la
última preparación (`Logs\play.log`). Para desinstalar, borra la carpeta del paquete y esa carpeta.

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
- **El juego no abre o la imagen sale negra.** Actualiza el controlador de video y, en el
  launcher, cambia el **Graphics backend** (Vulkan u OpenGL). El launcher abre aunque no haya
  control; solo **Play** lo pide. Después vuelve a abrir el juego como antes.
- **Jugué con un control y ahora quiero el teclado.** El launcher recuerda el control que usaste y
  `Play-Keyboard.bat` también lo usa. Para volver al teclado, cierra el juego y borra
  `%LOCALAPPDATA%\HarvestMoonMM\config.ini` y `%LOCALAPPDATA%\HarvestMoonMM\Config\GCPadNew.ini`
  (se crean otra vez; se pierde la resolución elegida, no tus partidas guardadas).

### Contenido del paquete

| Carpeta o archivo | Para qué sirve |
| --- | --- |
| `Play.bat`, `Play-Keyboard.bat`, `play.ps1`, `game.psd1` | Lo que haces doble clic (con el launcher, o con el teclado), el script que prepara el juego y los datos del juego. |
| `bin\` | El launcher, el runner, DolRecomp (el recompilador) y `moderngekko-port` (lo que los une). `bin\source` trae los pocos archivos de código que hacen falta para construir el módulo del juego. |
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
> against Dolphin's interpreter there (see [VALIDATION.md][validation]), but this package has
> not been tried with the real game on a real computer yet. If something fails, the log
> `Logs\play.log` (see below) helps to find out why.

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
5. The launcher opens. Connect your controller first (see "Controls") and press **Play**. If you
   have no controller, use `Play-Keyboard.bat` instead of `Play.bat`: it opens the game directly
   and you play with the keyboard.

The next times, just double-click `Play.bat` (or `Play-Keyboard.bat`).

### Controls

**With a controller.** `Play.bat` opens the launcher, which uses an SDL-compatible controller
(Xbox, PlayStation, Switch Pro, 8BitDo, …) and maps it to a GameCube controller. The launcher does
not let you press **Play** while no controller is connected ("Connect an SDL-compatible
controller before playing").

**With the keyboard.** `Play-Keyboard.bat` opens the game without the launcher, with Dolphin's
default keyboard layout (the game runs on Dolphin). It cannot be changed from the package yet:

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

### Where it keeps things

Everything the package creates is in the folder `%LOCALAPPDATA%\HarvestMoonMM` (paste that into
the address bar of File Explorer): the extracted game (`games`), the recompiled module (`modules`
and `StaticRecompModules`), your saved games, the settings, and the log of the last preparation
(`Logs\play.log`). To uninstall, delete the package folder and that folder.

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
- **The game does not open or the picture is black.** Update the video driver and, in the
  launcher, change the **Graphics backend** (Vulkan or OpenGL). The launcher opens even with no
  controller; only **Play** asks for one. Then open the game again as before.
- **I played with a controller and now I want the keyboard.** The launcher remembers the controller
  you used, and `Play-Keyboard.bat` uses it too. To go back to the keyboard, close the game and
  delete `%LOCALAPPDATA%\HarvestMoonMM\config.ini` and
  `%LOCALAPPDATA%\HarvestMoonMM\Config\GCPadNew.ini` (they are made again; the resolution you
  chose is lost, not your saved games).

### What is in the package

| Folder or file | What it is for |
| --- | --- |
| `Play.bat`, `Play-Keyboard.bat`, `play.ps1`, `game.psd1` | What you double-click (with the launcher, or with the keyboard), the script that prepares the game, and the game's data. |
| `bin\` | The launcher, the runner, DolRecomp (the recompiler) and `moderngekko-port` (what ties them together). `bin\source` holds the few source files that building the game's module needs. |
| `toolchain\` | The portable C compiler (llvm-mingw), CMake, Ninja and Python, which build the module. Nothing is installed in Windows. |
| `licenses\`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `VERSIONS.txt` | Licenses, third-party components, and the exact versions this package was made with. |

### License and source code

Everything is free software; the details, and where the source code of each part is, are in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). This project's code is at
<https://github.com/RodoLy/HMMMGC> under GPL-3.0-or-later.

[validation]: https://github.com/RodoLy/HMMMGC/blob/main/docs/VALIDATION.md
