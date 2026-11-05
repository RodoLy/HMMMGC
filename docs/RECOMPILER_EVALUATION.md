# Recompiler evaluation / Evaluación de recompiladores

Reviewed on 2026-11-03; the runtime landscape was added on 2026-11-04 /
Revisado el 2026-11-03; el panorama de runtimes se agregó el 2026-11-04.

[Español](#español) · [English](#english)

---

## Español

### Qué se decide

Qué herramienta traduce el código PowerPC del juego a C (el recompilador de
CPU) y qué la ejecuta en PC (el runtime: video, audio, controles, disco).

### Candidatos

| | DolRecomp + ModernGekko | NWiiRecomp | Recompilador propio |
|---|---|---|---|
| Revisado | DolRecomp `02e27ac` (10 oct 2026), ModernGekko-Template `c2553a9` (16 sep 2026) | `595f176` (7 ago 2026) | — |
| Licencia | GPL-3.0 (ModernGekko: GPL-3.0-or-later, deriva de Dolphin) | Propia: solo uso no comercial, derivados con términos igual de restrictivos, crédito obligatorio | La que elijas |
| CPU | C11, backends C y LLVM, ~44 000 líneas | C++, ~22 000 líneas (con runtime) | Por escribir |
| Pruebas | 30+ suites (opcodes, FPSCR, paired singles, tablas de saltos, comparación con Dolphin), CI en Linux/Windows/macOS | Su CMake no define pruebas | Por escribir |
| Runtime | ModernGekko: núcleo derivado de Dolphin para video, audio y HLE | Propio: rasterizador, HLE e interfaz de hardware; GX todavía en "seguimiento de estructuras" | Por escribir (GX es lo más largo) |
| Encaja con nuestro mapa | Sí: `--map` acepta `dirección tamaño nombre`; todos los backends escriben una macro `DOLRECOMP_SYMBOL_*` por función y el de LLVM además usa cada dirección como entrada y partición | Tiene su propio analizador | Sí, por diseño |
| Mods y parches | Parches por función en C con verificación FNV-64 del código original; reemplazo de funciones por código nativo | Hooks HLE por juego en TOML | Por diseñar |
| Estado público | El README de ModernGekko lista recomps de Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii y 007 Agent Under Fire (no verifiqué qué tan completos están) | Juegos que llegan a intros (Mario Party 7) | — |

### Recomendación: DolRecomp + ModernGekko

1. **Es el que mejor resuelve la parte difícil.** El runtime basado en el
   núcleo de Dolphin trae video (con resolución interna, o sea 4K), audio y
   HLE ya resueltos. Eso elimina la parte de meses del plan: escribir un
   renderizador de GX desde cero.
2. **Nuestro trabajo encaja directo.** Nuestras 4 486 funciones validadas y
   los nombres del SDK llegan a los mods como macros `DOLRECOMP_SYMBOL_*`,
   con el mismo header que DolRecomp escribe con `--map`
   (`tools/export_mod_symbols.py`); `tools/export_dolrecomp_map.py` da el
   mapa por si algún día usamos el backend LLVM, que sí toma sus direcciones
   como puntos de entrada.
3. **Las metas del juego son parches, que es lo que mejor soporta.** El mod
   de Jamie, la velocidad y la fricción del personaje, los cheats y el
   anclaje del HUD en widescreen son reemplazos de funciones en C. DolRecomp
   verifica con un hash que el código original sea el esperado antes de
   aplicar cada parche.
4. **La licencia es la estándar de la escena.** GPL-3.0 es la de
   Zelda64Recomp; no impide nada a un proyecto de fans. La de NWiiRecomp
   obligaría a tu proyecto a heredar sus restricciones.
5. **Está bien hecho.** Pruebas, CI en tres sistemas, guía de contribución.
   Coincide con el estándar que pedimos para nuestro código.

Riesgos:

- Todas las opciones son de 2026. ModernGekko ya tiene una lista de juegos
  recompilados con él, y Magical Melody ya arranca con este stack (ver
  abajo), pero falta jugarlo a velocidad real en una PC con GPU.
- El proyecto quedaría bajo GPL-3.0.
- ModernGekko descarga varios cientos de MB de dependencias de Dolphin.
- Los controles se configuran con un `.ini` de Dolphin a mano (no tiene UI).

### Cómo resuelven el runtime otros recomps de GameCube y Wii (2026-11-04)

Un recompilador como DolRecomp solo traduce el código del juego. Todo lo que
rodea a ese código (gráficos, audio, controles, disco, tarjeta de memoria,
sistema operativo) lo pone un runtime, y hay dos familias:

1. **Runtime derivado de Dolphin (lo que usamos hoy).** ModernGekko conserva
   el hardware emulado del núcleo de Dolphin y le quita solo la CPU. Es el
   camino más corto a un juego jugable. El código del juego corre como código
   nativo, pero el hardware que lo rodea sigue siendo código de Dolphin.
2. **Runtime propio.** No se emula el hardware: se enganchan las funciones del
   SDK de Nintendo que usa el juego (GX, PAD, DVD, CARD, OS, AX...) y se
   sustituyen por versiones para PC. Es lo que N64Recomp hace con RT64.

| Proyecto | Qué es | Cómo resuelve el runtime | Estado según su propia página |
|---|---|---|---|
| Mario Kart Wii (patchzyy, Wiicompiled) | Recompilación estática; el autor la presenta como la primera de un juego de Wii | "Sin emulador, sin intérprete, sin JIT". Gráficos sobre aurora, controles con SDL; Dolphin solo de referencia y como fuente de unos datos del DSP | No terminado: compatibilidad, render, red y rendimiento en trabajo; la interpolación de cuadros es experimental. Otro juego necesitaría su propio trabajo de runtime |
| Twilight Princess (Dusklight) | Port a partir de la decompilación (código fuente), no recompilación | "Powered by Aurora" | Soporta todos los discos comerciales salvo la versión coreana de Wii |
| Wind Waker (sp00nznet/ww) | Recompilación estática a C | Backend D3D11 propio para GX, mezclador de audio propio y reemplazos propios de OS, DVD, VI, CARD e hilos; todo marcado "en progreso" | Temprano: todavía no pasa de la pantalla de título |
| Recomps de ModernGekko (Super Mario Sunshine, Brawl, Melee, Luigi's Mansion, Kirby Wii, 007) | Recompilaciones de la comunidad | Runtime derivado de Dolphin, como el nuestro | No se verificó qué tan completos están |
| DolRecomp | Solo recompilador de CPU; no trae runtime | Sus autores dicen que planean un runtime y un renderizador propios, como N64Recomp con RT64 | Lanzamiento temprano, según SteamDeckHQ |

**Aurora** ([encounter/aurora](https://github.com/encounter/aurora), licencia
MIT) es una capa de compatibilidad de GameCube y Wii a nivel de código fuente.
Nació para Metaforce (la decompilación de Metroid Prime) y está pensada para
proyectos de decompilación. Según su README cubre:

- GX sobre WebGPU con Dawn (D3D12, Vulkan y Metal), widescreen, escalado de
  resolución y paquetes de texturas compatibles con Dolphin.
- PAD con `SDL_Gamepad` (adaptadores de GameCube, giroscopio, teclado y ratón).
- DVD con `nod`: todos los formatos de disco, RVZ incluido.
- CARD compatible con los `.gci` y `.raw` de Dolphin.
- Interfaces de depuración con Dear ImGui y de usuario con RmlUi.

No menciona VI, OS (hilos, interrupciones) ni AX (audio): eso lo pondría cada
proyecto.

Qué significa para Magical Melody (sin haberlo probado):

- La fase 2 de [PLAN.md](PLAN.md) ya no sería escribir un renderizador de GX
  desde cero: el candidato es integrar aurora y reemplazar con ella las
  funciones del SDK del juego. Esas funciones ya están identificadas
  (`docs/FUNCTION_MAP.md`) y lo que falta entre el juego y aurora se midió
  (sección siguiente); el audio y el sistema operativo quedan por escribir.
- Según su página, Mario Kart Wii usa aurora con un juego recompilado, no solo
  con una decompilación. La misma página advierte que cada juego pide su propio
  trabajo de runtime.
- Dato comprobado en `main.dol` con `tools/find_gx_pipe_writers.py`: solo 85
  funciones escriben directo en la tubería de comandos de GX (0xCC008000), y
  todas son la librería GX del SDK (77 funciones entre 0x8018EF00 y
  0x80195740) u ocho funciones pequeñas y vecinas (0x801CAB68–0x801CABD4) que
  mandan un byte, cuatro bytes, dos flotantes o tres flotantes, como las
  funciones de vértices del SDK. El código propio del juego no escribe en la
  tubería con el patrón habitual (`lis 0xCC01` y un almacenamiento en −0x8000).
  Eso es lo que permite reemplazar GX al nivel de las funciones, como hacen las
  decompilaciones con aurora, en vez de interpretar el flujo de comandos. No es
  una prueba: otras formas de armar la dirección no se buscan.
- Dato comprobado con `tools/find_callers.py`: las listas de visualización pasan
  por un solo embudo. La única función que manda a la tubería el comando para
  llamar una lista (el byte 0x40, seguido de la dirección y el tamaño de la
  lista) es `GXCallDisplayList` del SDK (0x80195010; la base de firmas del
  SDK confirma el nombre, ver `docs/FUNCTION_MAP.md`;
  `python3 tools/disasm.py GXCallDisplayList` la muestra). La llama una sola
  función diminuta del juego, `fn_801CBCEC`; a esa la llama un solo lugar,
  `fn_801B3914`, y `fn_801B4138` llama a este cuatro veces. Ningún puntero
  guardado en los datos apunta a ellas (las palabras con sus direcciones son
  entradas de la tabla de excepciones). Así, interpretar listas de GX, si
  aurora no lo hiciera, sería trabajo de un solo punto; aurora sí lo hace (ver
  la sección siguiente).

Qué no se verificó: no se compiló ni se ejecutó aurora ni ninguno de estos
proyectos. Lo de los proyectos viene de sus README y páginas y de notas de
prensa, y los estados pueden haber cambiado. El código de aurora se leyó
después (sección siguiente). Del lado del juego, no se miró qué traen las
listas guardadas en sus archivos, y las llamadas a través de un puntero armado
en un registro no se ven leyendo el código.

Fuentes:
[wiki de Wiicompiled](https://github.com/patchzyy/Wiicompiled/wiki),
[aurora](https://github.com/encounter/aurora),
[Dusklight](https://github.com/TwilitRealm/dusklight),
[README del recomp de Wind Waker](https://raw.githubusercontent.com/sp00nznet/ww/HEAD/README.md),
[SteamDeckHQ sobre DolRecomp](https://steamdeckhq.com/news/we-may-see-more-gamecube-wii-pc-ports-soon/),
[Tom's Hardware sobre Mario Kart Wii](https://www.tomshardware.com/video-games/retro-gaming/mario-kart-wii-recompiled-for-pc-using-ai-with-4k-potential-and-uncapped-frame-rates-first-static-recompilation-of-a-wii-game-supports-over-200-tracks-thanks-to-retro-rewind-compatibility)
y el README de ModernGekko (`lib/ModernGekko/README.md` en la plantilla).

### Lo que falta entre el juego y aurora (análisis estático, 2026-11-05)

La pregunta: si aurora fuera la capa de GX, ¿qué pide el juego que aurora no
tenga o haga distinto? Se respondió solo leyendo: el código de `main.dol` con
nuestras herramientas y el código fuente de aurora (commit `377ed36`,
2026-11-04, clonado solo para leerlo). **No se compiló ni se ejecutó nada**:
esto dice qué hay que hacer, no que funcione.

**Qué usa el juego de GX.** `tools/find_library_entry_points.py GX` lista las
funciones de la biblioteca a las que llega el resto del programa: 75 de las
156, con 354 llamadas en total, y las 75 tienen nombre
(`docs/FUNCTION_MAP.md`). Están muy concentradas: 63 las llama una sola
función del juego, 8 dos, 2 tres y `GXBegin` cuatro (`__GXShutdown` no tiene
llamadas: solo un puntero en los datos, en `0x802AAE8C`, con la forma de un
registro de reinicio del SDK). `GXSetVtxDesc` (33 llamadas), `GXSetArray`
(34), `GXSetTevOrder` (29) y las cuatro funciones de entradas y operaciones de
TEV (23 cada una) salen de una sola función del juego cada una;
`GXSetVtxAttrFmt` (33) sale de dos. Las de texturas se llaman una o dos veces
cada una.

**Qué cubre aurora.** Define 271 nombres de GX. De las 75 funciones del juego,
65 tienen cuerpo (que lo hagan bien no se comprobó), 6 están vacías o solo
devuelven 0 (`GXSetMisc`, `GXSetDispCopyDst`, `GXSetDispCopyYScale`,
`GXSetCopyFilter`, `GXSetDispCopyGamma` y `GXInvalidateTexAll`) y 4 no las
define: `GXSetDrawSyncCallback` (la declara `GXManage.h`, pero no tiene
definición), `GXSetGPMetric` y `GXClearGPMetric` (contadores de rendimiento de
la GPU) y `__GXShutdown`.

**Lo que habría que resolver al integrarla.**

| Tema | Qué se vio | Trabajo |
|---|---|---|
| Objetos de textura | El juego reserva `GXTexObj` de 32 bytes y `GXTlutObj` de 12 (`GXInitTexObj` escribe hasta el byte `0x1F` e `GXInitTlutObj` hasta el 9). En PC aurora usa 64 y 40 bytes (`include/dolphin/gx/GXStruct.h`). | Siete llamadas en total (`GXInitTexObj`, `GXInitTexObjCI`, `GXInitTexObjLOD`, `GXInitTlutObj`, `GXLoadTlut` una cada una y `GXLoadTexObj` dos). Un adaptador que guarde el objeto de aurora fuera de la memoria del juego, con la dirección del juego como llave. |
| Arreglos de vértices | `GXSetArray` recibe del juego atributo, dirección y paso; la versión de PC de aurora pide también el tamaño y si el arreglo es little-endian (`include/dolphin/gx/GXGeometry.h`). | 34 llamadas, todas desde `fn_801C9E68`: el tamaño se da en ese único lugar. |
| Fin de primitiva | En PC `GXEnd` es una función real que hay que llamar (`include/dolphin/gx/GXVert.h`); en el SDK es vacía y el juego no la llama. | `GXBegin` se llama desde 4 lugares: hay que cerrar la primitiva al llegar la cantidad de vértices que anunció. |
| Vértices escritos directo | Ocho funciones pequeñas del juego (`0x801CAB68`–`0x801CABD4`) escriben bytes o flotantes en la tubería (`0xCC008000`); las llaman seis funciones de dibujo. | Sustituir esas ocho por las de vértices de aurora (`GXPosition…`, `GXColor…`, `GXTexCoord…`). |
| Listas de visualización | El juego graba listas mientras corre: cuatro funciones suyas empiezan a grabar y cuatro terminan, por dos envoltorios (`fn_801CAC20` llama a `GXBeginDisplayList` y `fn_801CABE8` a `GXEndDisplayList`). Las reproduce por un solo embudo. aurora graba en el búfer que se le da y reproduce escribiendo la lista en su cola de comandos, pero su procesador de comandos ignora una lista dentro de otra («Ignoring nested GX_CMD_CALL_DL», `lib/gx/command_processor.cpp`). | Falta mirar si alguna lista del juego llama a otra. |
| `GXSetMisc` | Vacía en aurora (`lib/dolphin/gx/GXManage.cpp`). El juego la llama al iniciar los gráficos con `GXSetMisc(2, 0)`, que apaga el guardado del estado alrededor de las listas, y con `GXSetMisc(1, 8)`. aurora guarda y restaura el estado siempre (`dlSaveContext = 1` en `GXInit`). | Implementar al menos el indicador del guardado; si el juego depende del estado que deja una lista al grabarse, se vería en pantalla. |
| Callbacks | El juego instala un callback de sincronización de dibujo (una llamada al arrancar, por `main`) y otro de retrazado de VI. aurora no define `GXSetDrawSyncCallback` y sus `GXSetDrawSync` y `GXReadDrawSync` están vacías. En `.text` ningún `lis` ni `oris` arma la parte alta de los comandos de testigo (registros BP `0x47` y `0x48`), así que no se sabe qué, si algo, activa el primero. | Un modelo mínimo de interrupciones; es parte del paso de OS. |
| Punteros y flotantes | Las direcciones que el juego pasa (arreglos, imágenes, listas, matrices, destinos de copia) son del juego, no del anfitrión, y los argumentos flotantes llegan en registros de punto flotante. | Una capa fina que traduzca direcciones y lea esos registros, para las 75 funciones. |

**Conclusión.** No apareció nada que impida usar aurora como capa de GX. El
trabajo es de tamaño conocido: los ocho temas de la tabla, la mayoría en
pocas funciones del juego, más definir las 4 funciones que faltan y rellenar
las 6 vacías según importen. Lo que no se sabe sin ejecutar es si el resultado se
ve bien; eso se prueba en una PC con GPU (los equipos normales de GitHub
Actions no tienen).

**Lo que no se verificó.** No se compiló aurora ni nada con ella. No se miró
si las listas del juego se llaman unas a otras, ni qué comandos de GX usan que
el procesador de comandos de aurora no maneje. Las llamadas por un registro
(`bctrl`, `blrl`) no se ven leyendo el código. La lectura de que una función
de aurora «tiene cuerpo» no dice que lo haga bien.

**Dawn en Windows ARM64.** aurora dibuja con Dawn (WebGPU). Por defecto su
CMake baja un paquete ya compilado para Windows ARM64 (`dawn-windows-arm64`,
de las versiones de `encounter/dawn`; que ese archivo exista no se
comprobó); la alternativa es compilar Dawn desde su código
(`AURORA_DAWN_PROVIDER=vendor`), mucho más lento. Lo primero pone en la
prueba código binario de un tercero; lo segundo cuesta tiempo de CI. Es una
decisión pendiente del proyecto.

Fuentes: el código de [aurora](https://github.com/encounter/aurora) en el commit
`377ed36`: `include/dolphin/gx/GXStruct.h`, `GXGeometry.h`, `GXVert.h`,
`GXManage.h`, `lib/dolphin/gx/GXManage.cpp`, `GXDispList.cpp`,
`GXFrameBuffer.cpp`, `GXTexture.cpp`, `lib/gx/command_processor.cpp` y
`cmake/AuroraDawnProvider.cmake`.

### Resultado de la prueba de arranque (2026-11-03)

**Magical Melody arranca y dibuja con DolRecomp + ModernGekko.**

| Paso | Resultado |
|---|---|
| Compilar DolRecomp | Bien; sus 20 suites de pruebas pasan. |
| Recompilar `main.dol` | 448 834 de 448 840 instrucciones reconocidas, 0 desconocidas, 111 archivos de C. Con `--map` escribe una macro `DOLRECOMP_SYMBOL_*` para cada una de nuestras 4 486 funciones; el C generado es el mismo con o sin mapa. |
| Compilar el módulo nativo (gcc, 2 núcleos) | `gG4AEE9_recomp.so` en 21 minutos. |
| Ejecutar en ModernGekko | Carga el módulo con el entry point `0x80003154`. |
| 40 s | Pantalla "LICENSED BY NINTENDO". |
| 80 s | Logo de Natsume. |
| 140 s | Logo de Marvelous Interactive. |
| Tras pulsar Start/A | Escena 3D de apertura: la granja con el perro, ovejas y vacas. |

Más de 4 minutos corriendo, casi 500 frames y sin errores en el estado de
automatización. Va a 1.5–4 fps porque la máquina de prueba no tiene GPU y
todo el video se dibuja en la CPU (Vulkan por software, lavapipe); en una PC
con GPU debería ir a velocidad normal, falta comprobarlo.

Notas para repetir la prueba:

- El módulo del pipeline de ModernGekko se recompiló sin `--map`
  (`moderngekko-port` no lo pasa). Con el backend de C eso no cambia el
  código; los nombres para mods salen de `tools/export_mod_symbols.py`
  (ver [MODERNGEKKO_INTEGRATION.md](MODERNGEKKO_INTEGRATION.md)).
- La plantilla descarga GameTDB y `wit`, que solo sirven para Wii; si la red
  no lo permite, basta con llamar a
  `moderngekko-port build <juego extraído> --backend c --toolchain auto --output build/modules`.
- El submódulo `bzip2` de Dolphin vive en gitlab.com; si no se puede
  descargar, `libbz2-dev` del sistema lo reemplaza.
- Sin GPU, OpenGL no inicializó bajo Xvfb; Vulkan con lavapipe sí.
- La prueba se automatiza con `--automation-dir` (comandos de botones y
  capturas en archivos de texto, estado en `status.txt`).

### Pantalla de título y partida nueva (2026-11-03)

**Con el mismo módulo, el juego llega a la pantalla de título, abre el menú y
empieza una partida nueva.** Los botones se mandan por la carpeta de
automatización de ModernGekko, igual que si vinieran de un control.

| Paso | Resultado |
|---|---|
| Sin tocar nada tras los logos | Pantalla negra mientras carga (el contador de frames se queda en 420) y luego la apertura en 3D: el molino, el perro, las ovejas, las vacas y el granjero. |
| Start durante la apertura | Pantalla de título: logo de *Harvest Moon: Magical Melody*, "Push START" y los créditos de Natsume (2006) y Marvelous (2005). |
| Start en el título | Menú: New Game, Continue, Mini Games y Options. |
| New Game | Las preguntas de la partida nueva: estación del cumpleaños (Spring), chico o chica (Boy), confirmación e "Information registered". |
| Después | El protagonista en 3D en un camino empedrado y el anuncio del alcalde ("The Exciting Ranch Plan! ...") escribiéndose letra por letra. |
| Unos 25 A más | Escenas animadas del intro hasta la plaza del pueblo, donde el alcalde Theodore te recibe con confeti y empieza a hablar. |

Unos 26 minutos y 3 016 frames sin errores ni comandos fallidos, y el juego
cerró limpio. En las estadísticas de cierre `fallback=0`: el intérprete nunca
tuvo que ejecutar código, todo corrió desde el módulo recompilado (las
instrucciones que el código nativo le pasa al núcleo, como operaciones de
caché, se cuentan aparte en `hook_fb`).

Detalles útiles para la próxima prueba:

- Los menús mueven el cursor con el stick analógico (`main_y`); `dpad_up` no
  lo movía.
- Las confirmaciones Sí/No empiezan en "No".
- Con render por software en 2 núcleos va a 1–5 fps: unos 3 fps en los menús
  2D y 1.3–2 en las escenas 3D.
- El comando `screenshot` se marca como procesado antes de que la imagen se
  guarde; hay que esperar a que aparezca el archivo.

Falta la otra mitad del hito: mover al personaje y jugar a velocidad
completa en una PC con GPU.

### Cómo probarlo en tu PC

Ver [BUILDING.md](BUILDING.md): `scripts/build_and_run_linux.sh` hace todos
los pasos en Ubuntu o en WSL2. Se usa WSL2 en Windows porque los README se
contradicen: el de ModernGekko-Template pide Visual Studio (MSVC), pero el de
DolRecomp, más reciente, dice que ModernGekko no compila con MSVC por ahora.
La ruta de Linux es la mejor documentada (lista exacta de paquetes `apt`) y
WSL2 la da también en Windows ARM64, con ventanas gráficas vía WSLg. El
script llama a `moderngekko-port` directamente en vez de `make run`, que
además descarga herramientas que solo sirven para Wii.

---

## English

### What is being decided

Which tool translates the game's PowerPC code to C (the CPU recompiler) and
which runs it on PC (the runtime: video, audio, input, disc).

### Candidates

| | DolRecomp + ModernGekko | NWiiRecomp | Our own recompiler |
|---|---|---|---|
| Reviewed | DolRecomp `02e27ac` (2026-10-10), ModernGekko-Template `c2553a9` (2026-09-16) | `595f176` (2026-08-07) | — |
| License | GPL-3.0 (ModernGekko: GPL-3.0-or-later, Dolphin-derived) | Custom: non-commercial only, derivatives must carry equally restrictive terms, mandatory credit | Whichever we choose |
| CPU | C11, C and LLVM backends, ~44,000 lines | C++, ~22,000 lines (runtime included) | To be written |
| Tests | 30+ suites (opcodes, FPSCR, paired singles, jump tables, cross-checks against Dolphin), CI on Linux/Windows/macOS | Its CMake defines no tests | To be written |
| Runtime | ModernGekko: Dolphin-derived core for video, audio and HLE | Its own rasterizer, HLE and hardware interface; GX still at "structure tracking" | To be written (GX is the longest part) |
| Fits our map | Yes: `--map` takes "address size name" lines; every backend writes a `DOLRECOMP_SYMBOL_*` macro per function and the LLVM one also uses every address as an entry and partition point | Has its own analyzer | Yes, by design |
| Mods and patches | Per-function C patches verified with an FNV-64 hash of the original code; native function replacements | Per-game HLE hooks in TOML | To be designed |
| Public status | ModernGekko's README lists recomps of Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii and 007 Agent Under Fire (how complete they are was not checked) | Games that reach their intros (Mario Party 7) | — |

### Recommendation: DolRecomp + ModernGekko

1. The Dolphin-derived runtime solves video (internal resolution, i.e. 4K),
   audio and HLE, which removes the months-long part of the plan: writing a
   GX renderer from scratch.
2. Our work plugs straight in: our 4,486 validated functions and the SDK
   names reach mods as `DOLRECOMP_SYMBOL_*` macros, the same header DolRecomp
   writes with `--map` (`tools/export_mod_symbols.py`);
   `tools/export_dolrecomp_map.py` provides the map in case we ever use the
   LLVM backend, which does take its addresses as entry points.
3. The game's goals (Jamie mod, player speed and friction, cheats,
   widescreen HUD anchoring) are function replacements in C, which DolRecomp
   checks against a hash of the original code before applying.
4. GPL-3.0 is the scene's standard (Zelda64Recomp uses it); NWiiRecomp's
   license would force its restrictions onto this project.
5. It is well engineered (tests, CI, contribution guide), matching the
   standard this repository holds itself to.

Risks: every option dates from 2026. ModernGekko's README lists recomps of
Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii
and 007 Agent Under Fire (how complete they are was not checked), and
Magical Melody now boots on this stack (see below); the project
would be GPL-3.0; ModernGekko pulls several hundred MB of Dolphin
dependencies; input is configured through a hand-written Dolphin `.ini`.

### How other GameCube and Wii recomps provide their runtime (2026-11-04)

A recompiler such as DolRecomp only translates the game's code. Everything
around that code (graphics, audio, input, disc, memory card, operating system)
comes from a runtime, and there are two families:

1. **Dolphin-derived runtime (what we use today).** ModernGekko keeps the
   emulated hardware of Dolphin's core and removes only the CPU. It is the
   shortest way to a playable game. The game's code runs as native code, but
   the hardware around it is still Dolphin's code.
2. **Own runtime.** The hardware is not emulated: the Nintendo SDK functions
   that the game uses (GX, PAD, DVD, CARD, OS, AX...) are hooked and replaced
   with PC versions. This is what N64Recomp does with RT64.

| Project | What it is | How it provides the runtime | Status according to its own page |
|---|---|---|---|
| Mario Kart Wii (patchzyy, Wiicompiled) | Static recompilation; the author presents it as the first one of a Wii game | "No emulator, no interpreter, no JIT". Graphics on aurora, input with SDL; Dolphin only as a reference and as the source of some DSP data | Not finished: compatibility, rendering, networking and performance are in progress; frame interpolation is experimental. Another game would need its own runtime work |
| Twilight Princess (Dusklight) | A port built from the decompilation (source code), not a recompilation | "Powered by Aurora" | Supports every commercial disc except the Korean Wii release |
| Wind Waker (sp00nznet/ww) | Static recompilation to C | Its own D3D11 backend for GX, its own audio mixer and its own replacements for OS, DVD, VI, CARD and threads; all marked "in progress" | Early: it does not get past the title screen yet |
| ModernGekko recomps (Super Mario Sunshine, Brawl, Melee, Luigi's Mansion, Kirby Wii, 007) | Community recompilations | Dolphin-derived runtime, like ours | How complete they are was not checked |
| DolRecomp | CPU recompiler only; ships no runtime | Its authors say they plan their own runtime and renderer, like N64Recomp with RT64 | Early release, according to SteamDeckHQ |

**Aurora** ([encounter/aurora](https://github.com/encounter/aurora), MIT
license) is a source-level compatibility layer for GameCube and Wii. It was
born for Metaforce (the Metroid Prime decompilation) and is meant for
decompilation projects. According to its README it covers:

- GX on WebGPU with Dawn (D3D12, Vulkan and Metal), widescreen, resolution
  scaling and Dolphin-compatible texture packs.
- PAD with `SDL_Gamepad` (GameCube adapters, gyro, keyboard and mouse).
- DVD with `nod`: every disc image format, RVZ included.
- CARD compatible with Dolphin's `.gci` and `.raw` files.
- Dear ImGui for debug interfaces and RmlUi for user interfaces.

It does not mention VI, OS (threads, interrupts) or AX (audio): each project
would provide those.

What this means for Magical Melody (untested):

- Phase 2 of [PLAN.md](PLAN.md) would no longer be writing a GX renderer from
  scratch: the candidate is to integrate aurora and use it to replace the
  game's SDK functions. Those functions are already identified
  (`docs/FUNCTION_MAP.md`) and what is missing between the game and aurora has
  been measured (next section); the audio and the operating system are left to
  write.
- According to its page, Mario Kart Wii uses aurora with a recompiled game,
  not only with a decompilation. The same page warns that every game needs its
  own runtime work.
- Fact checked on `main.dol` with `tools/find_gx_pipe_writers.py`: only 85
  functions store straight into the GX command pipe (0xCC008000), and all of
  them are either the SDK's GX library (77 functions between 0x8018EF00 and
  0x80195740) or eight small neighbouring functions (0x801CAB68-0x801CABD4)
  that send one byte, four bytes, two floats or three floats, like the SDK's
  vertex functions. The game's own code does not store into the pipe with the
  usual pattern (`lis 0xCC01` and a store at -0x8000). That is what allows
  replacing GX at the level of the functions, as the decompilation ports do
  with aurora, instead of interpreting the command stream. It is not a proof:
  other ways of building the address are not looked for.
- Fact checked with `tools/find_callers.py`: display lists go through a single
  funnel. The only function that sends the command for calling a list to the
  pipe (the byte 0x40, followed by the list's address and size) is
  the SDK's `GXCallDisplayList` (0x80195010; the SDK signature database
  confirms the name, see `docs/FUNCTION_MAP.md`;
  `python3 tools/disasm.py GXCallDisplayList` shows it). One tiny function of
  the game, `fn_801CBCEC`, calls it; that one is called from a single place,
  `fn_801B3914`, and `fn_801B4138` calls this one four times. No pointer kept
  in the data points to them (the words holding their addresses are exception
  table entries). So interpreting GX lists, if aurora did not, would be work
  for a single point; aurora does it (see the next section).

What was not verified: neither aurora nor any of these projects was built or
run. What is said of the projects comes from their READMEs and pages and from
press articles, and their status may have changed. Aurora's code was read
afterwards (next section). On the game's side, what the lists stored in its
files contain was not looked at, and calls through a pointer built in a
register cannot be seen by reading the code.

Sources:
[Wiicompiled wiki](https://github.com/patchzyy/Wiicompiled/wiki),
[aurora](https://github.com/encounter/aurora),
[Dusklight](https://github.com/TwilitRealm/dusklight),
[Wind Waker recomp README](https://raw.githubusercontent.com/sp00nznet/ww/HEAD/README.md),
[SteamDeckHQ on DolRecomp](https://steamdeckhq.com/news/we-may-see-more-gamecube-wii-pc-ports-soon/),
[Tom's Hardware on Mario Kart Wii](https://www.tomshardware.com/video-games/retro-gaming/mario-kart-wii-recompiled-for-pc-using-ai-with-4k-potential-and-uncapped-frame-rates-first-static-recompilation-of-a-wii-game-supports-over-200-tracks-thanks-to-retro-rewind-compatibility)
and ModernGekko's README (`lib/ModernGekko/README.md` in the template).

### What is missing between the game and aurora (static analysis, 2026-11-05)

The question: if aurora were the GX layer, what does the game ask for that
aurora does not have or does differently? It was answered by reading alone: the
code of `main.dol` with our tools and aurora's source code (commit `377ed36`,
2026-11-04, cloned only to read it). **Nothing was built or run**: this says
what has to be done, not that it works.

**What the game uses of GX.** `tools/find_library_entry_points.py GX` lists the
functions of the library that the rest of the program reaches: 75 of the 156,
with 354 calls in all, and all 75 have a name (`docs/FUNCTION_MAP.md`). They
are very concentrated: 63 are called by a single function of the game, 8 by
two, 2 by three and `GXBegin` by four (`__GXShutdown` has no calls: only a
pointer in the data, at `0x802AAE8C`, shaped like an SDK reset record).
`GXSetVtxDesc` (33 calls), `GXSetArray` (34), `GXSetTevOrder` (29) and the four
TEV input and operation functions (23 each) come from a single function of the
game each; `GXSetVtxAttrFmt` (33) comes from two. The texture ones are called
once or twice each.

**What aurora covers.** It defines 271 GX names. Of the game's 75 functions,
65 have a body (that they do their job well was not checked), 6 are empty or
only return 0 (`GXSetMisc`, `GXSetDispCopyDst`, `GXSetDispCopyYScale`,
`GXSetCopyFilter`, `GXSetDispCopyGamma` and `GXInvalidateTexAll`) and 4 it does
not define: `GXSetDrawSyncCallback` (`GXManage.h` declares it, but there is no
definition), `GXSetGPMetric` and `GXClearGPMetric` (GPU performance counters)
and `__GXShutdown`.

**What would have to be solved to integrate it.**

| Topic | What was seen | Work |
|---|---|---|
| Texture objects | The game reserves a `GXTexObj` of 32 bytes and a `GXTlutObj` of 12 (`GXInitTexObj` writes up to byte `0x1F` and `GXInitTlutObj` up to byte 9). On PC aurora uses 64 and 40 bytes (`include/dolphin/gx/GXStruct.h`). | Seven calls in all (`GXInitTexObj`, `GXInitTexObjCI`, `GXInitTexObjLOD`, `GXInitTlutObj` and `GXLoadTlut` once each, `GXLoadTexObj` twice). An adapter that keeps aurora's object outside the game's memory, with the game's address as the key. |
| Vertex arrays | `GXSetArray` gets attribute, address and stride from the game; aurora's PC version also asks for the size and whether the array is little-endian (`include/dolphin/gx/GXGeometry.h`). | 34 calls, all from `fn_801C9E68`: the size is given in that one place. |
| End of a primitive | On PC `GXEnd` is a real function that has to be called (`include/dolphin/gx/GXVert.h`); in the SDK it is empty and the game does not call it. | `GXBegin` is called from 4 places: the primitive has to be closed when the number of vertices it announced is reached. |
| Vertices written directly | Eight small functions of the game (`0x801CAB68`–`0x801CABD4`) write bytes or floats to the pipe (`0xCC008000`); six drawing functions call them. | Replace those eight with aurora's vertex functions (`GXPosition…`, `GXColor…`, `GXTexCoord…`). |
| Display lists | The game records lists while it runs: four of its functions start recording and four finish, through two wrappers (`fn_801CAC20` calls `GXBeginDisplayList` and `fn_801CABE8` calls `GXEndDisplayList`). It replays them through a single funnel. Aurora records into the buffer it is given and replays by writing the list to its command queue, but its command processor ignores a list inside another one ("Ignoring nested GX_CMD_CALL_DL", `lib/gx/command_processor.cpp`). | Look at whether any list of the game calls another. |
| `GXSetMisc` | Empty in aurora (`lib/dolphin/gx/GXManage.cpp`). The game calls it when it starts the graphics with `GXSetMisc(2, 0)`, which turns off saving the state around display lists, and with `GXSetMisc(1, 8)`. Aurora always saves and restores the state (`dlSaveContext = 1` in `GXInit`). | Implement at least the flag for saving; if the game depends on the state a list leaves behind when it is recorded, it would show on screen. |
| Callbacks | The game installs a draw-sync callback (one call at start-up, through `main`) and a VI retrace callback. Aurora does not define `GXSetDrawSyncCallback` and its `GXSetDrawSync` and `GXReadDrawSync` are empty. In `.text` no `lis` or `oris` builds the high half of the token commands (BP registers `0x47` and `0x48`), so it is not known what, if anything, triggers the first one. | A minimal interrupt model; it belongs to the OS step. |
| Pointers and floats | The addresses the game passes (arrays, images, lists, matrices, copy destinations) are the game's, not the host's, and float arguments arrive in floating-point registers. | A thin layer that translates addresses and reads those registers, for the 75 functions. |

**Conclusion.** Nothing turned up that stops aurora from being the GX layer.
The work has a known size: the eight topics of the table, most of them in a
few functions of the game, plus defining the 4 missing functions and filling
in the 6 empty ones as they matter. What cannot be known without running it is
whether the result looks right; that is tested on a PC with a GPU (ordinary
GitHub Actions runners have none).

**What was not verified.** Aurora was not built, nor anything with it. Whether
the game's lists call one another was not looked at, nor which GX commands
they use that aurora's command processor does not handle. Calls through a
register (`bctrl`, `blrl`) cannot be seen by reading the code. Reading that an
aurora function "has a body" does not say it does the job well.

**Dawn on Windows ARM64.** Aurora draws with Dawn (WebGPU). By default its
CMake downloads a prebuilt package for Windows ARM64 (`dawn-windows-arm64`,
from the releases of `encounter/dawn`; that the file exists was not checked);
the alternative is building Dawn from its source
(`AURORA_DAWN_PROVIDER=vendor`), much slower. The first puts a third party's
binary code into the test; the second costs CI time. It is a decision still
open for the project.

Sources: [aurora](https://github.com/encounter/aurora)'s code at commit
`377ed36`: `include/dolphin/gx/GXStruct.h`, `GXGeometry.h`, `GXVert.h`,
`GXManage.h`, `lib/dolphin/gx/GXManage.cpp`, `GXDispList.cpp`,
`GXFrameBuffer.cpp`, `GXTexture.cpp`, `lib/gx/command_processor.cpp` and
`cmake/AuroraDawnProvider.cmake`.

### Boot test result (2026-11-03)

**Magical Melody boots and renders on DolRecomp + ModernGekko.**

| Step | Result |
|---|---|
| Build DolRecomp | Fine; its 20 test suites pass. |
| Recompile `main.dol` | 448,834 of 448,840 instructions known, 0 unknown, 111 C files. With `--map` it writes a `DOLRECOMP_SYMBOL_*` macro for each of our 4,486 functions; the generated C is the same with or without a map. |
| Build the native module (gcc, 2 cores) | `gG4AEE9_recomp.so` in 21 minutes. |
| Run on ModernGekko | Loads the module with entry point `0x80003154`. |
| 40 s | "LICENSED BY NINTENDO" screen. |
| 80 s | Natsume logo. |
| 140 s | Marvelous Interactive logo. |
| After pressing Start/A | The 3D opening scene: the farm with the dog, sheep and cows. |

Over 4 minutes running, almost 500 frames, no errors in the automation
status. It runs at 1.5–4 fps because the test machine has no GPU and all
video is drawn on the CPU (software Vulkan, lavapipe); on a PC with a GPU it
should run at full speed, which is still to be confirmed.

Notes for repeating the test:

- The module from ModernGekko's pipeline was recompiled without `--map`
  (`moderngekko-port` does not pass it). With the C backend that does not
  change the code; names for mods come from `tools/export_mod_symbols.py`
  (see [MODERNGEKKO_INTEGRATION.md](MODERNGEKKO_INTEGRATION.md)).
- The template downloads GameTDB and `wit`, which only Wii games need; when
  the network does not allow it, call
  `moderngekko-port build <extracted game> --backend c --toolchain auto --output build/modules`
  directly.
- Dolphin's `bzip2` submodule lives on gitlab.com; if it cannot be fetched,
  the system's `libbz2-dev` replaces it.
- Without a GPU, OpenGL did not initialise under Xvfb; Vulkan with lavapipe
  did.
- The test is automated with `--automation-dir` (button and screenshot
  commands as text files, state in `status.txt`).

### Title screen and new game (2026-11-03)

**With the same module, the game reaches the title screen, opens its menu and
starts a new game.** Buttons are sent through ModernGekko's automation
folder, exactly as if they came from a controller.

| Step | Result |
|---|---|
| No input after the logos | A black screen while it loads (the frame counter holds at 420), then the 3D opening: the windmill, the dog, sheep, cows and the farmer. |
| Start during the opening | Title screen: the *Harvest Moon: Magical Melody* logo, "Push START" and the Natsume (2006) and Marvelous (2005) credits. |
| Start on the title | Menu: New Game, Continue, Mini Games and Options. |
| New Game | The new-game questions: birthday season (Spring), boy or girl (Boy), confirmation and "Information registered". |
| Then | The 3D protagonist on a cobbled road and the mayor's flyer ("The Exciting Ranch Plan! ...") typing out letter by letter. |
| About 25 more A presses | Animated intro scenes up to the village square, where Mayor Theodore welcomes you with confetti and starts talking. |

About 26 minutes and 3,016 frames with no errors or failed commands, and the
game shut down cleanly. Its shutdown statistics show `fallback=0`: the
interpreter never had to run any code, everything ran from the recompiled
module (instructions the native code hands to the core, such as cache
operations, are counted separately as `hook_fb`).

Useful details for the next test:

- Menus move their cursor with the analog stick (`main_y`); `dpad_up` did not
  move it.
- Yes/No confirmations start on "No".
- With software rendering on 2 cores it runs at 1–5 fps: about 3 fps in 2D
  menus and 1.3–2 in 3D scenes.
- The `screenshot` command is marked processed before the image is saved;
  wait for the file to appear.

The other half of the milestone is still open: moving the character and
playing at full speed on a PC with a GPU.

### Trying it on your PC

See [BUILDING.md](BUILDING.md): `scripts/build_and_run_linux.sh` does every
step on Ubuntu or in WSL2. WSL2 is used on Windows because the READMEs
disagree: the template's asks for Visual Studio (MSVC), but DolRecomp's newer
README says ModernGekko does not build under MSVC yet. The Linux path is the
best documented one (exact `apt` package list), and WSL2 provides it on
Windows ARM64 too, with GUI windows through WSLg. The script calls
`moderngekko-port` directly instead of `make run`, which also downloads
tools only Wii games need.
