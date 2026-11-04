# Harvest Moon: Magical Melody – PC recompilation

[Español](#español) · [English](#english)

---

## Español

Port nativo a PC de *Harvest Moon: Magical Melody* (GameCube) por recompilación
estática, con mejoras modernas y un mod que elimina el final forzado al casarse
con Jamie.

**Binario objetivo:** `Harvest Moon: Magical Melody (USA)`, ID `G4AEE9`, revisión 0.
`sys/main.dol` SHA-1 `5977972cd798d60ecfcf5c1f26a86dcecc36446f`.

Este repositorio no contiene datos del juego. Necesitas tu propio respaldo del
disco (ISO, GCM o RVZ).

### Estado

- **Fase 0 terminada:** análisis del disco y mapa de funciones del ejecutable
  (4 486 funciones, validado contra el índice de excepciones de CodeWarrior).
- **Fase 1 en curso:** con DolRecomp + ModernGekko el juego ya llega a la
  pantalla de título y empieza una partida nueva, todo desde código
  recompilado ([docs/RECOMPILER_EVALUATION.md](docs/RECOMPILER_EVALUATION.md)).
  Ya se pueden compilar mods que enganchan funciones del juego por su nombre
  (`scripts/build_mods.sh`; guía en [mods/README.md](mods/README.md)).

Ver [docs/PLAN.md](docs/PLAN.md) para el plan completo,
[docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) para el disco y
[docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md) para el mapa de funciones.

### Metas

- Ejecutable nativo, sin emulador.
- Widescreen y ultra-panorámico, resolución hasta 4K.
- 120 fps.
- Cargas instantáneas.
- Personaje más rápido y sin el deslizamiento al caminar (configurable).
- Mod de Jamie: casarse con Jamie ya no termina el juego.
- Menú de cheats y mejoras de calidad de vida.
- Soporte para mods (reemplazo de assets y hooks de código).

### Jugar

Para compilar y abrir el juego recompilado en Ubuntu, o en Windows dentro de
WSL2, sigue [docs/BUILDING.md](docs/BUILDING.md); un solo script hace todo.

### Empezar (herramientas de análisis)

Necesitas Python 3.11+, y Rust para instalar `nodtool`.

```sh
# 1. Herramientas
cargo install nodtool                    # lee ISO/GCM/RVZ
python3 -m pip install -r requirements-dev.txt   # capstone, pytest, ruff

# 2. Extrae tu disco a orig/G4AEE9 y verifica que sea el binario correcto
python3 tools/extract_disc.py ruta/a/tu/disco.rvz
python3 tools/check_disc.py

# 3. Regenera y comprueba el mapa de funciones
python3 tools/analyze.py
HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest

# 4. Lee cualquier función
python3 tools/disasm.py main
```

#### En Windows

- Instala el Python **x64** de [python.org](https://www.python.org/downloads/windows/),
  también si tu PC tiene procesador ARM: `capstone` no publica paquete
  precompilado (wheel) para Windows ARM64, y Python x64 corre ahí gracias a la
  emulación de Windows 11.
- Usa `py` en lugar de `python3`. En PowerShell, la variable de los tests se
  define así:

  ```powershell
  $env:HM_MM_DOL = "orig/G4AEE9/sys/main.dol"
  py -m pytest
  ```

- Si ya usas Dolphin no necesitas Rust ni `nodtool`: clic derecho en el juego →
  *Properties* → pestaña *Filesystem* → clic derecho en el disco →
  *Extract Entire Disc...*, y elige la carpeta `orig\G4AEE9`. Después
  `py tools/check_disc.py` confirma que quedó bien.

Antes de contribuir lee [CONTRIBUTING.md](CONTRIBUTING.md).

### Licencia

Este proyecto se distribuye bajo la GNU General Public License, versión 3 o
posterior (GPL-3.0-or-later); el texto completo está en [LICENSE](LICENSE). Es
la licencia de ModernGekko y es compatible con la de DolRecomp (GPLv3) y con la
de Dolphin, sobre los que se construye. Los parches de `patches/` modifican
archivos derivados de Dolphin que son GPL-2.0-or-later y se ofrecen con esos
mismos términos. El juego no se incluye ni se distribuye.

---

## English

Native PC port of *Harvest Moon: Magical Melody* (GameCube) via static
recompilation, with modern enhancements and a mod that removes the forced
ending when marrying Jamie.

**Target binary:** `Harvest Moon: Magical Melody (USA)`, ID `G4AEE9`, revision 0.
`sys/main.dol` SHA-1 `5977972cd798d60ecfcf5c1f26a86dcecc36446f`.

This repository contains no game data. You need your own backup of the disc
(ISO, GCM or RVZ).

### Status

- **Phase 0 done:** disc analysis and function map of the executable (4,486
  functions, validated against CodeWarrior's exception index).
- **Phase 1 in progress:** with DolRecomp + ModernGekko the game already
  reaches the title screen and starts a new game, all from recompiled code
  ([docs/RECOMPILER_EVALUATION.md](docs/RECOMPILER_EVALUATION.md)). Mods that
  attach to the game's functions by name can now be built
  (`scripts/build_mods.sh`; guide in [mods/README.md](mods/README.md)).

See [docs/PLAN.md](docs/PLAN.md) for the full plan,
[docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) for the disc and
[docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md) for the function map.

### Goals

- Native executable, no emulator.
- Widescreen and ultrawide, up to 4K.
- 120 fps.
- Instant loading.
- Faster character movement without the sliding feel (configurable).
- Jamie mod: marrying Jamie no longer ends the game.
- Cheat menu and quality-of-life improvements.
- Mod support (asset replacement and code hooks).

### Playing

To build and start the recompiled game on Ubuntu, or on Windows inside WSL2,
follow [docs/BUILDING.md](docs/BUILDING.md); a single script does it all.

### Getting started (analysis tools)

You need Python 3.11+, and Rust to install `nodtool`.

```sh
# 1. Tools
cargo install nodtool                    # reads ISO/GCM/RVZ
python3 -m pip install -r requirements-dev.txt   # capstone, pytest, ruff

# 2. Extract your disc to orig/G4AEE9 and check it is the right binary
python3 tools/extract_disc.py path/to/your/disc.rvz
python3 tools/check_disc.py

# 3. Regenerate and check the function map
python3 tools/analyze.py
HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest

# 4. Read any function
python3 tools/disasm.py main
```

#### On Windows

- Install the **x64** Python from [python.org](https://www.python.org/downloads/windows/),
  even if your PC has an ARM processor: `capstone` ships no prebuilt package
  (wheel) for Windows ARM64, and x64 Python runs there through Windows 11's
  emulation.
- Use `py` instead of `python3`. In PowerShell, set the tests' variable like
  this:

  ```powershell
  $env:HM_MM_DOL = "orig/G4AEE9/sys/main.dol"
  py -m pytest
  ```

- If you already use Dolphin you need neither Rust nor `nodtool`: right-click
  the game → *Properties* → *Filesystem* tab → right-click the disc →
  *Extract Entire Disc...*, and pick the `orig\G4AEE9` folder. Then
  `py tools/check_disc.py` confirms it came out right.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before contributing.

### License

This project is distributed under the GNU General Public License, version 3 or
later (GPL-3.0-or-later); the full text is in [LICENSE](LICENSE). It is the
license of ModernGekko and is compatible with those of DolRecomp (GPLv3) and
Dolphin, which the project builds on. The patches in `patches/` modify
Dolphin-derived files that are GPL-2.0-or-later and are offered under those
same terms. The game is neither included nor distributed.
