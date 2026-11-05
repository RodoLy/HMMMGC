# Performance / Rendimiento

[Español](#español) · [English](#english)

---

## Español

El juego corre a 30 cuadros por segundo, como en la consola. Un juego recompilado tiene que ejecutar,
en tiempo real y en el procesador de la laptop, el código que la consola ejecutaba a 486 MHz. En una
laptop con Snapdragon (Windows en ARM, DirectX 12) eso al principio casi se lograba: la velocidad era
de 0.89 a 0.96 en la ciudad (1.00 es la velocidad completa) y el juego se quedaba detenido de 3 a 6
segundos al cargar. Dos ajustes por juego lo dejaron en 1.00. Este documento cuenta qué se midió, por
qué esos ajustes funcionan y qué falta.

### Resultados

Medidos en la misma laptop, con el cargador conectado y con el modo normal de Dolphin (un solo hilo
para el procesador y los gráficos), en dos sesiones de juego de pocos minutos: una sin los ajustes y
otra con ellos. Son sesiones sueltas, no un banco de pruebas: sirven para ver el orden de magnitud,
no la décima.

| | Sin los ajustes | Con los ajustes |
| --- | --- | --- |
| Velocidad (una lectura cada ~3 s) | mínima 0.59; 30 de 46 lecturas por debajo de 0.95; 0.89–0.96 en la ciudad | mínima 0.99; ninguna de 38 por debajo de 0.95; 0.99–1.06 todo el tiempo |
| Pausas de 3 s o más, con la imagen detenida | 5 en unos 140 s de juego (de 3 a 6.5 s cada una) | ninguna en unos 110 s; la caída más larga, de ~0.6 s |
| Hilo principal en escenas tranquilas (menú, granja) | 0.90–1.00 de un núcleo | 0.26–0.34 |
| Hilo principal en la ciudad | 0.98–1.00 (al tope, y por eso la velocidad bajaba) | 0.72–0.92 (lo normal, 0.85) |

Se descartó una lectura de 678× que el medidor imprime justo después de una pausa breve: es un error
del medidor, no una velocidad.

Con el cargador, la holgura de la ciudad es de ~18 % lo normal (1 / 0.85) y de ~9 % en el peor
momento medido (1 / 0.92).

Con los mismos ajustes pero **en batería** y con el modo de energía normal de Windows (una sesión de
21 lecturas), el menú y la granja siguieron a 1.00, pero pidieron 0.37–0.42 de un núcleo en lugar de
0.26–0.34: sin el cargador el procesador rinde alrededor de un cuarto menos (la frecuencia del
procesador aún no se mide, así que esto es una deducción). En la ciudad el hilo principal quedó al
tope (0.89–0.99) y la velocidad bajó a 0.45–1.07 (mediana 0.83), con tres lecturas a unos 15
cuadros por segundo.

### Qué se medía y cómo

La velocidad es el tiempo del juego dividido entre el tiempo real. Con `--automation-dir`, el runner
escribe `status.txt` con la velocidad, los cuadros y los retrazados por segundo y los contadores de
cuadros presentados: si los contadores no avanzan entre dos lecturas, el juego está detenido (por
ejemplo, cargando). En la laptop se leyó cada ~3 s con un script de PowerShell que anota también el
uso del procesador de cada hilo (ese script todavía no está en el repositorio).

Para saber por dónde pasa el juego sin herramientas de perfilado, el runner tiene un contador: con la
variable de entorno `STATICRECOMP_DISPATCH_SAMPLES=1`, cada 4096 veces que entra al código recompilado
anota la dirección del juego por la que entra y, al cerrar, imprime las 16 que más se repiten. Cuenta
entradas, no tiempo, pero un bucle que da miles de vueltas por segundo destaca de inmediato.

### El bucle ocioso (`StaticRecompIdlePC`)

Cuando el juego no tiene nada que hacer, el hilo ocioso del sistema operativo de la consola da vueltas
a un bucle de tres instrucciones hasta que una interrupción deja listo otro hilo:

```
80174D98  lwz     r0, -0x6f48(r13)    # RunQueueBits
80174D9C  cmplwi  r0, 0
80174DA0  beq     0x80174d98
```

En la consola eso no cuesta nada. En una recompilación cada vuelta es una entrada al código
recompilado más la contabilidad del runner (contar los ciclos, avanzar el reloj, revisar las
interrupciones), y en la PC de pruebas (sin tarjeta gráfica) el código de ese bucle por sí solo
ocupaba el 25 % del tiempo del procesador en la granja.

El recompilador (JIT) de Dolphin reconoce estos bucles y deja correr el tiempo emulado hasta el
siguiente evento en lugar de ejecutarlos. El núcleo de recompilación estática del runner no los
reconoce solo, pero tiene el ajuste `StaticRecompIdlePC`: cuando el juego llega a esa dirección, el
runner salta el tiempo hasta el siguiente evento (`CoreTiming::Idle`). Con él, la misma escena corrió
un 38 % más rápido en la PC de pruebas (de 0.52 a 0.72, sin límite de velocidad) y, en la laptop, el
hilo principal pasó de ~0.95 a ~0.3 de un núcleo con los mismos 30 cuadros por segundo.

### La velocidad del disco (`FastDiscSpeed`)

Dolphin emula el lector de discos de la consola, que es lento: cada lectura espera la búsqueda y la
rotación del disco. Esa espera es real en la consola, pero aquí solo alarga las cargas. Con
`FastDiscSpeed = True` toda lectura se trata como si ya estuviera en el búfer del lector. En la PC de
pruebas, llegar al mismo punto del arranque pasó de 128 s a 79 s. En la laptop, las pausas de 3 a 6 s
pasaron a caídas de menos de un segundo; como los dos ajustes se probaron juntos, esa mejora no se
puede repartir entre uno y otro.

### Dónde están los ajustes

En [`config/G4AEE9/game_settings.ini`](../config/G4AEE9/game_settings.ini). El paquete de Windows lo
agrega después de los ajustes que Dolphin ya trae para el juego (`bin\Sys\GameSettings\G4AEE9.ini`),
y `scripts/build_and_run_linux.sh` lo copia a `GameSettings/G4AEE9.ini` de la carpeta del usuario si
no hay uno. Para cambiar uno solo para ti, escribe la misma línea en
`%LOCALAPPDATA%\HarvestMoonMM\GameSettings\G4AEE9.ini`: Dolphin le da prioridad a ese archivo.

### Encontrar el bucle en otra versión del juego

La dirección es la de la versión USA (`G4AEE9`). Para otra versión:

```
python3 tools/find_idle_loops.py --dol ruta/a/main.dol
```

lista los bucles de espera del juego con la regla que usa Dolphin (cuerpo corto, solo lee memoria,
vuelve a su primera instrucción y no cuenta con CTR). Casi todos esperan a una pieza de hardware y
duran poco. El que hay que poner en `StaticRecompIdlePC` es el que además aparece entre las
direcciones que más repite el contador `STATICRECOMP_DISPATCH_SAMPLES`. En este juego, de las 16
direcciones más repetidas solo una es un bucle de espera: `0x80174D98`; las demás son el bucle de
`dcbf` de la función que vacía la caché de datos y las funciones que apagan y restauran las
interrupciones, entre otras. `tests/test_find_idle_loops.py` comprueba, con tu `main.dol` (variable
`HM_MM_DOL`), que la dirección del archivo de ajustes es de verdad ese bucle.

### Lo que falta

- **Batería.** Con el modo de energía normal, el juego baja de 1.00 en las escenas pesadas. Falta
  medirlo con el modo «Mejor rendimiento» y ganar más holgura (lo que sigue).
- **Optimización por perfil (PGO).** `moderngekko-port pgo-run` construye un módulo instrumentado, deja
  jugar y lo vuelve a construir con lo que midió. Aún no se ha probado con este juego.
- **Gastos fijos del runner.** En el perfil de la PC de pruebas, el bucle que llama al código
  recompilado (`StaticRecompCore::Run`) se lleva un 6–7 % del tiempo; el gestor de mods, un 2–4 %
  aunque no haya mods; la invalidación de la caché de instrucciones que sigue a cada `dcbf`, un
  1–2 %; y las funciones auxiliares de coma flotante (`ppc_fma`, `dolrecomp_f32_from_bits` y otras),
  cerca del 10 %. Además, tres lugares reciben cada uno entre el 8 y el 11 % de las entradas al código
  recompilado: el bucle de `dcbf` y las funciones que apagan y restauran las interrupciones
  (`0x80170978`, `0x80172824` y `0x8017284C`). Hay margen para abaratarlos; no se ha medido cuánto.
- **La pausa al cambiar de zona** (menos de un segundo en la laptop, 0.86 s en la PC de pruebas) es
  trabajo real del juego, que carga y descomprime en una sola ráfaga; solo un procesador más rápido o
  código más rápido la acorta.

---

## English

The game runs at 30 frames per second, like the console. A recompiled game has to run, in real time
and on the laptop's processor, the code that the console ran at 486 MHz. On a laptop with a Snapdragon
(Windows on ARM, DirectX 12) that was almost achieved at first: the speed was 0.89 to 0.96 in the city
(1.00 is full speed) and the game stood still for 3 to 6 seconds when loading. Two per-game settings
brought it to 1.00. This document tells what was measured, why those settings work and what is left.

### Results

Measured on the same laptop, with the charger connected and with Dolphin's normal mode (one thread for
the processor and the graphics), in two game sessions of a few minutes: one without the settings and
one with them. They are loose sessions, not a benchmark: they show the order of magnitude, not the
tenth.

