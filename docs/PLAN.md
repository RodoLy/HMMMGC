# Project plan / Plan del proyecto

[Español](#español) · [English](#english)

---

## Español

Prioridad: **la recompilación va primero** (pista A). La pista B (mods del
juego) empieza ya con el mod de Jamie, porque funciona sobre el juego
recompilado; todo lo que la pista A descubre le sirve.

**Meta: un port totalmente nativo, sin código de Dolphin.** Hoy es un híbrido:
el código del juego ya corre recompilado como código nativo en Windows ARM64,
pero el hardware que lo rodea (video, audio, disco, controles, tarjeta de
memoria, temporizadores) sigue siendo el núcleo de Dolphin que trae
ModernGekko.

**Decisión (2026-11-04): el runtime final será propio y se apoyará en
[aurora](https://github.com/encounter/aurora)** para GX, controles, disco y
tarjeta de memoria; el audio y el sistema operativo los escribimos nosotros.
No se escribe un renderizador de GX desde cero ni se espera a que DolRecomp
traiga su propio runtime. ModernGekko queda como la base que ya funciona y
como referencia para comparar mientras se reemplaza pieza por pieza. Por qué
aurora, y qué se sabe y qué no, está en
[RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md). El orden:

1. Mod de Jamie v1: es pequeño y prueba el mecanismo de ganchos y parches que
   usarán las piezas nuevas.
2. Identificar el SDK por firma (fase 0): sin saber qué funciones son de GX,
   OS, DVD, CARD, PAD y AX no se puede reemplazar nada.
3. Experimento de viabilidad con aurora: llevar las llamadas de GX del juego a
   aurora y dibujar la pantalla de título. Es el mayor riesgo del camino. A
   favor: solo 85 funciones de `main.dol` escriben en la tubería de GX (la
   librería del SDK y ocho funciones de vértices; `tools/find_gx_pipe_writers.py`),
   así que GX se puede reemplazar por funciones, y las listas de visualización
   pasan por un solo embudo del juego (`tools/find_callers.py`). Falta saber si
   aurora sabe reproducir listas ya armadas (si no, se interpretan en ese
   embudo) y si corre sobre DirectX 12 en Windows ARM64. Si el experimento
   falla, se decide de nuevo con lo aprendido.
4. DVD, CARD y PAD nativos: son los más sencillos, y dan cargas instantáneas,
   guardados en archivos y controles con SDL.
5. Audio (AX) y sistema operativo (hilos, alarmas, interrupciones, retrazos).
6. Quitar el núcleo de Dolphin del runtime.

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
      reemplaza en la fase 2; es el paso 2 del orden de arriba).

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
      también corrieron en GitHub, con las mismas cifras, en ARM64: Linux con
      GCC, y Windows con MSVC y con el compilador portátil llvm-mingw.
      Encontró un error del intérprete de Dolphin (el acarreo de `addme` y
      `subfme`) y una laguna de DolRecomp (`mtfsb1` y el bit FX); el juego no usa
      ninguna de las dos instrucciones. `run_isa_conformance.py --synthetic`;
      detalles en [VALIDATION.md](VALIDATION.md).
- [ ] Ampliar la validación con partida (tiendas, pueblo, minijuegos,
      festivales, un día nuevo y el fin de un evento) y repetir en ARM64 (Linux y
      Windows) las dos comprobaciones que usan el juego; la que no lo usa ya
      corrió en las dos con las mismas cifras.
- [x] Hito: mover al personaje y jugar a velocidad completa en una PC con
      GPU. Cumplido en una laptop con Snapdragon (Windows ARM64) con DirectX 12:
      de 0.99 a 1.06 de velocidad con el cargador conectado y los ajustes de
      [PERFORMANCE.md](PERFORMANCE.md); en batería, la ciudad baja a ~0.8.
- [ ] Jugar con GPU en laptops Windows ARM (Snapdragon): WSL2 no puede usar
      la GPU ahí ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)).
      Ruta: ModernGekko nativo para Windows ARM64, sin WSL. Ya compila ahí con
      MSVC, con los parches de `patches/`. Con el compilador portátil
      llvm-mingw recompila un juego inventado y pasa la prueba por
      instrucción. Un workflow de GitHub arma el paquete (`packaging/windows`:
      un zip con `Play.bat`, donde el jugador elige su disco y el juego se
      recompila en su computadora; `Play-Keyboard.bat` para jugar con teclado)
      y lo prueba como lo haría un jugador, con un disco inventado. Ya se
      probó con el juego real en una laptop con Snapdragon, con la GPU por
      DirectX 12 (el valor por defecto del paquete en ARM64): corre a velocidad
      completa con el cargador conectado ([PERFORMANCE.md](PERFORMANCE.md)).
      Falta la batería y confirmar el control y el teclado en Windows.
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
- Con ModernGekko el juego ya es jugable sin la fase 2, porque video, audio,
  disco y controles vienen del núcleo de Dolphin; la fase 2 es lo que falta
  para la meta de un port nativo.

