# Mods / Mods

[Español](#español) · [English](#english)

---

## Español

Un mod cambia el juego recompilado sin tocar su código original. Es una
biblioteca compartida (`mod.so`) que ModernGekko carga al arrancar: ahí
declaras qué funciones del juego **reemplazas** (*patch*) o **observas**
(*hook*), y tu código corre en su lugar o junto a ellas.

### Compilar e instalar

Corre primero `scripts/build_and_run_linux.sh` una vez: descarga ModernGekko,
cuyos headers necesitan los mods. Después:

```sh
scripts/build_mods.sh                 # compila todos los mods en build/mods
scripts/build_mods.sh boot_logger     # compila e instala boot_logger
scripts/build_and_run_linux.sh        # abre el juego con los mods instalados
```

`build_mods.sh` copia cada mod nombrado a `~/.local/share/moderngekko/Mods/`
(`MODERNGEKKO_USER_DIR` cambia esa carpeta). Para probar un mod sin instalarlo,
`moderngekko-run` acepta `--mods build/mods`.

Un mod instalado es una carpeta `<id>.mgm/` con un `mod.so` dentro (`mod.dll` en
Windows). Para borrar un mod, borra su carpeta.

En el paquete de Windows no hace falta nada de esto: `play.ps1` construye con
el compilador del paquete los mods que enumera `ModIds` en `game.psd1` y los
instala en `%LOCALAPPDATA%\HarvestMoonMM\Mods` (ver
[`packaging/windows/README.md`](../packaging/windows/README.md)). Un mod nuevo
entra al paquete cuando se agrega a `CMakeLists.txt` y a `ModIds`.

### Cómo está hecho un mod

El ejemplo completo y comentado es [`boot_logger/mod.c`](boot_logger/mod.c).
Todo mod tiene tres partes:

1. **Las funciones del mod**, que reciben el estado de la CPU del juego
   (`CPUState*`): los registros `gpr[]`, `fpr[]`, `lr` y la memoria.
2. **Una tabla de patches y hooks**, que dice en qué dirección del juego se
   engancha cada función. Las direcciones se escriben con nombre:
   `RECOMP_PATCH(DOLRECOMP_SYMBOL_OSInit, mi_funcion)`.
3. **El descriptor** (`ModernGekkoModDesc`) que `moderngekko_get_mod()`
   devuelve: identificador, versión, `game_id` (`G4AEE9`) y las tablas.

| Macro | Qué hace |
|---|---|
| `RECOMP_PATCH(dirección, f)` | Reemplaza la función del juego por `f`. Si dos mods parchean la misma dirección, el segundo se rechaza. |
| `RECOMP_FORCE_PATCH(dirección, f)` | Igual, pero gana aunque otro mod ya haya parcheado esa dirección. |
| `RECOMP_HOOK(dirección, f)` | Corre `f` al entrar a la función del juego, que después sigue normal. |
| `RECOMP_HOOK_RETURN(dirección, f)` | Corre `f` cuando la función del juego termina. |

Ayudas para escribir las funciones (de `moderngekko/mod_abi.h`):

- `moderngekko_mod_arg_u32(cpu, n)`: el argumento `n` de la función del juego
  (registro `r3 + n`).
- `moderngekko_mod_return_u32(cpu, valor)`: deja `valor` en `r3` y regresa al
  que llamó. Un *patch* debe terminar así (o poniendo `cpu->pc = cpu->lr`);
  si no, el juego no sabe a dónde volver.
- `moderngekko_mod_read(cpu, dirección, tamaño)` y
  `moderngekko_mod_write(cpu, dirección, valor, tamaño)`: leen y escriben la
  memoria del juego (`tamaño` en bytes: 1, 2, 4 u 8).
- `HM_MM_LOG(id_del_mod, "texto %d", valor)` (de `include/hm_mm_mod.h`):
  imprime una línea con `[id_del_mod]` en la terminal donde corre el juego.

### Nombres de funciones

`build_mods.sh` genera `build/G4AEE9_symbols.h` a partir de
`config/G4AEE9/symbols.txt` con una macro `DOLRECOMP_SYMBOL_<nombre>` por
función. Así un mod dice `DOLRECOMP_SYMBOL_PADInit` en vez de `0x8017C890`.
Cuando descubras qué hace una función, ponle nombre en `symbols.txt` (con la
evidencia, como pide [CONTRIBUTING.md](../CONTRIBUTING.md)) y vuelve a
compilar.

### Cuidados que hay que tener

- **Un mod hace más lento el código que toca.** El juego recompilado corre en
  bloques ("chunks") de 16 KB. Si un mod engancha una dirección, ModernGekko
  ejecuta ese bloque entero con su CPU de respaldo en vez del código nativo, y
  lo avisa en el registro: `[staticrecomp] mod fallback: chunk [...)`. Engancha
  lo mínimo, y evita funciones que el juego llama miles de veces por segundo.
- **Un hook no puede cambiar registros.** ModernGekko guarda el estado de la
  CPU antes del hook y lo restaura después. Lo que un hook escribe en la
  *memoria* del juego (`moderngekko_mod_write`) sí se queda. Para cambiar lo
  que una función devuelve o hace, usa un *patch*.
- **Un hook puede correr más de una vez por llamada.** En la prueba con
  `boot_logger`, una sola llamada a `OSInit` imprimió su línea 2 veces (mismo
  `lr`, misma pila) y una sola llamada a `GXInit` la imprimió 6. ModernGekko
  consulta los mods cada vez que su despachador vuelve a entrar a la dirección
  enganchada, y con el bloque de respaldo eso puede ocurrir varias veces antes
  de que la función avance. Escribe los hooks de modo que repetirlos no haga
  daño: que solo lean, o que siempre escriban el mismo valor; nunca que cuenten
  llamadas ni acumulen. Un *patch* reemplaza la función y regresa al que
  llamó, así que no debería repetirse, pero todavía no hay un mod con *patch*
  que lo confirme dentro del juego.
- **Un hook no corre si se llega a su dirección desde su mismo bloque.**
  DolRecomp corta cada sección de código en bloques de 4096 instrucciones
  (16 KB) desde el inicio de la sección; para el código del juego, desde
  `0x800184C0`. En el código generado, un salto o una llamada dentro del mismo
  bloque es un `goto`, y solo uno que sale del bloque vuelve al despachador,
  que es donde ModernGekko consulta los mods. Un hook de entrada en
  `fn_8001AC20`, que el juego ejecuta en cada cuadro, no corrió ni una vez en
  1 200 cuadros: su único llamador, `fn_80019AA4`, está en el mismo bloque
  (`tools/find_callers.py` lo muestra) y la llamada de `0x80019D18` es un
  `goto label_8001AC20`. Un hook en `fn_80049D28`, a la que se llama desde
  otro bloque, corrió en cada llamada. Vale para cualquier dirección, no solo
  para el inicio de una función: `jamie_marriage` engancha la primera
  instrucción de la revisión del embarazo (`0x80169410`), a la que se salta
  desde otro bloque, y sus hooks en los bloques de las cadenas, a los que se
  salta desde el mismo, nunca corrieron. Antes de escribir un mod, mira desde
  dónde se llega a la dirección; si todo llega desde su bloque, engancha
  otra.

### Mods del proyecto

| Mod | Estado | Qué hace |
|---|---|---|
| [`boot_logger`](boot_logger/mod.c) | Listo | Prueba de humo: imprime una línea cada vez que el juego entra a `OSInit`, `DVDInit`, `VIInit`, `PADInit`, `GXInit` y `main`. No cambia nada. |
| [`state_logger`](state_logger/mod.c) | Listo | Imprime cada cambio de estado de la máquina de estados del juego (título, día nuevo, eventos, juego libre), cada estado que otro código pide y cada vez que entra la revisión del campo. Sirve para saber cuándo corre un gancho. No cambia nada. |
| [`jamie_marriage`](jamie_marriage/mod.c) | Probado en el juego con una partida real; las frases de cónyuge, con condiciones escritas a mano; el embarazo, el parto y los días especiales, también con una copia de una partida real | Casarse con Jamie ya no termina el juego: si la ceremonia ya empezó y la bandera 230 sigue encendida, la apaga en la revisión del campo y anota la boda como el juego lo hace con las demás parejas (diseño en [SCRIPTS.md](../docs/SCRIPTS.md)); tras la ceremonia se ve una vez la escena de las notas. Jamie vive en la casa como cónyuge (0.4.0) y, desde 0.5.0, habla como cónyuge: 33 frases propias en lugar de sus frases de rival, en los cinco idiomas cuyo texto trae el disco ([Las conversaciones](../docs/SCRIPTS.md#las-conversaciones)). Desde 0.6.0 tiene cumpleaños: el 22 de invierno. Desde 0.7.0 hay embarazo y parto: Jamie toma prestadas las escenas de otra pareja, con su modelo y frases nuevas en los cinco idiomas, para el chico y para la chica ([El embarazo y el parto](../docs/SCRIPTS.md#el-embarazo-y-el-parto)). Desde 0.8.0, los días especiales: el aniversario, el cumpleaños del hijo y su revisión médica, también con frases nuevas de Jamie ([Los días especiales](../docs/SCRIPTS.md#los-días-especiales)). |
| [`player_speed`](player_speed/mod.c) | Probado en el juego, en la nube; falta Windows | El personaje camina y corre más rápido: engancha la actualización del jugador (`fn_80049D28`) y multiplica la velocidad horizontal que el juego guardó el cuadro anterior, solo en juego libre. Por omisión al 100 % (sin cambio); `HM_MM_PLAYER_SPEED_PERCENT` (de 25 a 300) lo cambia, y en el paquete de Windows `play.ps1 -PlayerSpeed`. Con 400 el personaje atravesó la puerta de su casa; por encima de 200 queda bajo el propio riesgo del jugador. No cambia la animación. Cómo se mueve el personaje: [MOVEMENT.md](../docs/MOVEMENT.md). |
| [`carry_speed`](carry_speed/mod.c) | Probado en el juego, en la nube, con el estado de cargar forzado; falta levantar algo de verdad y Windows | Con algo en las manos el personaje solo camina, porque el juego no tiene cambio a correr desde sus estados de cargar (`0x25`–`0x28`, `0x38`–`0x3B`, `0x46`). Engancha la misma función que `player_speed` y, con el stick a fondo (54,5 o más), alarga la velocidad 5,0 / 3,0 veces. Medido: el paso por cuadro en el estado `0x25` pasó de 0,0928 a 0,1547 (1,667 veces). Corre después de `player_speed` y funciona con él. |
| [`far_camera`](far_camera/mod.c) | Probado en el juego, en la nube; falta Windows y un control de verdad | El juego acerca y aleja la cámara con la cruceta, entre 10.0 y 20.0 (un float de la cámara que actualiza `fn_80031580`). El mod deja seguir alejándola más allá de 20.0: mientras la cámara está en el tope le presta al juego un valor un paso abajo y, por lo que el juego hizo con él, sabe si la cruceta fue hacia abajo o hacia arriba. La cantidad que resulta la lee el runtime nativo, que ensancha la imagen (`runtime/src/graphics/far_camera.h`). Solo al aire libre. |
| [`longer_days`](longer_days/mod.c) | Con pruebas que corren su hook como lo corre el juego, cuadro por cuadro ([tests/test_longer_days.py](../tests/test_longer_days.py)); falta probarlo en el juego | Cada cuadro de juego libre, `fn_80038088` llama a `fn_80037EAC` con `f1` = 100 por el tiempo del cuadro; `fn_80037EAC` le quita los decimales y lo suma a la parte de minuto (`save + 0x2270`), de donde avanzan el minuto (`0x2264`) y la hora (`0x2258`). El mod engancha la entrada de `fn_80037EAC` y le da lo que el juego sumaría dividido entre la duración del día elegida (`HM_MM_DAY_LENGTH_PERCENT`, de 100 a 400), guardando lo que sobra para los cuadros siguientes. Dormir, los eventos y el inicio del día no pasan por ahí. |
| [`gentle_stamina`](gentle_stamina/mod.c) | Con pruebas que corren su hook como lo corre el juego ([tests/test_gentle_stamina.py](../tests/test_gentle_stamina.py)); falta probarlo en el juego | Todo cambio de estamina pasa por `fn_800A4D8C(partida, jugador, cambio)`: lo multiplica por 1, 2 o 4 según el cansancio (`partida + 0x4040`), lo suma a la estamina (`partida + 0x4004`, 12 bytes por jugador) y la deja entre sus límites (el máximo en `0x3FC8`). Las herramientas, la pesca y correr llaman con -1 a -5. El mod engancha la entrada y hace más chico un cambio negativo (`HM_MM_STAMINA_COST_PERCENT`, de 10 a 100), guardando lo que sobra de un punto para la siguiente vez. |
| [`faster_tools`](faster_tools/mod.c) | Con pruebas que corren su hook como lo corre el juego ([tests/test_faster_tools.py](../tests/test_faster_tools.py)); falta probarlo en el juego | Cada herramienta tiene un contador de experiencia en la partida (`partida + 0xCD1C` en adelante, 12 bytes cada uno, hasta 10000) y usarla le suma 1 con `fn_800A78D0(contador, cantidad)`, la misma función que suma a otros contadores del juego. El mod la engancha y, si `r3` es el contador de una herramienta, multiplica la cantidad (`HM_MM_TOOL_EXPERIENCE_PERCENT`, de 100 a 400), guardando lo que sobra de un punto. El traductor copia esa función dentro de sus 40 llamadas; `tools/insert_mod_hooks.py` escribe el hook también en cada copia. El sexto contador (`0xCD58`) se sube en `fn_8004E244` con otra función y todavía no cambia. |
| [`cheats`](cheats/mod.c) | Con pruebas sobre una memoria falsa ([tests/test_cheats.py](../tests/test_cheats.py)); en el juego se vio cargar y llegar a su hook, falta ver cada truco | Los trucos de la pestaña Trucos. No aparece en la pestaña Mods (`"hidden"` en su `mod.json`): el runtime lo carga cuando hay un truco encendido (`settings.json`, `cheats`) y le pasa sus nombres en `HM_MM_CHEATS`. Estamina infinita: en `fn_800A4D8C` un cambio negativo se vuelve 0. Dinero: en juego libre, el dinero (`partida + 0x245C`) se mantiene en 999,999. Detener el tiempo: `fn_80037EAC` suma 0. Amistad: en juego libre, cada amistad (`partida + 0x6628 + 12·n`) se pone en su máximo. |
| [`marry_anyone`](marry_anyone/mod.c) | Probado en el juego, en la nube: la boda del chico con Basil y la adopción (la Diosa y el duende); los ganchos y el cambio de palabras, con pruebas sobre una memoria falsa y los mensajes del disco ([tests/test_marry_anyone.py](../tests/test_marry_anyone.py)); falta Windows | Las veinte parejas se pueden cortejar y casar, sea cual sea el personaje del jugador. La pluma azul funciona con todas (el built-in 189 pregunta por una pareja del personaje del jugador), la pareja más cercana y el consejo del pueblo cuentan a las veinte, la revisión del campo da la segunda escena de amor y la boda de la cadena de esa pareja, y el guion de la boda viste al jugador como es (72 el traje del chico, 73 el vestido de la chica). No empiezan las escenas de rivales de la pareja. Una pareja del mismo sexo no tiene embarazo: desde 0.2.0, el parto se vuelve una escena nueva en el manantial, donde la Diosa de la Cosecha (o un duende, si sigue de piedra) les da un bebé en adopción; su script y sus líneas en cinco idiomas los escribe [tools/make_adoption_scene.py](../tools/make_adoption_scene.py) ([La adopción](../docs/SCRIPTS.md#la-adopción)). Las primeras palabras del cónyuge (mensajes 9 y 100) cambian «esposo» o «esposa», «Sra.», «casados» y palabras así en los cinco idiomas. La partida lo sigue usando si se casó o se comprometió con alguien de su mismo sexo. |

### En el runtime nativo

El runtime nativo (`runtime/`, sin Dolphin) compila los mods dentro del
programa en lugar de cargarlos como bibliotecas: el mismo `mod.c`, con
`runtime/mod_api/moderngekko/mod_abi.h` en lugar del header de ModernGekko,
que declara los mismos nombres. Ahí funcionan los *patch* y los *hook* (de
entrada, de retorno y en una instrucción dentro de una función), los registros
y la memoria; las exportaciones, importaciones y eventos se ignoran.

Como las funciones traducidas se llaman directamente entre sí, los lugares
donde actúa cada mod se marcan en el código traducido al compilar:
`tools/insert_mod_hooks.py` lee las tablas de los mods y agrega las llamadas.
`scripts/build_native_runtime.sh` lo hace solo para los mods de `NATIVE_MODS`
(por omisión `jamie_marriage player_speed carry_speed far_camera marry_anyone longer_days gentle_stamina faster_tools cheats`). Si el traductor copió en línea una función
enganchada dentro de otra, el hook de entrada se escribe también en cada copia
(su etiqueta es `loc_inl<N>_0x<dirección>`); un hook de retorno no puede, y la
herramienta se detiene si pasa.
El registro del runtime dice qué mods cargó y la primera vez que el juego
llega a cada lugar donde actúan.

Cada uno de esos mods tiene además un manifiesto, `mod.json`, que es lo que
muestra el launcher de la versión nativa en su pantalla de mods: el nombre, una
línea, la descripción y un aviso opcional, en español y en inglés, la versión
(la misma que su `mod.c`), los autores, su categoría y sus ajustes. Los mods con
`"category": "quality_of_life"` (`player_speed`, `carry_speed`, `far_camera`,
`longer_days`, `gentle_stamina` y `faster_tools`), que solo hacen el juego más
cómodo, salen al final bajo «Calidad de vida», en el launcher y en el menú del
juego (F10); en el perfil de RecompLauncher son el grupo «Quality of life». Un
ajuste es un
porcentaje que el launcher guarda en `settings.json` y le pasa al juego en una
variable de entorno, que el mod lee al empezar (el de `player_speed`:
`HM_MM_PLAYER_SPEED_PERCENT`). `tests/test_mod_manifests.py` revisa que
coincidan con los mods.

---

## English

A mod changes the recompiled game without touching its original code. It is a
shared library (`mod.so`) that ModernGekko loads at start-up: in it you
declare which game functions you **replace** (*patch*) or **watch** (*hook*),
and your code runs in their place or beside them.

### Build and install

Run `scripts/build_and_run_linux.sh` once first: it downloads ModernGekko,
whose headers the mods need. Then:

```sh
scripts/build_mods.sh                 # build every mod into build/mods
scripts/build_mods.sh boot_logger     # build and install boot_logger
scripts/build_and_run_linux.sh        # start the game with the installed mods
```

`build_mods.sh` copies each named mod to `~/.local/share/moderngekko/Mods/`
(`MODERNGEKKO_USER_DIR` moves that folder). To try a mod without installing
it, `moderngekko-run` accepts `--mods build/mods`.

An installed mod is a folder `<id>.mgm/` with a `mod.so` inside (`mod.dll` on
Windows). To remove a mod, delete its folder.

None of this is needed in the Windows package: `play.ps1` builds, with the
package's compiler, the mods that `ModIds` lists in `game.psd1` and installs
them in `%LOCALAPPDATA%\HarvestMoonMM\Mods` (see
[`packaging/windows/README.md`](../packaging/windows/README.md)). A new mod
joins the package when it is added to `CMakeLists.txt` and to `ModIds`.

### How a mod is built

The complete, commented example is [`boot_logger/mod.c`](boot_logger/mod.c).
Every mod has three parts:

1. **The mod's functions**, which receive the game CPU's state (`CPUState*`):
   the `gpr[]`, `fpr[]` and `lr` registers, and memory.
2. **A table of patches and hooks**, saying at which game address each function
   attaches. Addresses are written by name:
   `RECOMP_PATCH(DOLRECOMP_SYMBOL_OSInit, my_function)`.
3. **The descriptor** (`ModernGekkoModDesc`) that `moderngekko_get_mod()`
   returns: id, version, `game_id` (`G4AEE9`) and the tables.

| Macro | What it does |
|---|---|
| `RECOMP_PATCH(address, f)` | Replaces the game function with `f`. If two mods patch the same address, the second is rejected. |
| `RECOMP_FORCE_PATCH(address, f)` | The same, but wins even if another mod already patched that address. |
| `RECOMP_HOOK(address, f)` | Runs `f` when the game function is entered; the function then carries on as usual. |
| `RECOMP_HOOK_RETURN(address, f)` | Runs `f` when the game function finishes. |

Helpers for writing the functions (from `moderngekko/mod_abi.h`):

- `moderngekko_mod_arg_u32(cpu, n)`: argument `n` of the game function
  (register `r3 + n`).
- `moderngekko_mod_return_u32(cpu, value)`: leaves `value` in `r3` and returns
  to the caller. A *patch* must end this way (or by setting
  `cpu->pc = cpu->lr`); otherwise the game does not know where to go back to.
- `moderngekko_mod_read(cpu, address, size)` and
  `moderngekko_mod_write(cpu, address, value, size)`: read and write the
  game's memory (`size` in bytes: 1, 2, 4 or 8).
- `HM_MM_LOG(mod_id, "text %d", value)` (from `include/hm_mm_mod.h`): prints a
  line tagged `[mod_id]` in the terminal the game runs in.

### Function names

`build_mods.sh` writes `build/G4AEE9_symbols.h` from
`config/G4AEE9/symbols.txt`, with one `DOLRECOMP_SYMBOL_<name>` macro per
function. A mod says `DOLRECOMP_SYMBOL_PADInit` instead of `0x8017C890`. When
you find out what a function does, name it in `symbols.txt` (with the
evidence, as [CONTRIBUTING.md](../CONTRIBUTING.md) asks) and build again.

### Things to be careful about

- **A mod slows down the code it touches.** The recompiled game runs in 16 KB
  blocks ("chunks"). If a mod attaches to an address, ModernGekko runs that
  whole block with its fallback CPU instead of native code, and says so in the
  log: `[staticrecomp] mod fallback: chunk [...)`. Attach to as little as
  possible, and avoid functions the game calls thousands of times a second.
- **A hook cannot change registers.** ModernGekko saves the CPU state before
  the hook and restores it afterwards. What a hook writes to the game's
  *memory* (`moderngekko_mod_write`) does stay. To change what a function does
  or returns, use a *patch*.
- **A hook can run more than once per call.** In the test with `boot_logger`,
  a single call to `OSInit` printed its line twice (same `lr`, same stack) and
  a single call to `GXInit` printed it six times. ModernGekko asks the mods
  every time its dispatcher re-enters the hooked address, and with the
  fallback block that can happen several times before the function moves on.
  Write hooks so that repeating them does no harm: they only read, or always
  write the same value; never count calls or accumulate. A *patch* replaces
  the function and returns to the caller, so it should not repeat, but there
  is not yet a mod with a *patch* that confirms this inside the game.
- **A hook does not run when its address is reached from its own block.**
  DolRecomp cuts each code section into blocks of 4096 instructions (16 KB)
  from the start of the section; for the game's code, from `0x800184C0`. In
  the generated code, a branch or a call within the same block is a `goto`,
  and only one that leaves the block returns to the dispatcher, which is
  where ModernGekko asks the mods. An entry hook on `fn_8001AC20`, which the
  game runs every frame, did not run once in 1,200 frames: its only caller,
  `fn_80019AA4`, is in the same block (`tools/find_callers.py` shows it) and
  the call at `0x80019D18` is a `goto label_8001AC20`. A hook on
  `fn_80049D28`, which is called from another block, ran on every call. It
  holds for any address, not only the start of a function: `jamie_marriage`
  hooks the first instruction of the pregnancy check (`0x80169410`), reached
  by a branch from another block, and its hooks on the blocks of the chains,
  reached from the same block, never ran. Before writing a mod, look at
  where the address is reached from; if everything comes from its block,
  hook another one.

### The project's mods

| Mod | Status | What it does |
|---|---|---|
| [`boot_logger`](boot_logger/mod.c) | Done | Smoke test: prints a line each time the game enters `OSInit`, `DVDInit`, `VIInit`, `PADInit`, `GXInit` and `main`. Changes nothing. |
| [`state_logger`](state_logger/mod.c) | Done | Prints every change of the game's state machine (title, new day, events, free play), every state other code asks for, and each time the field check is entered. Useful to learn when a hook will run. Changes nothing. |
| [`jamie_marriage`](jamie_marriage/mod.c) | Tried in the game with a real save; the spouse lines under hand-written conditions; the pregnancy, the birth and the special days also with a copy of a real save | Marrying Jamie no longer ends the game: if the ceremony has already started and flag 230 is still set, it clears the flag in the field check and records the wedding the way the game does for the other partners (design in [SCRIPTS.md](../docs/SCRIPTS.md)); the notes scene plays once after the ceremony. Jamie lives in the house as the spouse (0.4.0) and, since 0.5.0, talks like a spouse: 33 lines of its own instead of the rival's lines, in the five languages whose text is on the disc ([The conversations](../docs/SCRIPTS.md#the-conversations)). Since 0.6.0 Jamie has a birthday: the 22nd of winter. Since 0.7.0 there are a pregnancy and a birth: Jamie borrows another partner's scenes, with Jamie's model and new lines in the five languages, for the boy and for the girl ([Pregnancy and birth](../docs/SCRIPTS.md#pregnancy-and-birth)). Since 0.8.0, the special days: the anniversary, the child's birthday and its checkup, also with new lines from Jamie ([Special days](../docs/SCRIPTS.md#special-days)). |
| [`player_speed`](player_speed/mod.c) | Tried in the game, in the cloud; Windows still to do | The character walks and runs faster: it hooks the player's update (`fn_80049D28`) and multiplies the horizontal velocity that the game stored one frame earlier, only in free play. By default at 100% (no change); `HM_MM_PLAYER_SPEED_PERCENT` (25 to 300) changes it, and in the Windows package so does `play.ps1 -PlayerSpeed`. At 400 the character went through the door of its house; above 200 it is at the player's own risk. It does not change the animation. How the character moves: [MOVEMENT.md](../docs/MOVEMENT.md). |
| [`carry_speed`](carry_speed/mod.c) | Tried in the game, in the cloud, with the carrying state forced; picking something up for real and Windows still to do | With something in the hands the character can only walk, because the game has no change to running from its carrying states (`0x25`–`0x28`, `0x38`–`0x3B`, `0x46`). It hooks the same function as `player_speed` and, with the stick fully pushed (54.5 or more), makes the velocity 5.0 / 3.0 times longer. Measured: the step per frame in state `0x25` went from 0.0928 to 0.1547 (1.667 times). It runs after `player_speed` and works with it. |
| [`far_camera`](far_camera/mod.c) | Tried in the game, in the cloud; Windows and a real controller still to do | The game brings the camera closer and pulls it back with the D-pad, between 10.0 and 20.0 (a float of the camera that `fn_80031580` updates). The mod lets it go on past 20.0: while the camera is at the limit it lends the game a value one step below it and, from what the game made of it, knows whether the D-pad went down or up. The native runtime reads the resulting amount and widens the picture (`runtime/src/graphics/far_camera.h`). Only outdoors. |
| [`longer_days`](longer_days/mod.c) | With tests that run its hook as the game does, frame by frame ([tests/test_longer_days.py](../tests/test_longer_days.py)); not tried in the game yet | Every frame of free play, `fn_80038088` calls `fn_80037EAC` with `f1` = 100 * the frame's time; `fn_80037EAC` cuts the decimals off and adds it to the part of a minute (`save + 0x2270`), from which the minute (`0x2264`) and the hour (`0x2258`) go on. The mod hooks the entry of `fn_80037EAC` and gives it what the game would add divided by the chosen length of the day (`HM_MM_DAY_LENGTH_PERCENT`, from 100 to 400), keeping what is left over for the next frames. Sleeping, the events and the start of the day do not go through it. |
| [`gentle_stamina`](gentle_stamina/mod.c) | With tests that run its hook as the game does ([tests/test_gentle_stamina.py](../tests/test_gentle_stamina.py)); not tried in the game yet | Every change of stamina goes through `fn_800A4D8C(save, player, change)`: it multiplies it by 1, 2 or 4 by how tired the player is (`save + 0x4040`), adds it to the stamina (`save + 0x4004`, 12 bytes per player) and keeps it within its limits (the most at `0x3FC8`). The tools, fishing and running call it with -1 to -5. The mod hooks its entry and makes a negative change smaller (`HM_MM_STAMINA_COST_PERCENT`, from 10 to 100), keeping what is left of a point for the next time. |
| [`faster_tools`](faster_tools/mod.c) | With tests that run its hook as the game does ([tests/test_faster_tools.py](../tests/test_faster_tools.py)); not tried in the game yet | Each tool has an experience counter in the save data (from `save + 0xCD1C` on, 12 bytes each, up to 10000), and using it adds 1 with `fn_800A78D0(counter, amount)`, the same function that adds to other counters of the game. The mod hooks it and, when `r3` is a tool's counter, multiplies the amount (`HM_MM_TOOL_EXPERIENCE_PERCENT`, from 100 to 400), keeping what is left of a point. The translator copies that function into its 40 calls; `tools/insert_mod_hooks.py` writes the hook into each copy too. The sixth counter (`0xCD58`) is raised in `fn_8004E244` with another function and is not changed yet. |
| [`cheats`](cheats/mod.c) | With tests on a fake memory ([tests/test_cheats.py](../tests/test_cheats.py)); in the game it was seen to load and reach its hook, each cheat still to be seen | The cheats of the Cheats tab. It is not in the Mods tab (`"hidden"` in its `mod.json`): the runtime loads it when a cheat is on (`settings.json`, `cheats`) and gives it their names in `HM_MM_CHEATS`. Infinite stamina: in `fn_800A4D8C` a negative change becomes 0. Money: in free play, the money (`save + 0x245C`) stays at 999,999. Stop time: `fn_80037EAC` adds 0. Friendship: in free play, each friendship (`save + 0x6628 + 12·n`) is set to its maximum. |
| [`marry_anyone`](marry_anyone/mod.c) | Tried in the game, in the cloud: the boy's wedding with Basil and the adoption (the Goddess and the sprite); the hooks and the swap of words, with tests on a fake memory and on the disc's messages ([tests/test_marry_anyone.py](../tests/test_marry_anyone.py)); Windows still to do | All twenty partners can be courted and married, whichever character the player chose. The blue feather works with all of them (built-in 189 asks about a partner of the player's character), the closest partner and the village's advice count all twenty, the field check gives the second love scene and the wedding of that partner's chain, and the wedding script dresses the player as the player is (72 the boy's suit, 73 the girl's dress). The partner's rival scenes do not start. A couple of the same sex has no pregnancy: since 0.2.0 the birth becomes a new scene at the spring, where the Harvest Goddess (or a sprite, while she is stone) gives them a baby to adopt; its script and its lines in five languages are written by [tools/make_adoption_scene.py](../tools/make_adoption_scene.py) ([The adoption](../docs/SCRIPTS.md#the-adoption)). The spouse's first words (messages 9 and 100) change "husband" or "wife", "Mrs.", "married" and such words in the five languages. A save keeps using it once the player is engaged or married to someone of the same sex. |

### In the native runtime

The native runtime (`runtime/`, without Dolphin) compiles the mods into the
program instead of loading them as libraries: the same `mod.c`, with
`runtime/mod_api/moderngekko/mod_abi.h` in place of ModernGekko's header,
which declares the same names. Patches and hooks (at entry, at return and at
an instruction inside a function), the registers and the memory work there;
exports, imports and events are ignored.

As the translated functions call each other directly, the places where each
mod acts are marked in the translated code when it is built:
`tools/insert_mod_hooks.py` reads the mods' tables and adds the calls.
`scripts/build_native_runtime.sh` does it for the mods in `NATIVE_MODS` (by
default `jamie_marriage player_speed carry_speed far_camera marry_anyone longer_days gentle_stamina faster_tools cheats`). If the translator copied a hooked
function inline into another one, the entry hook is written into each copy too
(its label is `loc_inl<N>_0x<address>`); a return hook cannot be, and the tool
stops if that happens. The
runtime's log says which mods it loaded and the first time the game reaches
each place where they act.

Each of those mods also has a manifest, `mod.json`, which is what the native
version's launcher shows on its mods screen: the name, a line, the description
and an optional warning, in Spanish and in English, the version (the same as
its `mod.c`), the authors, its category and its settings. The mods with
`"category": "quality_of_life"` (`player_speed`, `carry_speed`, `far_camera`,
`longer_days`, `gentle_stamina` and `faster_tools`), which only make the game
more comfortable, come last under "Quality of life", in the launcher and in the
game's menu (F10); in the RecompLauncher profile they are the "Quality of life"
group. A setting is a percentage that the
launcher keeps in `settings.json` and passes to the game in an environment
variable, which the mod reads when it starts (`player_speed`'s:
`HM_MM_PLAYER_SPEED_PERCENT`). `tests/test_mod_manifests.py` checks that they
agree with the mods.
