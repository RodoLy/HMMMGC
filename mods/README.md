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

Un mod instalado es una carpeta `<id>.mgm/` con un `mod.so` dentro. Para borrar
un mod, borra su carpeta.

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

### Mods del proyecto

| Mod | Estado | Qué hace |
|---|---|---|
| [`boot_logger`](boot_logger/mod.c) | Listo | Prueba de humo: imprime una línea cada vez que el juego entra a `OSInit`, `DVDInit`, `VIInit`, `PADInit`, `GXInit` y `main`. No cambia nada. |
| [`state_logger`](state_logger/mod.c) | Listo | Imprime cada cambio de estado de la máquina de estados del juego (título, día nuevo, eventos, juego libre), cada estado que otro código pide y cada vez que entra la revisión del campo. Sirve para saber cuándo corre un gancho. No cambia nada. |
| [`jamie_marriage`](jamie_marriage/mod.c) | Probado en el juego con condiciones escritas a mano | Casarse con Jamie ya no termina el juego: si la ceremonia ya empezó y la bandera 230 sigue encendida, la apaga en la revisión del campo (diseño en [SCRIPTS.md](../docs/SCRIPTS.md)). Sin el mod, tras la ceremonia sigue el epílogo y el título; con él, el juego queda en juego libre, después de ver una vez la escena de las notas. Falta probarlo con una partida real ([resultados](../docs/SCRIPTS.md#prueba-en-ejecución-del-mod-2026-11-04)). |
| `movement` | Planeado | Personaje más rápido y sin resbalar. |
| `cheats` | Planeado | Trucos activables. |

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

An installed mod is a folder `<id>.mgm/` with a `mod.so` inside. To remove a
mod, delete its folder.

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

### The project's mods

| Mod | Status | What it does |
|---|---|---|
| [`boot_logger`](boot_logger/mod.c) | Done | Smoke test: prints a line each time the game enters `OSInit`, `DVDInit`, `VIInit`, `PADInit`, `GXInit` and `main`. Changes nothing. |
| [`state_logger`](state_logger/mod.c) | Done | Prints every change of the game's state machine (title, new day, events, free play), every state other code asks for, and each time the field check is entered. Useful to learn when a hook will run. Changes nothing. |
| [`jamie_marriage`](jamie_marriage/mod.c) | Tried in the game under hand-written conditions | Marrying Jamie no longer ends the game: if the ceremony has already started and flag 230 is still set, it clears the flag in the field check (design in [SCRIPTS.md](../docs/SCRIPTS.md)). Without the mod the ceremony is followed by the epilogue and the title screen; with it the game goes back to free play, after showing the notes scene once. It still has to be tried with a real save ([results](../docs/SCRIPTS.md#run-time-test-of-the-mod-2026-11-04)). |
| `movement` | Planned | A faster character that does not slide. |
| `cheats` | Planned | Switchable cheats. |
