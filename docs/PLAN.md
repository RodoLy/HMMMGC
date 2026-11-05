# Project plan / Plan del proyecto

[Español](#español) · [English](#english)

---

## Español

Prioridad: **la recompilación va primero** (pista A). La pista B (mods del
juego) empieza ya con el mod de Jamie, porque funciona sobre el juego
recompilado; todo lo que la pista A descubre le sirve. La pista C (launcher)
es la cara del proyecto: la aplicación con la que el jugador carga su juego,
activa mods y juega, como en otras recompilaciones. Avanza en paralelo; hoy
son scripts de PowerShell sueltos.

**Decisión (2026-11-05, corregida el mismo día): la pista A sigue.** El
jugador pidió quitar Dolphin solo si otra recompilación de GameCube o Wii ya
lo hace. Al principio se creyó que ninguna lo hacía, pero sí hay una:
[WiiCompiled](https://github.com/patchzyy/wiicompiled), la recompilación
estática de Mario Kart Wii (GPL-3.0, como este proyecto), corre sin emulador:
traduce el código del juego por adelantado y lo une a un runtime propio de
alto nivel (DVD, NAND, PAD, audio, ritmo de video y servicios del sistema)
con [aurora](https://github.com/encounter/aurora) para los gráficos sobre
Dawn (D3D12, Vulkan, Metal). Dolphin solo le sirvió de referencia del
hardware. Ya hay otra recompilación hecha sobre ella (New Super Mario Bros.
Wii). Es el mismo camino que esta pista eligió el 2026-11-04, así que el orden
de abajo se mantiene, ahora con WiiCompiled como referencia de un runtime que
ya funciona con aurora; falta estudiar cuánto de él sirve para un juego de
GameCube. Mientras tanto, la base que juega sigue siendo ModernGekko.
Varios proyectos ya la usan de base: New Super Mario Bros. Wii, KartPad (iOS,
macOS, Android), DriftDroid (Android) y WebCompiled (navegador). Su traductor
lee cualquier DOL; lo que hay que poner por juego es el runtime (audio,
controles, GX, disco). Primer paso: traducir el `main.dol` de este juego con
su traductor y llevarlo hasta la pantalla de título con su runtime, viendo
qué le falta para GameCube (memory card en vez de NAND, sin IOS).

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
   pasan por un solo embudo del juego (`tools/find_callers.py`). El análisis
   estático ya midió lo que falta (75 funciones de GX que usa el juego, de
   las que aurora define 71, y ocho temas de integración; ver
   [RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md)) y no encontró nada que
   lo impida. Falta compilar aurora y ver si corre sobre DirectX 12 en
   Windows ARM64 con GPU. Si el experimento falla, se decide de nuevo con lo
   aprendido.
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
      274 tablas de `switch`, validado contra `extabindex` (2 597/2 597).
      Herramientas propias en vez de dtk; ver [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Nombres con evidencia: arranque de CodeWarrior y los 13 `Init` del SDK.
- [x] Identificar el SDK por firma (paso 2 del orden de arriba):
      `tools/sdk_signatures.py` compara cada función con la base de firmas de
      Dolphin y puso 563 nombres más (588 en total). Dio el rango de cada
      biblioteca del SDK, que es la frontera de lo que el runtime reemplaza en
      la fase 2; ver [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Nombrar las funciones de GX que el juego usa: `tools/find_library_entry_points.py`
      lista las 75 que alcanza el resto del programa; la firma nombró 49 y
      las otras 26 se nombraron leyendo su código, cada una con una prueba
      contra `main.dol` (614 nombres en total).
- [x] Nombrar los demás puntos de entrada del SDK que usa el juego: 39 de OS,
      CARD, DVD, VI, AI, DSP, AR y PAD, y dos funciones internas de CARD, se
      nombraron leyendo su código, cada una con una prueba contra `main.dol`
      (655 nombres en total; ver [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md)).
- [x] Nombrar los 27 puntos de entrada sin nombre de la zona del audio (AX,
      AXFX, MIX y dos de DSP), que la herramienta solo veía con un tramo dado a
      mano; cada uno con su prueba contra `main.dol` (682 nombres en total).
- [ ] Afinar los nombres del SDK: usar el archivo del enlazador que trae la
      mitad de la base (para AX, TRK y la biblioteca C) y nombrar `PPCArch` por
      el número de registro especial. No bloquea nada.

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
- [x] Identificar el SDK por firma (hecho en la fase 0).
- [x] Análisis estático de viabilidad: qué pide el juego de GX y qué tiene
      aurora ([RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md)).
- [ ] Experimento de viabilidad: las llamadas de GX del juego dibujan la
      pantalla de título con aurora. Necesita compilar aurora y una PC con
      GPU; antes hay que decidir cómo se obtiene Dawn en Windows ARM64.
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
- [ ] Velocidad del jugador en un archivo de configuración. Hoy se elige con
      `-PlayerSpeed` de `play.ps1` (o con la variable
      `HM_MM_PLAYER_SPEED_PERCENT`), ver [MOVEMENT.md](MOVEMENT.md). La
      fricción no hace falta: al soltar el stick el personaje se detiene en
      unos 4 cuadros caminando y unos 9 corriendo.
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
- [x] Mod de Jamie v1: un hook en `0x80166888` que apaga la bandera 230 una
      vez que la ceremonia empezó (diseño en [SCRIPTS.md](SCRIPTS.md)). Está
      en [`mods/jamie_marriage`](../mods/jamie_marriage/mod.c), compila sin
      avisos y se probó dentro del juego con condiciones escritas a mano: sin
      el mod siguen el epílogo y el título; con él, el juego queda en juego
      libre ([resultados](SCRIPTS.md#prueba-en-ejecución-del-mod-2026-11-04)).
- [ ] Mods en el paquete de Windows: `play.ps1` construye los activos de
      `game.psd1` (`ModIds`) con el compilador del paquete, los instala en la
      carpeta del jugador y no impide jugar si fallan; `-NoMods` juega sin
      ellos. Se probó en la laptop con Windows ARM64 (2026-11-05, con los
      archivos nuevos copiados sobre el paquete): el compilador del paquete
      construyó los dos mods y el runner los cargó (`mod loaded:` en
      `Logs\play.log`). Falta el flujo de GitHub, que comprueba que el runner
      cargue cada mod y no se ha vuelto a ejecutar (cuesta unos 30 minutos).
- [ ] Ventana de ajustes provisional (`Settings.bat`): una casilla por mod y
      la velocidad del personaje de 25 % a 300 % con deslizador, con botones
      para guardar y jugar. Guarda `settings.json` en la carpeta del jugador y
      `play.ps1` lo sigue (`-PlayerSpeed` gana solo esa vez; `-NoMods` lo
      ignora). Es un puente: la pista C lo reemplaza por el launcher y conserva
      el mismo archivo. Se escribió sin poder ejecutar PowerShell en el
      contenedor (las pruebas comparan los archivos entre sí y un analizador
      revisa la sintaxis); la primera ejecución en la laptop encontró un error
      de texto vacío en una etiqueta, ya corregido. Falta confirmarla allí.
- [ ] Probar el mod de Jamie con una partida real: la propuesta de verdad,
      guardar y cargar, una segunda propuesta y los dos sexos del jugador.
      Necesita un guardado cerca de la boda con Jamie, que hoy no existe: se
      está armando uno con la propia rutina de guardado del juego, para que el
      jugador lo pruebe en su laptop.
- [ ] **Jamie vive en la casa como cónyuge** después de la boda, igual que
      cualquier otra pareja del juego (lo pidió el jugador el 2026-11-05). El
      juego original no lo hace: con Jamie la boda termina la partida, así que
      no hay rutina de cónyuge para Jamie. Pasos: (1) ver qué guarda el juego
      cuando te casas con otra pareja (bandera 161, id de la pareja en
      `save+0x22B8`, valor 137, bandera 0, la cama doble) y escribir eso
      mismo para Jamie al terminar la ceremonia; (2) ver si el juego pone a
      Jamie en la casa con eso, o si hay que agregarlo como personaje de la
      casa (posición, horario, diálogos de cónyuge, que pueden tener que
      tomarse prestados de otra pareja). Es el mod de Jamie v2.
      **Paso 1 hecho** (`jamie_marriage` 0.2.0): el código que inicia la boda
      de las otras veinte parejas (`fn_80166888`, desde `0x801672B8`) hace
      siete escrituras en la partida: la pareja (`save+0x22B8`), la bandera
      0, el bit 3 de los bits del personaje (`save+0x5564 + 3·n`), dos copias
      (`0x2234`→`0xD514`, `0x224C`→`0xD520`) y `0x1BC8 = 0`. El mod las hace
      con Jamie (personaje 2) al apagar la bandera 230. Probado dentro del
      juego: tras la revisión del campo sale la nota musical de la boda
      ("Your life together starts today"). Falta el paso 2: en la prueba,
      con la casa inicial, la pareja no apareció en la casa, ni Jamie ni una
      pareja normal; hay que probar con la casa nivel 3 (lo que exige el
      juego para casarse).
- [ ] Avance rápido de las escenas mientras se mantiene una tecla (la barra
      espaciadora): un mod que acelere los eventos y la asignación de la
      tecla (hoy Espacio no está asignada en la distribución de teclado de
      Dolphin). Se activa y se ajusta desde la ventana de ajustes y, después,
      desde el launcher.
- [ ] Mod de Jamie v1.1: que la ceremonia no vaya seguida de la escena de las
      notas (opción A+ de [SCRIPTS.md](SCRIPTS.md): pasar al estado 9 al
      terminar la ceremonia; falta comprobar qué hace ese estado).
- [ ] Convertir la prueba dentro del juego en una herramienta del repositorio
      (`tools/`): hoy son guiones desechables. Hace falta para repetirla cuando
      cambie el runtime (pasos 3 a 6 del orden de arriba).
- [x] Velocidad del personaje como mod: [`mods/player_speed`](../mods/player_speed/mod.c)
      engancha la actualización del jugador y multiplica la velocidad
      horizontal (150 % por defecto, de 25 % a 300 %), solo en juego libre.
      Se midió dentro del juego recompilado: en una caminata de 15 cuadros la
      distancia fue exactamente proporcional al factor con 100, 150 y 200 %.
      Con 400 % el personaje atravesó la puerta de su casa, por eso el máximo
      es 300 %. Cómo se mueve el personaje y los resultados están en
      [MOVEMENT.md](MOVEMENT.md). Probado en la laptop con Windows ARM64
      (2026-11-05): funciona, según el jugador.
- [ ] Widescreen como mod (o como códigos Gecko para quien use Dolphin):
      localizar las constantes.
- [ ] Mod de Jamie v2 (contenido): rutina de cónyuge, diálogos, eventos de hijo.

### Pista C – Launcher y experiencia del jugador (en paralelo)

**Meta (la pidió el jugador el 2026-11-05):** una aplicación de verdad, como las
de otras recompilaciones (por ejemplo Zelda64Recomp): una ventana propia donde
se carga el juego, se activan los mods y se ajusta todo, en lugar de los
`.bat` y `.ps1` sueltos de hoy.

**Decisión: no se escribe un launcher desde cero.** ModernGekko ya trae uno
(`tools/moderngekko_launcher.cpp`, en C++ con SDL3 y Dear ImGui) con selección
de la imagen del disco y extracción, controles, API gráfica, resolución,
pantalla completa y partida en red; el nuestro (`HarvestMoonMM.exe`) sale de
él. Lo que le falta es justo lo que hoy hace `play.ps1`: recompilar el juego y
manejar los mods. La interfaz no depende del núcleo de Dolphin (solo la lectura
del disco usa hoy su `DiscIO`, que cambia con el paso 4 de la pista A), así que
sobrevive al cambio de runtime. Se prefiere un parche mínimo que enlace código
propio de este repositorio (`launcher/`) a un parche grande sobre un código
ajeno; el clon de ModernGekko no se toca.

- [ ] C0. Diseño: pantallas (Juego, Mods, Controles, Ajustes, Acerca de y
      licencias), textos en español e inglés, y un solo archivo `settings.json`
      con el formato de hoy, para que el paso de la ventana provisional al
      launcher no le cambie nada al jugador.
- [ ] C1. «Cargar juego» dentro del launcher: elegir la imagen del disco
      (el botón de explorar ya existe), comprobar el ID `G4AEE9`, extraer y
      **recompilar mostrando el progreso y el registro en la propia ventana**
      (el launcher ejecuta `moderngekko-port` y el compilador del paquete como
      procesos hijos y muestra su salida), recordar el disco, y dar mensajes de
      error claros. Reemplaza `Play.bat`, `Play-Keyboard.bat` y `play.ps1`.
- [ ] C2. Pestaña Mods: lista con casilla, descripción y opciones de cada mod
      (la velocidad del personaje con deslizador de 25 % a 300 %, el avance
      rápido de escenas), instalar un mod de la comunidad (`.mgm`)
      arrastrándolo y quitarlo. Reemplaza `Settings.ps1`.
- [ ] C3. Mods ya construidos: que el flujo de GitHub compile los mods
      oficiales con los mismos headers que el runner y los traiga listos en el
      paquete. El jugador deja de necesitar compilador para los mods y el
      inicio es más rápido; el compilador queda solo para recompilar el juego
      desde su disco, que es lo único que depende de sus datos. (Cambia la
      decisión de construirlos en la computadora del jugador, que permite
      probar un mod nuevo en la laptop sin reconstruir todo el paquete, unos
      30 minutos de GitHub.)
- [ ] C4. Controles (lo pidió el jugador el 2026-11-05): asignación
      automática según el tipo de control y una pestaña para reasignar todo.
      Hoy el launcher genera para cualquier control SDL un mismo perfil de
      GameCube fijo (`GCPadNew.ini`, un botón de SDL por cada botón del juego;
      se puede volver a generar, pero no cambiar botón por botón) y el
      teclado usa la asignación fija de Dolphin. Lo que se hará:
      - Reconocer el tipo de control con SDL (Xbox, PlayStation, Switch
        Pro y Joy-Con, 8BitDo, teclado) y escoger la distribución que le
        corresponde: en un control de Nintendo los botones A y B están en la
        posición contraria a la de uno de Xbox, y los gatillos L y R de
        GameCube son analógicos.
      - Asignar solo al conectar un control (y avisar al conectar o
        desconectar), recordar la elección de cada control por su
        identificador y volver al teclado si no hay ninguno.
      - Una pestaña para reasignar cada botón y cada eje de GameCube («pulsa
        el botón que quieres»), con vista en vivo del control, zonas muertas y
        sensibilidad de las palancas, perfiles con nombre y un botón para
        volver a la asignación automática.
      - Lo mismo para el teclado, incluida la tecla del avance rápido de
        escenas (la barra espaciadora).
      Reemplaza la asignación fija de teclado de Dolphin y el perfil generado.
- [ ] C5. Pestaña Ajustes: API gráfica, resolución, pantalla completa,
      relación de aspecto y widescreen, 60 cuadros por segundo y velocidad de
      carga, a medida que existan como mods o ajustes; idioma de la interfaz.
- [ ] C6. Un solo programa: `HarvestMoonMM.exe` es lo único que el jugador
      abre; el paquete queda con ese programa, `bin\`, `mods\` y `toolchain\`,
      sin `.bat` ni `.ps1`.
- [ ] C7. Distribución: avisos de Windows (hoy hay que desbloquear el zip; una
      firma digital los quitaría) y actualización desde el propio launcher.
- [ ] C8. Pruebas: de la lógica (archivo de ajustes, lista de mods, avance de la
      recompilación) en el flujo de pruebas, y de la interfaz con capturas de
      pantalla. SDL3 con el renderizador por software corre en Linux con una
      pantalla virtual (Xvfb), como ya se hace con el juego, así que gran parte
      se puede comprobar en el contenedor sin gastar minutos de GitHub; lo que
      depende de Windows ARM64 (compilador, DirectX 12) se prueba en la laptop.

**Mientras tanto (el puente):** `Settings.bat` y `play.ps1` siguen funcionando y
comparten el archivo `settings.json`; lo que el jugador elija hoy lo conserva
el launcher.

### Convenciones

Ver [CONTRIBUTING.md](../CONTRIBUTING.md): nombres que se explican solos,
comentarios que dicen el porqué, evidencia para cada nombre, un commit por
cambio en inglés, documentación bilingüe y nunca datos del juego.

---

## English

Priority: **the recompilation comes first** (track A). Track B (game mods)
starts now with the Jamie mod, because it works on the recompiled game;
everything track A discovers feeds into it. Track C (launcher) is the face of
the project: the application with which the player loads their game, turns
mods on and plays, as in other recompilations. It moves in parallel; today it
is loose PowerShell scripts.

**Decision (2026-11-05, corrected the same day): track A goes on.** The
player asked to remove Dolphin only if another GameCube or Wii recompilation
already does it. At first it was believed that none did, but there is one:
[WiiCompiled](https://github.com/patchzyy/wiicompiled), the static
recompilation of Mario Kart Wii (GPL-3.0, like this project), runs without an
emulator: it translates the game's code ahead of time and joins it to its own
high-level runtime (DVD, NAND, PAD, audio, video pacing and system services)
with [aurora](https://github.com/encounter/aurora) for the graphics, on Dawn
(D3D12, Vulkan, Metal). Dolphin was only its reference for the hardware.
Another recompilation is already built on it (New Super Mario Bros. Wii). It
is the same road that this track chose on 2026-11-04, so the order below
stays, now with WiiCompiled as the reference of a runtime that already works
with aurora; how much of it serves a GameCube game is still to be studied.
Meanwhile, the base that plays is still ModernGekko.
Several projects already use it as their base: New Super Mario Bros. Wii,
KartPad (iOS, macOS, Android), DriftDroid (Android) and WebCompiled (browser).
Its translator reads any DOL; what each game has to add is the runtime (audio,
input, GX, disc). First step: translate this game's `main.dol` with its
translator and take it to the title screen with its runtime, seeing what it
lacks for GameCube (memory card instead of NAND, no IOS).

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
   funnel of the game (`tools/find_callers.py`). Static analysis has already
   measured what is missing (75 GX functions that the game uses, of which
   aurora defines 71, and eight integration topics; see
   [RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md)) and found nothing
   that rules it out. Still to do: build aurora and see whether it runs on
   DirectX 12 on Windows ARM64 with a GPU. If the experiment fails, we
   decide again with what it taught us.
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
      274 `switch` tables, validated against `extabindex` (2,597/2,597).
      In-repo tools instead of dtk; see [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Names with evidence: CodeWarrior startup and the 13 SDK `Init`s.
- [x] Identify the SDK by signature (step 2 of the order above):
      `tools/sdk_signatures.py` compares every function with Dolphin's
      signature database and named 563 more (588 in total). It gave the range
      of each SDK library, which is the boundary of what the runtime replaces
      in phase 2; see [FUNCTION_MAP.md](FUNCTION_MAP.md).
- [x] Name the GX functions the game uses: `tools/find_library_entry_points.py`
      lists the 75 that the rest of the program reaches; the signature named
      49 and the other 26 were named by reading their code, each with a test
      against `main.dol` (614 names in total).
- [x] Name the other SDK entry points the game uses: 39 of OS, CARD, DVD, VI,
      AI, DSP, AR and PAD, and two internal CARD functions, were named by
      reading their code, each with a test against `main.dol` (655 names in
      total; see [NATIVE_RUNTIME.md](NATIVE_RUNTIME.md)).
- [x] Name the 27 unnamed entry points of the audio zone (AX, AXFX, MIX and
      two of DSP), which the tool only saw with a stretch given by hand; each
      with its test against `main.dol` (682 names in total).
- [ ] Refine the SDK names: use the linker archive that half of the database
      carries (for AX, TRK and the C library) and name `PPCArch` by its
      special-register number. Nothing waits on it.

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
- [x] Identify the SDK by signature (done in phase 0).
- [x] Static feasibility analysis: what the game asks of GX and what aurora has
      ([RECOMPILER_EVALUATION.md](RECOMPILER_EVALUATION.md)).
- [ ] Feasibility experiment: the game's GX calls draw the title screen with
      aurora. It needs aurora built and a PC with a GPU; how to get Dawn on
      Windows ARM64 has to be decided first.
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
- [ ] Player speed in a config file. Today it is chosen with `-PlayerSpeed`
      of `play.ps1` (or with the variable `HM_MM_PLAYER_SPEED_PERCENT`), see
      [MOVEMENT.md](MOVEMENT.md). Friction is not needed: when the stick is
      released the character stops in about 4 frames when walking and about
      9 when running.
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
- [x] Jamie mod v1: a hook on `0x80166888` that clears flag 230 once the
      ceremony has started (design in [SCRIPTS.md](SCRIPTS.md)). It is in
      [`mods/jamie_marriage`](../mods/jamie_marriage/mod.c), builds without
      warnings and was tried inside the game under hand-written conditions:
      without the mod the epilogue and the title screen follow; with it the
      game goes back to free play
      ([results](SCRIPTS.md#run-time-test-of-the-mod-2026-11-04)).
- [ ] Mods in the Windows package: `play.ps1` builds the active ones of
      `game.psd1` (`ModIds`) with the package's compiler, installs them in the
      player's folder and does not stop the game if they fail; `-NoMods` plays
      without them. It was tried on the laptop with Windows ARM64 (2026-11-05,
      with the new files copied over the package): the package's compiler
      built both mods and the runner loaded them (`mod loaded:` in
      `Logs\play.log`). What is left is the GitHub workflow, which checks that
      the runner loads each mod and has not been run again (it takes about 30
      minutes).
- [ ] Provisional settings window (`Settings.bat`): a check box for each mod
      and the character speed from 25 % to 300 % with a slider, with buttons
      to save and play. It keeps `settings.json` in the player's folder and
      `play.ps1` follows it (`-PlayerSpeed` wins for that one run; `-NoMods`
      ignores it). It is a bridge: track C replaces it with the launcher and
      keeps the same file. It was written without being able to run PowerShell
      in the container (the tests compare the files with each other and a
      parser checks the syntax); the first run on the laptop found an error
      with an empty text in a label, already fixed. It still has to be
      confirmed there.
- [ ] Try the Jamie mod with a real save: the actual proposal, save and load,
      a second proposal and both genders of the player. It needs a save close
      to the wedding with Jamie, which does not exist today: one is being made
      with the game's own save routine, for the player to try on their laptop.
- [ ] **Jamie lives in the house as a spouse** after the wedding, like any
      other partner of the game (asked for by the player on 2026-11-05). The
      original game does not do it: with Jamie the wedding ends the game, so
      there is no spouse routine for Jamie. Steps: (1) see what the game saves
      when you marry another partner (flag 161, the partner's id at
      `save+0x22B8`, value 137, flag 0, the double bed) and write the same for
      Jamie when the ceremony ends; (2) see whether that puts Jamie in the
      house, or whether Jamie has to be added as a character of the house
      (position, schedule, spouse dialogue, which may have to be borrowed from
      another partner). This is Jamie mod v2.
      **Step 1 done** (`jamie_marriage` 0.2.0): the code that starts the
      wedding of the other twenty partners (`fn_80166888`, from `0x801672B8`)
      makes seven writes in the save data: the spouse (`save+0x22B8`), flag 0,
      bit 3 of the character's bits (`save+0x5564 + 3·n`), two copies
      (`0x2234`→`0xD514`, `0x224C`→`0xD520`) and `0x1BC8 = 0`. The mod makes
      them for Jamie (character 2) when it clears flag 230. Tried in the game:
      after the field check the wedding's musical note comes
      ("Your life together starts today"). Step 2 is left: in the test, with
      the starting house, the spouse did not appear in the house, neither
      Jamie nor an ordinary partner; it has to be tried with the level 3 house
      (which the game requires for marrying).
- [ ] Fast-forward the scenes while a key is held (the space bar): a mod that
      speeds up the events and the key assignment (today Space is not mapped
      in Dolphin's keyboard layout). It is turned on and adjusted from the
      settings window and, later, from the launcher.
- [ ] Jamie mod v1.1: do not follow the ceremony with the notes scene (option
      A+ in [SCRIPTS.md](SCRIPTS.md): go to state 9 when the ceremony ends;
      what that state does still has to be checked).
- [ ] Turn the in-game test into a tool of the repository (`tools/`): today it
      is made of throwaway scripts. It is needed to repeat it whenever the
      runtime changes (steps 3 to 6 of the order above).
- [x] Player speed as a mod: [`mods/player_speed`](../mods/player_speed/mod.c)
      hooks the player's update and multiplies the horizontal velocity (150 %
      by default, from 25 % to 300 %), only in free play. It was measured
      inside the recompiled game: in a 15-frame walk the distance was exactly
      proportional to the factor at 100, 150 and 200 %. At 400 % the character
      went through the door of its house, which is why the maximum is 300 %.
      How the character moves and the results are in
      [MOVEMENT.md](MOVEMENT.md). Tried on the laptop with Windows ARM64
      (2026-11-05): it works, according to the player.
- [ ] Widescreen as a mod (or as Gecko codes for people using Dolphin): find
      the constants.
- [ ] Jamie mod v2 (content): spouse routine, dialogue, child events.

### Track C – Launcher and player experience (in parallel)

**Goal (asked for by the player on 2026-11-05):** a real application, like those
of other recompilations (for example Zelda64Recomp): a window of its own where
the game is loaded, mods are turned on and everything is adjusted, instead of
today's loose `.bat` and `.ps1` files.

**Decision: a launcher is not written from scratch.** ModernGekko already
brings one (`tools/moderngekko_launcher.cpp`, in C++ with SDL3 and Dear ImGui)
with disc image selection and extraction, controls, graphics API, resolution,
fullscreen and netplay; ours (`HarvestMoonMM.exe`) comes from it. What it lacks
is exactly what `play.ps1` does today: recompiling the game and handling the
mods. The interface does not depend on the Dolphin core (only the disc reading
uses its `DiscIO` today, which changes with step 4 of track A), so it survives
the change of runtime. A minimal patch that links code of our own in this
repository (`launcher/`) is preferred to a big patch on somebody else's code;
the ModernGekko clone is not touched.

- [ ] C0. Design: screens (Game, Mods, Controls, Settings, About and licenses),
      texts in Spanish and English, and a single `settings.json` file with
      today's format, so that going from the provisional window to the
      launcher changes nothing for the player.
- [ ] C1. "Load game" inside the launcher: choose the disc image (the browse
      button already exists), check the ID `G4AEE9`, extract and **recompile
      showing the progress and the log in the window itself** (the launcher
      runs `moderngekko-port` and the package's compiler as child processes
      and shows their output), remember the disc, and give clear error
      messages. It replaces `Play.bat`, `Play-Keyboard.bat` and `play.ps1`.
- [ ] C2. Mods tab: a list with a check box, description and options for each
      mod (the character speed with a slider from 25 % to 300 %, the scene
      fast-forward), install a community mod (`.mgm`) by dragging it in, and
      remove it. It replaces `Settings.ps1`.
- [ ] C3. Prebuilt mods: the GitHub workflow compiles the official mods with
      the same headers as the runner and ships them ready in the package. The
      player no longer needs a compiler for the mods and starting is faster;
      the compiler is left only for recompiling the game from their disc, the
      only thing that depends on their data. (It changes the decision to build
      them on the player's computer, which lets a new mod be tried on the
      laptop without rebuilding the whole package, about 30 minutes of GitHub.)
- [ ] C4. Controls (asked for by the player on 2026-11-05): automatic
      assignment according to the type of controller, and a tab to remap
      everything. Today the launcher generates for any SDL controller the same
      fixed GameCube profile (`GCPadNew.ini`, one SDL button for each button of
      the game; it can be generated again, but not changed button by button)
      and the keyboard uses Dolphin's fixed assignment. What will be done:
      - Recognise the type of controller with SDL (Xbox, PlayStation, Switch
        Pro and Joy-Con, 8BitDo, keyboard) and pick the layout that suits it:
        on a Nintendo controller the A and B buttons are in the opposite
        position to those of an Xbox one, and the GameCube L and R triggers
        are analog.
      - Assign only when a controller is connected (and say so when one is
        connected or disconnected), remember the choice for each controller by
        its identifier, and go back to the keyboard if there is none.
      - A tab to remap each GameCube button and axis ("press the button you
        want"), with a live view of the controller, dead zones and stick
        sensitivity, named profiles and a button to go back to the automatic
        assignment.
      - The same for the keyboard, including the scene fast-forward key (the
        space bar).
      It replaces Dolphin's fixed keyboard assignment and the generated
      profile.
- [ ] C5. Settings tab: graphics API, resolution, fullscreen, aspect ratio and
      widescreen, 60 frames per second and load speed, as they come to exist as
      mods or settings; language of the interface.
- [ ] C6. A single program: `HarvestMoonMM.exe` is the only thing the player
      opens; the package is left with that program, `bin\`, `mods\` and
      `toolchain\`, with no `.bat` or `.ps1`.
- [ ] C7. Distribution: Windows warnings (today the zip has to be unblocked; a
      digital signature would remove them) and updating from the launcher
      itself.
- [ ] C8. Tests: of the logic (settings file, list of mods, progress of the
      recompilation) in the test workflow, and of the interface with
      screenshots. SDL3 with the software renderer runs on Linux with a
      virtual screen (Xvfb), as we already do with the game, so much of it can
      be checked in the container without spending GitHub minutes; what
      depends on Windows ARM64 (compiler, DirectX 12) is tried on the laptop.

**Meanwhile (the bridge):** `Settings.bat` and `play.ps1` keep working and
share the `settings.json` file; whatever the player chooses today is kept by
the launcher.

### Conventions

See [CONTRIBUTING.md](../CONTRIBUTING.md): self-explaining names, comments
that say why, evidence for every name, one English commit per change,
bilingual docs and never any game data.
