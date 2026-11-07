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
| [`player_speed`](player_speed/mod.c) | Probado en el juego, en la nube; falta Windows | El personaje camina y corre más rápido: engancha la actualización del jugador (`fn_80049D28`) y multiplica la velocidad horizontal que el juego guardó el cuadro anterior, solo en juego libre. Por omisión al 150 % de la velocidad original; `HM_MM_PLAYER_SPEED_PERCENT` (de 25 a 300) lo cambia, y en el paquete de Windows `play.ps1 -PlayerSpeed`. Con 400 el personaje atravesó la puerta de su casa; por encima de 200 queda bajo el propio riesgo del jugador. No cambia la animación. Cómo se mueve el personaje: [MOVEMENT.md](../docs/MOVEMENT.md). |
| `cheats` | Planeado | Trucos activables. |

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
(por omisión `jamie_marriage player_speed`). Una función enganchada no puede
estar copiada en línea en otra: la herramienta lo revisa y se detiene si pasa.
El registro del runtime dice qué mods cargó y la primera vez que el juego
llega a cada lugar donde actúan.

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
| [`player_speed`](player_speed/mod.c) | Tried in the game, in the cloud; Windows still to do | The character walks and runs faster: it hooks the player's update (`fn_80049D28`) and multiplies the horizontal velocity that the game stored one frame earlier, only in free play. By default at 150% of the original speed; `HM_MM_PLAYER_SPEED_PERCENT` (25 to 300) changes it, and in the Windows package so does `play.ps1 -PlayerSpeed`. At 400 the character went through the door of its house; above 200 it is at the player's own risk. It does not change the animation. How the character moves: [MOVEMENT.md](../docs/MOVEMENT.md). |
| `cheats` | Planned | Switchable cheats. |

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
default `jamie_marriage player_speed`). A hooked function cannot be copied
inline into another one: the tool checks this and stops if it happens. The
runtime's log says which mods it loaded and the first time the game reaches
each place where they act.