| | Without the settings | With the settings |
| --- | --- | --- |
| Speed (one reading every ~3 s) | lowest 0.59; 30 of 46 readings below 0.95; 0.89–0.96 in the city | lowest 0.99; none of 38 below 0.95; 0.99–1.06 all the time |
| Pauses of 3 s or more, with the picture stopped | 5 in about 140 s of play (3 to 6.5 s each) | none in about 110 s; the longest dip, ~0.6 s |
| Main thread in quiet scenes (menu, farm) | 0.90–1.00 of one core | 0.26–0.34 |
| Main thread in the city | 0.98–1.00 (maxed out, which is why the speed dropped) | 0.72–0.92 (usually 0.85) |

One reading of 678× that the meter prints right after a short pause was discarded: it is an error of
the meter, not a speed.

With the charger, the headroom in the city is ~18 % usually (1 / 0.85) and ~9 % at the worst moment
measured (1 / 0.92).

With the same settings but **on battery** and with Windows' normal power mode (one session of 21
readings), the menu and the farm stayed at 1.00 but asked for 0.37–0.42 of a core instead of
0.26–0.34: without the charger the processor delivers about a quarter less (the processor frequency
is not measured yet, so this is a deduction). In the city the main thread was maxed out (0.89–0.99)
and the speed fell to 0.45–1.07 (median 0.83), with three readings at about 15 frames per second.

