# ModernGekko integration / Integración con ModernGekko

Read from the versions ModernGekko-Template pins, ModernGekko `edd460a`
(2026-09-16) and DolRecomp `1bec355` (2026-08-23), and checked against
DolRecomp `02e27ac` (2026-10-10). / Leído de las versiones que fija
ModernGekko-Template y comparado con DolRecomp `02e27ac`.

[Español](#español) · [English](#english)

---

## Español

Cómo encaja este repositorio con DolRecomp + ModernGekko, si la prueba de
arranque sale bien. Todo lo de abajo sale de leer su código, no de
suposiciones.

### Qué hace cada pieza

```
tu disco ──► moderngekko-port build ──► dolrecomp (PPC → C) ──► módulo nativo del juego
                    │                                                   │
                    └─ aplica parches de Dolphin                        ▼
                       (GameSettings/G4AEE9.ini)          moderngekko-run (núcleo de Dolphin:
                                                          video, audio, disco, controles)
                                                                        │
                                                          carga mods .mgm desde Mods/
```

- **Parches por defecto:** antes de recompilar, `moderngekko-port` escribe en
  el DOL los parches `OnFrame` que Dolphin tiene activados para el juego en
  `vendor/dolphin/Data/Sys/GameSettings/<ID>.ini`
  (`tools/moderngekko_port.cpp`, `LoadDefaultDolPatches`).
- **Mods:** `moderngekko-run` carga paquetes `<id>.mgm` (con `mod.dll`,
  `mod.so` o `mod.dylib`) desde `Mods/`. La ABI ofrece `RECOMP_PATCH`,
  `RECOMP_FORCE_PATCH`, `RECOMP_HOOK`, `RECOMP_HOOK_RETURN`, `RECOMP_EXPORT`,
  `RECOMP_IMPORT`, `RECOMP_DECLARE_EVENT` y `RECOMP_CALLBACK`, los mismos
  nombres que usa el runtime de N64Recomp. Plantilla mínima en
  `mod-template/`.

### Dónde entra lo nuestro

| Meta | Cómo se hace con este stack |
|---|---|
| Nombres de funciones en mods | `tools/export_mod_symbols.py` escribe `build/G4AEE9_symbols.h` con una macro `DOLRECOMP_SYMBOL_<nombre>` por función, el mismo header que genera DolRecomp con `--map`; los mods la usan en lugar de direcciones: `RECOMP_PATCH(DOLRECOMP_SYMBOL_OSInit, ...)`. |
| Mod de Jamie | Paquete `.mgm` con `RECOMP_PATCH` o `RECOMP_HOOK` sobre la función que dispara el final (por encontrar en el intérprete `lsrun`). |
| Velocidad del personaje | `.mgm` con un gancho en la actualización del jugador (`mods/player_speed`, ver [MOVEMENT.md](MOVEMENT.md)). |
| Cheats y calidad de vida | `.mgm`; varios a la vez, ordenados por dependencias. |
| 4K | Resolución interna del núcleo de Dolphin. |
| Widescreen / ultrawide | Primero el ajuste de Dolphin; el HUD bien anclado y el ultrawide real, como mod. |
| Cargas rápidas | Lectura directa de la carpeta extraída; los timers de pantallas de carga, como mod. |
| Soporte para mods | Viene de serie (`Mods/`). |

### Por qué no hace falta `--map` en moderngekko-port

`moderngekko-port` arma la línea de comandos de DolRecomp sin `--map`
(`tools/moderngekko_port.cpp`, variable `generate`), y no hace falta
agregarlo. Con el backend de C, el que usamos, el mapa solo produce
`generated_symbols.h`: el resto de la salida es idéntico con o sin mapa
(comparamos las dos salidas de `main.dol` archivo por archivo). Por eso ese
header lo generamos nosotros desde `symbols.txt`; sale igual byte por byte al
de DolRecomp, y renombrar una función ya no obliga a recompilar el módulo
del juego. Solo el backend LLVM usa las direcciones del mapa (como puntos de
entrada y, desde la `02e27ac`, de partición); si algún día lo usamos, ahí sí
convendría proponer `--map` al proyecto.

### Estructura propuesta para este repo

```
config/G4AEE9/symbols.txt     mapa (ya existe)
mods/jamie_marriage/          mod .mgm del final de Jamie
mods/player_speed/            velocidad del personaje
mods/cheats/                  cheats
config/G4AEE9/GCPadNew.ini    controles por defecto (ModernGekko no trae UI)
```

---

## English

How this repository plugs into DolRecomp + ModernGekko if the boot test
succeeds. Everything here comes from reading their code.

- **Pipeline:** `moderngekko-port build` applies Dolphin's enabled `OnFrame`
  patches for the game (`GameSettings/<ID>.ini`) to the DOL, runs
  `dolrecomp` (PPC → C) and builds a native game module; `moderngekko-run`
  runs it on the Dolphin-derived core (video, audio, disc, input) and loads
  `.mgm` mods from `Mods/`.
- **Mod ABI:** `RECOMP_PATCH`, `RECOMP_FORCE_PATCH`, `RECOMP_HOOK`,
  `RECOMP_HOOK_RETURN`, `RECOMP_EXPORT`, `RECOMP_IMPORT`,
  `RECOMP_DECLARE_EVENT`, `RECOMP_CALLBACK`, the same names as N64Recomp's
  runtime. Minimal package in `mod-template/`.
- **Where our work goes:** `tools/export_mod_symbols.py` turns our symbols
  into `DOLRECOMP_SYMBOL_<name>` constants for mods (`build/G4AEE9_symbols.h`,
  the same header DolRecomp writes with `--map`), so a mod says
  `RECOMP_PATCH(DOLRECOMP_SYMBOL_OSInit, ...)`; the Jamie mod, the player's
  speed (see [MOVEMENT.md](MOVEMENT.md)), cheats and QoL are `.mgm`
  packages; 4K and basic
  widescreen come from the Dolphin core, with proper HUD anchoring and
  ultrawide as mods; mod support is built in.
- **No `--map` needed in moderngekko-port:** it builds the DolRecomp command
  line without `--map` (`tools/moderngekko_port.cpp`, variable `generate`),
  and that is fine. With the C backend, the one we use, the map only adds
  `generated_symbols.h`; the rest of the output is identical with or
  without it (we diffed both outputs for `main.dol` file by file). So we
  write that header ourselves, byte-identical to DolRecomp's, and renaming a
  function no longer forces a module rebuild. Only the LLVM backend uses
  map addresses (as entry points and, from `02e27ac` on, partition points);
  if we ever switch to it, proposing `--map` upstream becomes worthwhile.
- **Proposed layout:** `mods/jamie_marriage/`, `mods/player_speed/`,
  `mods/cheats/`, and a default `config/G4AEE9/GCPadNew.ini`, since
  ModernGekko has no input configuration UI.
