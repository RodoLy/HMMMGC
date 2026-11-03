# Building and playing / Compilar y jugar

[Español](#español) · [English](#english)

---

## Español

`scripts/build_and_run_linux.sh` hace todo el camino: compila ModernGekko,
recompila el juego desde tu disco y lo abre. Corre en Ubuntu o Debian, y en
Windows dentro de WSL2: el README más reciente de DolRecomp dice que
ModernGekko todavía no compila con Visual Studio, y la ruta de Linux es la
que está probada con este juego.

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
primera vez tarda: descarga cerca de 1 GB, compila ModernGekko (15 a 40
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
defecto de Dolphin:

| GameCube | Teclado |
|---|---|
| Stick | Flechas (Shift para caminar) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| Cruceta | T, F, G, H |
| C-stick | I, J, K, L |

Los menús del juego se mueven con el stick, no con la cruceta. Un control
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

- `resolution=3840x2160` para 4K interno (por defecto `1920x1080`). Acepta
  640x528, 1280x720, 1920x1080, 2560x1440, 3840x2160, 5120x2880 y 7680x4320.
- `fullscreen=true` para pantalla completa.
- El backend gráfico lo elige el script; para forzarlo:
  `GRAPHICS_BACKEND=Vulkan bash .../build_and_run_linux.sh`.

### Si algo falla

- **Va muy lento:** el script avisó "no GPU acceleration found". En WSL,
  actualiza Windows, el driver de la GPU y WSL (`wsl --update`).
- **"WSL has no window support yet":** corre `wsl --update` y vuelve a abrir
  Ubuntu.
- **`$'\r': command not found`:** el script tiene saltos de línea de
  Windows. El repositorio trae `.gitattributes` para evitarlo; si pasa,
  `sed -i 's/\r$//' scripts/build_and_run_linux.sh` lo arregla.
- **"is not the G4AEE9 revision 0 disc":** borra
  `~/ModernGekko-Template/extracted/Harvest-Moon-Magical-Melody` y vuelve a
  correr el script con la imagen correcta.

---

## English

`scripts/build_and_run_linux.sh` does the whole trip: it builds
ModernGekko, recompiles the game from your disc and starts it. It runs on
Ubuntu or Debian, and on Windows inside WSL2: DolRecomp's latest README says
ModernGekko does not build with Visual Studio yet, and the Linux path is the
one tested with this game.

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
a while: it downloads about 1 GB, builds ModernGekko (15 to 40 minutes) and
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
default mapping:

| GameCube | Keyboard |
|---|---|
| Stick | Arrow keys (Shift to walk) |
| A / B | X / Z |
| X / Y | C / S |
| Z | D |
| Start | Enter |
| L / R | Q / W |
| D-pad | T, F, G, H |
| C-stick | I, J, K, L |

The game's menus move with the stick, not the D-pad. A USB or Bluetooth
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

- `resolution=3840x2160` for 4K internal resolution (default `1920x1080`).
  Accepts 640x528, 1280x720, 1920x1080, 2560x1440, 3840x2160, 5120x2880 and
  7680x4320.
- `fullscreen=true` for full screen.
- The script picks the graphics backend; to force one:
  `GRAPHICS_BACKEND=Vulkan bash .../build_and_run_linux.sh`.

### If something goes wrong

- **It is very slow:** the script said "no GPU acceleration found". In WSL,
  update Windows, the GPU driver and WSL (`wsl --update`).
- **"WSL has no window support yet":** run `wsl --update` and reopen Ubuntu.
- **`$'\r': command not found`:** the script has Windows line endings. The
  repository's `.gitattributes` prevents that; if it happens,
  `sed -i 's/\r$//' scripts/build_and_run_linux.sh` fixes it.
- **"is not the G4AEE9 revision 0 disc":** delete
  `~/ModernGekko-Template/extracted/Harvest-Moon-Magical-Melody` and run the
  script again with the right image.
