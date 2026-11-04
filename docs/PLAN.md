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
- [x] Validación diferencial: el verificador lockstep de ModernGekko compara
      cada bloque recompilado con el intérprete de Dolphin mientras se juega.
      11 286 bloques (arranque, intro, título, partida nueva, casa, granja y
      camino) sin una sola divergencia con el verificador parcheado, y los
      fallos inyectados a propósito sí se detectan. Herramientas: `tools/lockstep_report.py` y
      `tools/lockstep_coverage.py`. Resultados y límites en
      [VALIDATION.md](VALIDATION.md).
- [x] Prueba por instrucción: cada una de las 157 formas de instrucción del
      juego que se pueden ejecutar, con palabras copiadas del propio juego y
      operandos aleatorios, comparada con el intérprete de Dolphin. 10
      semillas, 26 450 casos, 0 diferencias; 269 sin comparar (instrucciones de
      caché y SPRG/PVR, que el módulo ejecuta con el intérprete).
      `tools/run_isa_conformance.py`; detalles en [VALIDATION.md](VALIDATION.md).
- [x] Prueba por instrucción sin el juego: palabras inventadas de 316 formas (casi
      todo el procesador, no solo lo que usa el juego) en un disco inventado, así
      que cualquier máquina con ModernGekko puede repetirla. 6 semillas,
      41 045 casos, 0 diferencias; 485 sin comparar. Las semillas 1 y 2
      también corrieron en ARM64 (Linux, en GitHub) con las mismas cifras.
      Encontró un error del intérprete de Dolphin (el acarreo de `addme` y
      `subfme`) y una laguna de DolRecomp (`mtfsb1` y el bit FX); el juego no usa
      ninguna de las dos instrucciones. `run_isa_conformance.py --synthetic`;
      detalles en [VALIDATION.md](VALIDATION.md).
- [ ] Ampliar la validación con partida (tiendas, pueblo, minijuegos,
      festivales, un día nuevo y el fin de un evento), repetir en ARM64 las dos
      comprobaciones que usan el juego, y las tres en Windows ARM64 con MSVC.
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
- [ ] 120 fps. El juego calcula todo por cuadro, a 60 o a 30 por segundo (falta
      medir cuántas retrazas de video espera por cuadro), así que subir la
      cadencia no es un ajuste: hay dos caminos. (a) Interpolar en el
      renderizador: se dibuja entre el estado anterior y el actual (posición
      de los objetos y de la cámara) sin tocar la lógica ni la velocidad del
      juego. Es la idea que permite tasas de cuadros altas en otros recomps
      ([Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), con
      RT64) y necesita el renderizador propio de la fase 2. (b) Correr la
      lógica a 120 Hz: obliga a escalar todo lo que el juego cuenta por cuadro
      (movimiento, temporizadores, animaciones, audio) y es fácil romper algo.
      Primer paso, cuando el juego corra bien: medir la cadencia real y
      separar lo que cuenta cuadros de lo que lee el reloj.
- [ ] Saltar timers de pantallas de carga.
- [ ] Velocidad, aceleración y fricción del jugador en archivo de config.
- [ ] Selector de idioma (el disco ya trae ES/FR/DE/IT).
- [ ] Mod de Jamie integrado (ver pista B).

**Fase 4 – Cheats y mods**
- [ ] Menú in-game: dinero, stamina, hora, notas, afecto.
- [x] Hooks de funciones para mods: `scripts/build_mods.sh` compila mods que
      enganchan funciones del juego por su nombre
      ([mods/README.md](../mods/README.md)); `boot_logger` lo prueba y
      `state_logger` muestra los estados del juego (sirve para saber cuándo
      corre el gancho de Jamie). Los *patch* (reemplazar una función) existen
      en la API, pero ningún mod los ha probado todavía dentro del juego.
- [ ] Carpeta de reemplazo de assets para mods.

**Fase 5 – Release**
- [ ] Instalador que toma la ISO/RVZ del usuario, verifica hash y extrae.
- [ ] Builds Windows/Linux. Sin assets del juego en el repo ni en el release.

### Pista B – Mods en Dolphin (después de la recomp)

- [x] Documentar el formato `.lsb`: [SCRIPTS.md](SCRIPTS.md) y `tools/lsb.py`
      (desensamblador y verificador; los 291 scripts del disco se leen sin
      errores).
