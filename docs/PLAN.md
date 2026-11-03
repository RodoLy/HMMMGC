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
- [x] Evaluar recompiladores. Recomendación: DolRecomp + ModernGekko
      (runtime derivado de Dolphin). Ver
      [RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md).
- [x] Exportar nuestro mapa al formato `--map` de DolRecomp
      (`tools/export_dolrecomp_map.py`).
- [x] Probar Magical Melody con ModernGekko-Template: arranca, muestra los
      logos de Nintendo, Natsume y Marvelous, y la escena 3D de apertura
      (render por software, sin GPU).
- [x] Pantalla de título, menú y partida nueva con entrada de control
      (automatización de ModernGekko), hasta el saludo del alcalde en el
      intro; `fallback=0`, todo corre recompilado.
- [ ] Hito: mover al personaje y jugar a velocidad completa en una PC con
      GPU.
- [ ] Jugar con GPU en laptops Windows ARM (Snapdragon): WSL2 no puede usar
      la GPU ahí ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)).
      Ruta posible: compilar ModernGekko de forma nativa para Windows ARM64.
      Su CMake y el Dolphin que trae ya contemplan ARM64 (`_M_ARM_64`,
      FFmpeg ARM64, sin OpenGL en Windows ARM64, así que Vulkan o D3D12);
      falta comprobar que compile: el README de DolRecomp dice que
      ModernGekko no compila con MSVC por ahora, y el de ModernGekko-Template
      fijado dice que sí.
      Otra ruta, más barata de probar: `GALLIUM_DRIVER=d3d12` en WSL. Hay
      usuarios de laptops x64 a los que Mesa les caía a software y con eso
      vieron su GPU ([wslg#1394](https://github.com/microsoft/wslg/issues/1394)),
      y otros que reportan congelamientos y pantallazos azules con algunas GPU
      ([WSL#14033](https://github.com/microsoft/WSL/issues/14033)); no hay
      reportes en Snapdragon. Se prueba primero con
      `GALLIUM_DRIVER=d3d12 glxinfo -B`, que solo consulta.
- [x] Nombres de funciones para mods: `tools/export_mod_symbols.py` genera
      el mismo header que DolRecomp con `--map` (idéntico byte por byte).
      Con el backend de C el mapa no cambia el código, así que
      `moderngekko-port` no necesita cambios.
- Si se adopta ModernGekko, la fase 2 se reduce mucho: video, audio, disco y
  controles ya vienen del núcleo de Dolphin.

**Fase 2 – Runtime** (la parte pesada)
- [ ] Renderizador GX sobre OpenGL/Vulkan. Referencia: Dolphin para la
      semántica de GX; Wiicompiled para la arquitectura de un recomp.
- [ ] Audio AX → SDL/miniaudio.
- [ ] PAD → SDL gamepad y teclado.
- [ ] CARD → archivos de guardado en disco.
- [ ] DVD → lectura directa de la carpeta `orig/G4AEE9/files` (esto ya da cargas
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
- [x] Hooks de funciones para mods: `scripts/build_mods.sh` compila mods que
      enganchan funciones del juego por su nombre
      ([mods/README.md](../mods/README.md)); `boot_logger` lo prueba. Los
      *patch* (reemplazar una función) existen en la API, pero ningún mod los
      ha probado todavía dentro del juego.
- [ ] Carpeta de reemplazo de assets para mods.

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
- [x] Evaluate recompilers. Recommendation: DolRecomp + ModernGekko
      (Dolphin-derived runtime). See
      [RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md).
- [x] Export our map to DolRecomp's `--map` format
      (`tools/export_dolrecomp_map.py`).
- [x] Try Magical Melody with ModernGekko-Template: it boots, shows the
      Nintendo, Natsume and Marvelous logos, and the 3D opening scene
      (software rendering, no GPU).
- [x] Title screen, menu and a new game with controller input
      (ModernGekko's automation), up to the mayor's greeting in the intro;
      `fallback=0`, everything runs recompiled.
- [ ] Milestone: move the character and play at full speed on a PC with a
      GPU.
- [ ] Play with a GPU on Windows ARM laptops (Snapdragon): WSL2 cannot use
      the GPU there ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)).
      Possible route: build ModernGekko natively for Windows ARM64. Its
      CMake and the Dolphin it ships already account for ARM64
      (`_M_ARM_64`, ARM64 FFmpeg, no OpenGL on Windows ARM64, so Vulkan or
      D3D12); what is left is checking that it builds: DolRecomp's README
      says ModernGekko does not build with MSVC for now, and the pinned
      ModernGekko-Template's says it does.
      Another route, cheaper to try: `GALLIUM_DRIVER=d3d12` in WSL. Some
      users of x64 laptops whose Mesa fell back to software saw their GPU
      with it ([wslg#1394](https://github.com/microsoft/wslg/issues/1394)),
      and others report freezes and blue screens with some GPUs
      ([WSL#14033](https://github.com/microsoft/WSL/issues/14033)); there are
      no reports on Snapdragon. Try it first with
      `GALLIUM_DRIVER=d3d12 glxinfo -B`, which only queries.
- [x] Function names for mods: `tools/export_mod_symbols.py` writes the
      same header DolRecomp writes with `--map` (byte-identical). With the C
      backend the map does not change the code, so `moderngekko-port` needs
      no changes.
- If ModernGekko is adopted, phase 2 shrinks a lot: video, audio, disc and
  input already come from the Dolphin core.

**Phase 2 – Runtime** (the heavy part)
- [ ] GX renderer on OpenGL/Vulkan. Reference: Dolphin for GX semantics,
      Wiicompiled for recomp architecture.
- [ ] AX audio → SDL/miniaudio.
- [ ] PAD → SDL gamepad and keyboard.
- [ ] CARD → save files on disk.
- [ ] DVD → direct reads from `orig/G4AEE9/files` (this alone gives instant loads).
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
- [x] Function hooks for mods: `scripts/build_mods.sh` builds mods that hook
      the game's functions by name ([mods/README.md](../mods/README.md));
      `boot_logger` proves it. *Patches* (replacing a function) exist in the
      API, but no mod has tried one inside the game yet.
- [ ] Asset-override folder for mods.

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