### What was measured and how

The speed is game time divided by real time. With `--automation-dir`, the runner writes `status.txt`
with the speed, the frames and retraces per second and the counters of presented frames: if the
counters do not advance between two readings, the game is stopped (for example, loading). On the
laptop it was read every ~3 s by a PowerShell script that also records the processor use of every
thread (that script is not in the repository yet).

To find out where the game spends its time without profiling tools, the runner has a counter: with the
environment variable `STATICRECOMP_DISPATCH_SAMPLES=1`, every 4096th time it enters the recompiled
code it records the game address it enters at and, on exit, prints the 16 that repeat the most. It
counts entries, not time, but a loop that turns thousands of times per second stands out at once.

### The idle loop (`StaticRecompIdlePC`)

When the game has nothing to do, the console operating system's idle thread spins in a loop of three
instructions until an interrupt makes another thread ready:

```
80174D98  lwz     r0, -0x6f48(r13)    # RunQueueBits
80174D9C  cmplwi  r0, 0
80174DA0  beq     0x80174d98
```

On the console that costs nothing. In a recompilation every turn is an entry into the recompiled code
plus the runner's bookkeeping (counting the cycles, advancing the clock, checking the interrupts), and
on the test PC (without a graphics card) the code of that loop alone took 25 % of the processor's time
on the farm.