- [x] Localizar la llamada de "fin de juego": no está en los scripts sino en
      código nativo; la cadena completa está en
      [SCRIPTS.md](SCRIPTS.md#qué-termina-el-juego).
- [ ] Mod de Jamie v1: un hook en `0x80166888` que borra la bandera 230 cuando
      termina la ceremonia (diseño en [SCRIPTS.md](SCRIPTS.md); sin probar
      todavía dentro del juego).
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
- [x] Differential validation: ModernGekko's lockstep verifier compares every
      recompiled block with Dolphin's interpreter while the game plays.
      11,286 blocks (boot, intro, title, new game, house, farm and road) with
      not one divergence under the patched verifier, and faults injected on
      purpose are detected. Tools:
      `tools/lockstep_report.py` and `tools/lockstep_coverage.py`. Results and
      limits in [VALIDATION.md](VALIDATION.md).
- [x] Instruction-level test: each of the game's 157 runnable instruction
      forms, with words copied from the game itself and random operands,
      compared with Dolphin's interpreter. 10 seeds, 26,450 cases, 0
      differences; 269 not compared (cache instructions and SPRG/PVR, which
      the module runs through the interpreter).
      `tools/run_isa_conformance.py`; details in [VALIDATION.md](VALIDATION.md).
- [x] Instruction-level test without the game: made-up words of 316 forms (almost
      the whole processor, not only what the game uses) on a made-up disc, so any
      machine with ModernGekko can repeat it. 6 seeds,
      41,045 cases, 0 differences; 485 not compared. Seeds 1 and 2 also
      ran on ARM64 (Linux, on GitHub) with the same figures. It found a bug in
      Dolphin's interpreter (the carry of `addme` and `subfme`) and a gap in
      DolRecomp (`mtfsb1` and the FX bit); the game uses neither instruction.
      `run_isa_conformance.py --synthetic`; details in
      [VALIDATION.md](VALIDATION.md).
- [ ] Widen the validation with play (shops, village, minigames, festivals, a
      new day and the end of an event), repeat the two checks that use the game
      on ARM64, and all three on Windows ARM64 with MSVC.
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
- [ ] 120 fps. The game computes everything per frame, at 60 or 30 a second
      (still to measure: how many video retraces it waits for per frame), so
      raising the rate is not a setting: there are two routes. (a) Interpolate
      in the renderer: draw between the previous and the current state
      (object and camera positions) without touching the game's logic or
      speed. It is the idea behind high frame rates in other recomps
      ([Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), with
      RT64) and needs the own renderer of phase 2. (b) Run the logic at
      120 Hz: it forces scaling everything the game counts per frame
      (movement, timers, animations, audio) and it is easy to break
      something. First step, once the game runs well: measure the real
      cadence and tell what counts frames from what reads the clock.
- [ ] Skip loading-screen timers.
- [ ] Player speed, acceleration and friction in a config file.
- [ ] Language selector (disc already ships ES/FR/DE/IT).
- [ ] Jamie mod integrated (see track B).

**Phase 4 – Cheats and mods**
- [ ] In-game menu: money, stamina, time, notes, affection.
- [x] Function hooks for mods: `scripts/build_mods.sh` builds mods that hook
      the game's functions by name ([mods/README.md](../mods/README.md));
      `boot_logger` proves it and `state_logger` shows the game's states (it
      tells when the Jamie hook will run). *Patches* (replacing a function)
      exist in the API, but no mod has tried one inside the game yet.
- [ ] Asset-override folder for mods.

**Phase 5 – Release**
- [ ] Installer that takes the user's ISO/RVZ, verifies the hash and extracts.
- [ ] Windows/Linux builds. No game assets in the repo or the release.

### Track B – Dolphin mods (after the recomp)

- [x] Document the `.lsb` format: [SCRIPTS.md](SCRIPTS.md) and `tools/lsb.py`
      (disassembler and checker; all 291 scripts on the disc parse without
      problems).
- [x] Locate the "end game" call: it is not in the scripts but in native code;
      the whole chain is in [SCRIPTS.md](SCRIPTS.md#what-ends-the-game).
- [ ] Jamie mod v1: a hook on `0x80166888` that clears flag 230 once the
      ceremony is over (design in [SCRIPTS.md](SCRIPTS.md); not tried inside
      the game yet).
- [ ] Widescreen and speed as Gecko codes (find the constants).
- [ ] Jamie mod v2 (content): spouse routine, dialogue, child events.

### Conventions

See [CONTRIBUTING.md](../CONTRIBUTING.md): self-explaining names, comments
that say why, evidence for every name, one English commit per change,
bilingual docs and never any game data.