**Fase 2 – Runtime propio** (la parte pesada: reemplaza el hardware de Dolphin
pieza por pieza, en el orden de arriba)
- [ ] Identificar el SDK por firma (pendiente de la fase 0).
- [ ] Experimento de viabilidad: las llamadas de GX del juego dibujan la
      pantalla de título con aurora.
- [ ] GX con aurora (WebGPU con Dawn: D3D12, Vulkan y Metal). Referencia:
      Dolphin para la semántica de GX; Wiicompiled (Mario Kart Wii) para la
      arquitectura de un recomp.
- [ ] DVD → lectura directa de la carpeta `orig/G4AEE9/files` (esto ya da cargas
      instantáneas).
- [ ] CARD → archivos de guardado en disco (aurora lee los `.gci` y `.raw` de
      Dolphin).
- [ ] PAD → SDL gamepad y teclado.
- [ ] Audio AX → SDL/miniaudio.
- [ ] Sistema operativo: hilos, alarmas, interrupciones y retrazos de video sin
      el núcleo de Dolphin.
- [ ] Quitar el núcleo de Dolphin del runtime; mientras dure la migración,
      comparar contra él (verificador lockstep, capturas de pantalla).
- [ ] Hito 1.0: jugable de principio a fin a 480p 4:3 sin código de Dolphin.

**Fase 3 – Mejoras**
- [ ] Resolución interna arbitraria (4K). Con ModernGekko es el ajuste de
      resolución interna de Dolphin (sin probar todavía en el paquete); con
      aurora, su escalado de resolución.
- [ ] Widescreen / ultrawide: matriz de proyección, culling, HUD anclado. Aurora
      trae widescreen y escalado, pero el HUD bien anclado y el culling siguen
      siendo trabajo nuestro (un mod).
- [ ] 120 fps. El juego calcula todo por cuadro y presenta 30 por segundo,
      con dos retrazas de video por cuadro (medido en una laptop: 29.97 cuadros
      y 59.94 retrazas por segundo), así que subir la cadencia no es un ajuste:
      hay dos caminos. (a) Interpolar en el
      renderizador: se dibuja entre el estado anterior y el actual (posición
      de los objetos y de la cámara) sin tocar la lógica ni la velocidad del
      juego. Es la idea que permite tasas de cuadros altas en otros recomps
      ([Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), con
      RT64) y necesita el renderizador propio de la fase 2. (b) Correr la
      lógica a 120 Hz: obliga a escalar todo lo que el juego cuenta por cuadro
      (movimiento, temporizadores, animaciones, audio) y es fácil romper algo.
      Primer paso: separar lo que cuenta cuadros de lo que lee el reloj (la
      cadencia real ya está medida).
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
- [ ] Capa de mods en el runtime propio (ganchos y parches por nombre, como los
      de ModernGekko): el mod de Jamie y los demás se escriben una vez y
      tienen que seguir funcionando cuando el runtime cambie.

**Fase 5 – Release**
- [ ] Instalador que toma la ISO/RVZ del usuario, verifica hash y extrae.
- [ ] Builds Windows/Linux. Sin assets del juego en el repo ni en el release.

### Pista B – Mods del juego (en paralelo; con ModernGekko son paquetes `.mgm`)

- [x] Documentar el formato `.lsb`: [SCRIPTS.md](SCRIPTS.md) y `tools/lsb.py`
      (desensamblador y verificador; los 291 scripts del disco se leen sin
      errores).