Dolphin's recompiler (JIT) recognises these loops and lets emulated time run on to the next event
instead of executing them. The runner's static-recompilation core does not recognise them by itself,
but it has the `StaticRecompIdlePC` setting: when the game reaches that address, the runner skips time
ahead to the next event (`CoreTiming::Idle`). With it, the same scene ran 38 % faster on the test PC
(from 0.52 to 0.72, with no speed limit) and, on the laptop, the main thread went from ~0.95 to ~0.3
of a core at the same 30 frames per second.

### The disc speed (`FastDiscSpeed`)

Dolphin emulates the console's disc drive, which is slow: every read waits for the disc to seek and
spin. That wait is real on the console, but here it only lengthens the loads. With
`FastDiscSpeed = True` every read is treated as if it were already in the drive's buffer. On the test
PC, getting to the same point of the start-up went from 128 s to 79 s. On the laptop, the pauses of 3
to 6 s became dips of under a second; since the two settings were tested together, that improvement
cannot be split between them.

### Where the settings are

In [`config/G4AEE9/game_settings.ini`](../config/G4AEE9/game_settings.ini). The Windows package adds it
after the settings that Dolphin already has for the game (`bin\Sys\GameSettings\G4AEE9.ini`), and
`scripts/build_and_run_linux.sh` copies it to `GameSettings/G4AEE9.ini` in the user folder if there is
none. To change one only for yourself, write the same line in
`%LOCALAPPDATA%\HarvestMoonMM\GameSettings\G4AEE9.ini`: Dolphin gives that file priority.

### Finding the loop in another version of the game

The address is the USA version's (`G4AEE9`). For another version:

```
python3 tools/find_idle_loops.py --dol path/to/main.dol
```

lists the game's wait loops with the rule that Dolphin uses (a short body, only reads memory, goes
back to its first instruction and does not count with CTR). Almost all of them wait for a piece of
hardware and are short-lived. The one to put in `StaticRecompIdlePC` is the one that also appears
among the addresses that the `STATICRECOMP_DISPATCH_SAMPLES` counter repeats the most. In this game,
of the 16 most repeated addresses only one is a wait loop: `0x80174D98`; the others are, among other
things, the `dcbf` loop of the function that flushes the data cache and the functions that disable and
restore interrupts. `tests/test_find_idle_loops.py` checks, with your `main.dol` (variable
`HM_MM_DOL`), that the address in the settings file really is that loop.

### What is left

- **Battery.** With the normal power mode, the game drops below 1.00 in heavy scenes. It still has to
  be measured with the "Best performance" mode, and more headroom has to be won (what follows).
- **Profile-guided optimization (PGO).** `moderngekko-port pgo-run` builds an instrumented module,
  lets you play and builds it again with what it measured. It has not been tried with this game yet.
- **The runner's fixed costs.** In the test PC's profile, the loop that calls the recompiled code
  (`StaticRecompCore::Run`) takes 6–7 % of the time; the mod manager, 2–4 % even when there are no
  mods; the instruction-cache invalidation that follows every `dcbf`, 1–2 %; and the floating-point
  helper functions (`ppc_fma`, `dolrecomp_f32_from_bits` and others), close to 10 %. In addition,
  three places each receive 8 to 11 % of the entries into the recompiled code: the `dcbf` loop and
  the functions that disable and restore interrupts (`0x80170978`, `0x80172824` and `0x8017284C`).
  There is room to make them cheaper; how much has not been measured.
- **The pause when changing zone** (under a second on the laptop, 0.86 s on the test PC) is the
  game's own work, which loads and decompresses in a single burst; only a faster processor or faster
  code shortens it.
