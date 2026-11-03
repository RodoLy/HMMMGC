# ModernGekko integration / Integración con ModernGekko

Read from ModernGekko `a2e4e2b` (2026-09-16) and DolRecomp `02e27ac`
(2026-10-10). / Leído de esas versiones.

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
| Nombres de funciones en mods | DolRecomp con `--map` genera `DOLRECOMP_SYMBOL_<nombre>`; los mods lo usan en lugar de direcciones. Nuestro mapa sale de `tools/export_dolrecomp_map.py`. |
| Mod de Jamie | Paquete `.mgm` con `RECOMP_PATCH` o `RECOMP_HOOK` sobre la función que dispara el final (por encontrar en el intérprete `lsrun`). |
| Velocidad y fricción del personaje | `.mgm` con hooks sobre la función de movimiento del jugador. |
| Cheats y calidad de vida | `.mgm`; varios a la vez, ordenados por dependencias. |
| 4K | Resolución interna del núcleo de Dolphin. |
| Widescreen / ultrawide | Primero el ajuste de Dolphin; el HUD bien anclado y el ultrawide real, como mod. |
| Cargas rápidas | Lectura directa de la carpeta extraída; los timers de pantallas de carga, como mod. |
| Soporte para mods | Viene de serie (`Mods/`). |

### Lo que falta del lado de ModernGekko

`moderngekko-port` arma la línea de comandos de DolRecomp sin `--map`
(`tools/moderngekko_port.cpp`, variable `generate`). Para que los mods
usen nuestros nombres hace falta una opción que lo pase, por ejemplo
`moderngekko-port build <juego> --map archivo.map`. Es un cambio de pocas
líneas que conviene proponer al proyecto en vez de mantener un fork.

### Estructura propuesta para este repo

```
config/G4AEE9/symbols.txt     mapa (ya existe)
mods/jamie_marriage/          mod .mgm del final de Jamie
mods/movement/                velocidad y fricción
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
- **Where our work goes:** DolRecomp's `--map` turns our symbols into
  `DOLRECOMP_SYMBOL_<name>` constants for mods; the Jamie mod, movement
  speed/friction, cheats and QoL are `.mgm` packages; 4K and basic
  widescreen come from the Dolphin core, with proper HUD anchoring and
  ultrawide as mods; mod support is built in.
- **Missing upstream:** `moderngekko-port` builds the DolRecomp command line
  without `--map` (`tools/moderngekko_port.cpp`, variable `generate`). A
  small option such as `moderngekko-port build <game> --map file.map` should
  be proposed upstream rather than kept in a fork.
- **Proposed layout:** `mods/jamie_marriage/`, `mods/movement/`,
  `mods/cheats/`, and a default `config/G4AEE9/GCPadNew.ini`, since
  ModernGekko has no input configuration UI.
