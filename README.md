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
- **Fase 1 en curso:** con DolRecomp + ModernGekko el juego ya arranca y
  llega a la escena 3D de apertura
  ([docs/RECOMPILER_EVALUATION.md](docs/RECOMPILER_EVALUATION.md)).

Ver [docs/PLAN.md](docs/PLAN.md) para el plan completo,
[docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) para el disco y
[docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md) para el mapa de funciones.

### Metas

- Ejecutable nativo, sin emulador.
- Widescreen y ultra-panorámico, resolución hasta 4K.
- Cargas instantáneas.
- Personaje más rápido y sin el deslizamiento al caminar (configurable).
- Mod de Jamie: casarse con Jamie ya no termina el juego.
- Menú de cheats y mejoras de calidad de vida.
- Soporte para mods (reemplazo de assets y hooks de código).

### Empezar

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

Antes de contribuir lee [CONTRIBUTING.md](CONTRIBUTING.md).

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
  boots and reaches the 3D opening scene
  ([docs/RECOMPILER_EVALUATION.md](docs/RECOMPILER_EVALUATION.md)).

See [docs/PLAN.md](docs/PLAN.md) for the full plan,
[docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) for the disc and
[docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md) for the function map.

### Goals

- Native executable, no emulator.
- Widescreen and ultrawide, up to 4K.
- Instant loading.
- Faster character movement without the sliding feel (configurable).
- Jamie mod: marrying Jamie no longer ends the game.
- Cheat menu and quality-of-life improvements.
- Mod support (asset replacement and code hooks).

### Getting started

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

Read [CONTRIBUTING.md](CONTRIBUTING.md) before contributing.