- [x] Localizar la llamada de "fin de juego": no está en los scripts sino en
      código nativo; la cadena completa está en
      [SCRIPTS.md](SCRIPTS.md#qué-termina-el-juego).
- [ ] Mod de Jamie v1: un hook en `0x80166888` que borra la bandera 230 cuando
      termina la ceremonia (diseño en [SCRIPTS.md](SCRIPTS.md); sin probar
      todavía dentro del juego).
- [ ] Widescreen y velocidad como mods (o como códigos Gecko para quien use
      Dolphin): localizar las constantes.
- [ ] Mod de Jamie v2 (contenido): rutina de cónyuge, diálogos, eventos de hijo.

### Convenciones

Ver [CONTRIBUTING.md](../CONTRIBUTING.md): nombres que se explican solos,
comentarios que dicen el porqué, evidencia para cada nombre, un commit por
cambio en inglés, documentación bilingüe y nunca datos del juego.

---

## English

Priority: **the recompilation comes first** (track A). Track B (game mods)
starts now with the Jamie mod, because it works on the recompiled game;
everything track A discovers feeds into it.

**Goal: a fully native port, with no Dolphin code.** Today it is a hybrid: the
game's code already runs recompiled as native code on Windows ARM64, but the
hardware around it (video, audio, disc, input, memory card, timers) is still
the Dolphin core that ModernGekko brings.

**Decision (2026-11-04): the final runtime will be our own and will rest on
[aurora](https://github.com/encounter/aurora)** for GX, input, disc and memory
card; we write the audio and the operating system ourselves. A GX renderer is
not written from scratch and we do not wait for DolRecomp to bring its own
runtime. ModernGekko stays as the base that already works and as the reference
to compare against while it is replaced piece by piece. Why aurora, and what
is known and what is not, is in
[RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md). The order:

1. Jamie mod v1: it is small and it tests the hook and patch mechanism that
   the new pieces will use.
2. Identify the SDK by signature (phase 0): without knowing which functions
   belong to GX, OS, DVD, CARD, PAD and AX nothing can be replaced.
3. Feasibility experiment with aurora: take the game's GX calls to aurora and
   draw the title screen. It is the biggest risk on the way. In its favour:
   only 85 functions of `main.dol` store into the GX pipe (the SDK's library
   and eight vertex functions; `tools/find_gx_pipe_writers.py`), so GX can be
   replaced function by function, and display lists go through a single
   funnel of the game (`tools/find_callers.py`). Still unknown: whether aurora
   can replay display lists that are already built (if not, they are
   interpreted in that funnel) and whether it runs on DirectX 12 on Windows
   ARM64. If the experiment fails, we decide again with what it taught us.
4. Native DVD, CARD and PAD: the simplest ones, and they bring instant loads,
   save files and SDL input.
5. Audio (AX) and operating system (threads, alarms, interrupts, retraces).
6. Remove the Dolphin core from the runtime.

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
      runtime replaces in phase 2; it is step 2 of the order above).

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
      ran on GitHub, with the same figures, on ARM64: Linux with GCC, and
      Windows with MSVC and with the portable compiler llvm-mingw. It found a bug in
      Dolphin's interpreter (the carry of `addme` and `subfme`) and a gap in
      DolRecomp (`mtfsb1` and the FX bit); the game uses neither instruction.
      `run_isa_conformance.py --synthetic`; details in
      [VALIDATION.md](VALIDATION.md).
- [ ] Widen the validation with play (shops, village, minigames, festivals, a
      new day and the end of an event) and repeat on ARM64 (Linux and Windows)
      the two checks that use the game; the one that does not has already run
      on both with the same figures.
- [x] Milestone: move the character and play at full speed on a PC with a
      GPU. Done on a laptop with a Snapdragon (Windows ARM64) with DirectX 12:
      0.99 to 1.06 speed with the charger connected and the settings of
      [PERFORMANCE.md](PERFORMANCE.md); on battery, the city drops to ~0.8.
- [ ] Play with a GPU on Windows ARM laptops (Snapdragon): WSL2 cannot use
      the GPU there ([microsoft/wslg#1470](https://github.com/microsoft/wslg/issues/1470)).
      Route: ModernGekko built natively for Windows ARM64, with no WSL. It
      already builds there with MSVC, with the patches in `patches/`. With the
      portable compiler llvm-mingw it recompiles a made-up game and passes the
      instruction-level test. A GitHub workflow assembles the package
      (`packaging/windows`: a zip with `Play.bat`, where the player chooses
      their disc and the game is recompiled on their computer;
      `Play-Keyboard.bat` to play with the keyboard) and tests it the way a
      player would, with a made-up disc. It has already been tried with the
      real game on a laptop with a Snapdragon, with the GPU through DirectX 12
      (the package's default on ARM64): it runs at full speed with the charger
      connected ([PERFORMANCE.md](PERFORMANCE.md)). What is left is the
      battery and confirming the controller and keyboard on Windows.
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
- With ModernGekko the game is already playable without phase 2, because
  video, audio, disc and input come from the Dolphin core; phase 2 is what is
  left for the goal of a native port.

**Phase 2 – Own runtime** (the heavy part: it replaces Dolphin's hardware piece
by piece, in the order above)
- [ ] Identify the SDK by signature (pending from phase 0).
- [ ] Feasibility experiment: the game's GX calls draw the title screen with
      aurora.
- [ ] GX with aurora (WebGPU with Dawn: D3D12, Vulkan and Metal). Reference:
      Dolphin for GX semantics; Wiicompiled (Mario Kart Wii) for recomp
      architecture.
- [ ] DVD → direct reads from `orig/G4AEE9/files` (this alone gives instant loads).
- [ ] CARD → save files on disk (aurora reads Dolphin's `.gci` and `.raw`
      files).
- [ ] PAD → SDL gamepad and keyboard.
- [ ] AX audio → SDL/miniaudio.
- [ ] Operating system: threads, alarms, interrupts and video retraces without
      the Dolphin core.
- [ ] Remove the Dolphin core from the runtime; while the migration lasts,
      compare against it (lockstep verifier, screenshots).
- [ ] Milestone 1.0: playable start to finish at 480p 4:3 with no Dolphin code.

**Phase 3 – Enhancements**
- [ ] Arbitrary internal resolution (4K). With ModernGekko it is Dolphin's
      internal-resolution setting (not tried in the package yet); with aurora,
      its resolution scaling.
- [ ] Widescreen / ultrawide: projection matrix, culling, anchored HUD. Aurora
      brings widescreen and scaling, but a well-anchored HUD and culling remain
      our work (a mod).
- [ ] 120 fps. The game computes everything per frame and presents 30 a
      second, with two video retraces per frame (measured on a laptop: 29.97
      frames and 59.94 retraces a second), so raising the rate is not a
      setting: there are two routes. (a) Interpolate
      in the renderer: draw between the previous and the current state
      (object and camera positions) without touching the game's logic or
      speed. It is the idea behind high frame rates in other recomps
      ([Zelda64Recomp](https://github.com/Zelda64Recomp/Zelda64Recomp), with
      RT64) and needs the own renderer of phase 2. (b) Run the logic at
      120 Hz: it forces scaling everything the game counts per frame
      (movement, timers, animations, audio) and it is easy to break
      something. First step: tell what counts frames from what reads the
      clock (the real cadence is already measured).
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
- [ ] Mod layer in the own runtime (hooks and patches by name, like
      ModernGekko's): the Jamie mod and the others are written once and have
      to keep working when the runtime changes.

**Phase 5 – Release**
- [ ] Installer that takes the user's ISO/RVZ, verifies the hash and extracts.
- [ ] Windows/Linux builds. No game assets in the repo or the release.

### Track B – Game mods (in parallel; with ModernGekko they are `.mgm` packages)

- [x] Document the `.lsb` format: [SCRIPTS.md](SCRIPTS.md) and `tools/lsb.py`
      (disassembler and checker; all 291 scripts on the disc parse without
      problems).
- [x] Locate the "end game" call: it is not in the scripts but in native code;
      the whole chain is in [SCRIPTS.md](SCRIPTS.md#what-ends-the-game).
- [ ] Jamie mod v1: a hook on `0x80166888` that clears flag 230 once the
      ceremony is over (design in [SCRIPTS.md](SCRIPTS.md); not tried inside
      the game yet).
- [ ] Widescreen and speed as mods (or as Gecko codes for people using
      Dolphin): find the constants.
- [ ] Jamie mod v2 (content): spouse routine, dialogue, child events.

### Conventions

See [CONTRIBUTING.md](../CONTRIBUTING.md): self-explaining names, comments
that say why, evidence for every name, one English commit per change,
bilingual docs and never any game data.
