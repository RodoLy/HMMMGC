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

Fase 0 (análisis del disco) terminada. Ver [docs/PLAN.md](docs/PLAN.md) para el
plan completo y [docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) para lo que se
sabe del binario y los archivos.

### Metas

- Ejecutable nativo, sin emulador.
- Widescreen y ultra-panorámico, resolución hasta 4K.
- Cargas instantáneas.
- Personaje más rápido y sin el deslizamiento al caminar (configurable).
- Mod de Jamie: casarse con Jamie ya no termina el juego.
- Menú de cheats y mejoras de calidad de vida.
- Soporte para mods (reemplazo de assets y hooks de código).

### Empezar

```sh
# 1. Instala nodtool (lee ISO/GCM/RVZ)
cargo install nodtool

# 2. Extrae tu disco
python3 tools/extract_disc.py ruta/a/tu/disco.rvz

# 3. Verifica que sea el binario correcto
python3 tools/check_disc.py disc/
```

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

Phase 0 (disc analysis) done. See [docs/PLAN.md](docs/PLAN.md) for the full
plan and [docs/DISC_ANALYSIS.md](docs/DISC_ANALYSIS.md) for what is known about
the binary and the files.

### Goals

- Native executable, no emulator.
- Widescreen and ultrawide, up to 4K.
- Instant loading.
- Faster character movement without the sliding feel (configurable).
- Jamie mod: marrying Jamie no longer ends the game.
- Cheat menu and quality-of-life improvements.
- Mod support (asset replacement and code hooks).

### Getting started

```sh
# 1. Install nodtool (reads ISO/GCM/RVZ)
cargo install nodtool

# 2. Extract your disc
python3 tools/extract_disc.py path/to/your/disc.rvz

# 3. Verify it is the right binary
python3 tools/check_disc.py disc/
```
