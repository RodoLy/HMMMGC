# Project plan / Plan del proyecto

[Español](#español) · [English](#english)

---

## Español

Prioridad: **la recompilación va primero** (pista A). La pista B (mods sobre
los scripts del disco) queda para después; todo lo que la pista A descubre
le sirve.

### Pista A – Recompilación (port nativo)

**Fase 0 – Análisis del disco y mapa de funciones** ✅
- [x] Extraer disco, identificar binario objetivo y registrar hashes.
- [x] Layout del DOL, versión del SDK, archivos fuente, sistema de archivos.
- [x] Localizar el script de la boda con Jamie.
- [x] Mapa de funciones (`config/G4AEE9/symbols.txt`): 4 486 funciones,
      273 tablas de `switch`, validado contra `extabindex` (2 597/2 597).
      Herramientas propias en vez de dtk; ver [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Nombres con evidencia: arranque de CodeWarrior y los 13 `Init` del SDK.
- [ ] Identificar el resto del SDK por firma (fronteras de lo que el runtime
      reemplaza en la fase 2).

**Fase 1 – Recompilación de CPU**
- [ ] Probar DolRecomp y NWiiRecomp sobre `main.dol`; quedarse con el que
      genere C compilable.
- [ ] Stubs vacíos con log para OS, PAD, DVD, CARD, AX/DSP, GX, VI, AI, AR.
- [ ] Hito: el juego arranca y llega a la pantalla de título (sin gráficos).

**Fase 2 – Runtime** (la parte pesada)
- [ ] Renderizador GX sobre OpenGL/Vulkan. Referencia: Dolphin para la
      semántica de GX; Wiicompiled para la arquitectura de un recomp.
- [ ] Audio AX → SDL/miniaudio.
- [ ] PAD → SDL gamepad y teclado.
- [ ] CARD → archivos de guardado en disco.
- [ ] DVD → lectura directa de la carpeta `disc/files` (esto ya da cargas
      instantáneas).
- [ ] Hito 1.0: jugable de principio a fin a 480p 4:3.

**Fase 3 – Mejoras**
- [ ] Resolución interna arbitraria (4K).
- [ ] Widescreen / ultrawide: matriz de proyección, culling, HUD anclado.
- [ ] Saltar timers de pantallas de carga.
- [ ] Velocidad, aceleración y fricción del jugador en archivo de config.
- [ ] Selector de idioma (el disco ya trae ES/FR/DE/IT).
- [ ] Mod de Jamie integrado (ver pista B).

**Fase 4 – Cheats y mods**
- [ ] Menú in-game: dinero, stamina, hora, notas, afecto.
- [ ] Sistema de mods: carpeta de reemplazo de assets + hooks de funciones.
      Diseñar los hooks desde la fase 1.

**Fase 5 – Release**
- [ ] Instalador que toma la ISO/RVZ del usuario, verifica hash y extrae.
- [ ] Builds Windows/Linux. Sin assets del juego en el repo ni en el release.

### Pista B – Mods en Dolphin (después de la recomp)

- [ ] Documentar el formato `.lsb` (desensamblar `lsrun.cpp` / `lsrun_bltin.cpp`).
- [ ] Localizar la llamada de "fin de juego" en `Eve_rm01.lsb` o en código nativo.
- [ ] Mod de Jamie v1: parche al script o código Gecko que salta el final.
- [ ] Widescreen y velocidad como códigos Gecko (localizar constantes).
- [ ] Mod de Jamie v2 (contenido): rutina de cónyuge, diálogos, eventos de hijo.

### Convenciones

Ver [CONTRIBUTING.md](../CONTRIBUTING.md): nombres que se explican solos,
comentarios que dicen el porqué, evidencia para cada nombre, un commit por
cambio en inglés, documentación bilingüe y nunca datos del juego.

---

## English

Priority: **the recompilation comes first** (track A). Track B (mods on the
disc's scripts) comes later; everything track A discovers feeds into it.

### Track A – Recompilation (native port)

**Phase 0 – Disc analysis and function map** ✅
- [x] Extract disc, identify target binary, record hashes.
- [x] DOL layout, SDK version, source file names, filesystem.
- [x] Locate the Jamie wedding script.
- [x] Function map (`config/G4AEE9/symbols.txt`): 4,486 functions,
      273 `switch` tables, validated against `extabindex` (2,597/2,597).
      In-repo tools instead of dtk; see [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Names with evidence: CodeWarrior startup and the 13 SDK `Init`s.
- [ ] Identify the rest of the SDK by signature (the boundary of what the
      runtime replaces in phase 2).

**Phase 1 – CPU recompilation**
- [ ] Try DolRecomp and NWiiRecomp on `main.dol`; keep whichever yields
      compilable C.
- [ ] Logging stubs for OS, PAD, DVD, CARD, AX/DSP, GX, VI, AI, AR.
- [ ] Milestone: game boots to the title screen (no graphics yet).

**Phase 2 – Runtime** (the heavy part)
- [ ] GX renderer on OpenGL/Vulkan. Reference: Dolphin for GX semantics,
      Wiicompiled for recomp architecture.
- [ ] AX audio → SDL/miniaudio.
- [ ] PAD → SDL gamepad and keyboard.
- [ ] CARD → save files on disk.
- [ ] DVD → direct reads from `disc/files` (this alone gives instant loads).
- [ ] Milestone 1.0: playable start to finish at 480p 4:3.

**Phase 3 – Enhancements**
- [ ] Arbitrary internal resolution (4K).
- [ ] Widescreen / ultrawide: projection matrix, culling, anchored HUD.
- [ ] Skip loading-screen timers.
- [ ] Player speed, acceleration and friction in a config file.
- [ ] Language selector (disc already ships ES/FR/DE/IT).
- [ ] Jamie mod integrated (see track B).

**Phase 4 – Cheats and mods**
- [ ] In-game menu: money, stamina, time, notes, affection.
- [ ] Mod system: asset-override folder + function hooks. Design the hooks
      from phase 1 onward.

**Phase 5 – Release**
- [ ] Installer that takes the user's ISO/RVZ, verifies the hash and extracts.
- [ ] Windows/Linux builds. No game assets in the repo or the release.

### Track B – Dolphin mods (after the recomp)

- [ ] Document the `.lsb` format (disassemble `lsrun.cpp` / `lsrun_bltin.cpp`).
- [ ] Locate the "end game" call in `Eve_rm01.lsb` or in native code.
- [ ] Jamie mod v1: script patch or Gecko code that skips the ending.
- [ ] Widescreen and speed as Gecko codes (find the constants).
- [ ] Jamie mod v2 (content): spouse routine, dialogue, child events.

### Conventions

See [CONTRIBUTING.md](../CONTRIBUTING.md): self-explaining names, comments
that say why, evidence for every name, one English commit per change,
bilingual docs and never any game data.
