# Project plan / Plan del proyecto

[Español](#español) · [English](#english)

---

## Español

Dos pistas en paralelo. La pista B da resultados visibles pronto y todo lo
que descubre se reutiliza en la pista A.

### Pista A – Recompilación (port nativo)

**Fase 0 – Análisis del disco** ✅
- [x] Extraer disco, identificar binario objetivo y registrar hashes.
- [x] Layout del DOL, versión del SDK, archivos fuente, sistema de archivos.
- [x] Localizar el script de la boda con Jamie.
- [ ] Split inicial con decomp-toolkit (`dtk dol split`) para obtener límites
      de funciones y símbolos del SDK con nombre.

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

### Pista B – Mods en Dolphin (mientras tanto)

- [ ] Documentar el formato `.lsb` (desensamblar `lsrun.cpp` / `lsrun_bltin.cpp`).
- [ ] Localizar la llamada de "fin de juego" en `Eve_rm01.lsb` o en código nativo.
- [ ] Mod de Jamie v1: parche al script o código Gecko que salta el final.
- [ ] Widescreen y velocidad como códigos Gecko (localizar constantes).
- [ ] Mod de Jamie v2 (contenido): rutina de cónyuge, diálogos, eventos de hijo.

### Convenciones

- Un commit por cambio, mensajes en inglés.
- Documentación en español e inglés; nombres de archivo en inglés.
- El código debe leerse como escrito por una persona y traer guía para
  reproducir cada paso.
- Nunca subir datos del juego.

---

## English

Two parallel tracks. Track B produces visible results early and everything it
discovers feeds straight into track A.

### Track A – Recompilation (native port)

**Phase 0 – Disc analysis** ✅
- [x] Extract disc, identify target binary, record hashes.
- [x] DOL layout, SDK version, source file names, filesystem.
- [x] Locate the Jamie wedding script.
- [ ] Initial split with decomp-toolkit (`dtk dol split`) for function
      boundaries and named SDK symbols.

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

### Track B – Dolphin mods (meanwhile)

- [ ] Document the `.lsb` format (disassemble `lsrun.cpp` / `lsrun_bltin.cpp`).
- [ ] Locate the "end game" call in `Eve_rm01.lsb` or in native code.
- [ ] Jamie mod v1: script patch or Gecko code that skips the ending.
- [ ] Widescreen and speed as Gecko codes (find the constants).
- [ ] Jamie mod v2 (content): spouse routine, dialogue, child events.

### Conventions

- One commit per change, messages in English.
- Documentation in Spanish and English; file names in English.
- Code should read as if a person wrote it and come with a guide to
  reproduce each step.
- Never commit game data.
