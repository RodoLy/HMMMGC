# Character movement / Movimiento del personaje

Target: `Harvest Moon: Magical Melody (USA)` · Game ID `G4AEE9`

[Español](#español) · [English](#english)

Reproduce / Reproducir:

```sh
python3 tools/disasm.py fn_8001AC20       # the actor update; the call to the player's update is at 0x8001AE10
python3 tools/disasm.py fn_80049D28       # the player's own update
scripts/build_mods.sh player_speed        # builds the mod and installs it in ModernGekko's Mods folder
```

Everything about the game's behaviour here was seen in the running recompiled game (gdb write
watchpoints on its memory, and its automation commands to hold the stick and read memory), and
checked against the disassembly. / Todo lo que se dice aquí del comportamiento del juego se vio en el
juego recompilado en ejecución (puntos de escritura de gdb sobre su memoria, y sus comandos de
automatización para mantener el stick y leer memoria), y se comprobó con el desensamblado.

---

## Español

### Resumen

- El personaje del jugador se mueve con una velocidad que el juego guarda en cada cuadro como
  `dirección × velocidad escalar` y que el cuadro siguiente usa para moverlo. El mod
  [`player_speed`](../mods/player_speed/mod.c) engancha la actualización del jugador
  (`fn_80049D28`) y multiplica esa velocidad horizontal por un factor: 150 % por defecto, de 25 %
  a 300 %.
- No cambia ninguna constante del juego ni recuerda nada entre cuadros. Al final de cada cuadro
  el juego vuelve a escribir la velocidad sin multiplicar, así que el factor se aplica una vez
  por cuadro y no se acumula; y el mod solo toca una velocidad que es, bit por bit, lo que el
  juego escribió.
- Solo actúa en juego libre (estado principal 0). En los eventos (las escenas) no hace nada.
- El máximo es 300 %. Con 400 % el personaje atravesó la puerta de su casa en las seis pruebas y
  cayó fuera del mapa (ver «La puerta de la casa»). Los muros delgados del resto del juego no
  se probaron, así que por encima de 200 % queda bajo el propio riesgo del jugador.
- Medido en el juego recompilado en ejecución: con el stick inclinado a fondo durante 15 cuadros el
  personaje recorre 1,0 / 1,5 / 2,0 veces la distancia original con el factor en 100 / 150 /
  200 %, y no hay deriva al soltar el stick.
- El personaje frena rápido al soltar el stick: la velocidad escalar baja 0,6 por cuadro (el triple
  de lo que sube), así que se detiene en unos 4 cuadros caminando y unos 9 corriendo (0,13 s y
  0,3 s). No hay un deslizamiento largo que quitar.

### Lo que se sabe del movimiento

Direcciones de la versión USA (`G4AEE9`); los desplazamientos son dentro de cada estructura.

| Qué | Dónde |
|---|---|
| Administrador de actores | puntero en `0x80321D58`; en `administrador + 0x74` hay una tabla de 100 punteros a actores |
| Actor del jugador | el primero de la tabla; su byte de tipo (`actor + 0x94`) vale 1 |
| Nodo de física del actor | palabra en `actor + 0x30` |
| Posición | tres floats en `nodo + 0x44` (x), `+ 0x48` (y, vertical) y `+ 0x4C` (z) |
| Velocidad | tres floats en `nodo + 0x98`, `+ 0x9C` y `+ 0xA0`, en el marco del propio actor: z es «hacia adelante» |
| Dirección | tres floats en `actor + 0x44`, `+ 0x48` y `+ 0x4C` (en el marco del actor; z ≈ 0,96 mientras camina, con el stick a medias o a fondo, y 0,84 al soltarlo) |
| Velocidad escalar | float en `actor + 0x58` |
| Estado del jugador | media palabra en `actor + 0xC8` (0 parado, `0x08` caminando, `0x0C` corriendo) |
| Estado principal del juego | palabra en `0x80321D18`: 0 juego libre, 4 evento en curso, `0x25` y `0x26` cambio de día, `0x28` revisión del campo (ver [SCRIPTS.md](SCRIPTS.md)) |

La lógica del juego corre a 30 cuadros por segundo: el paso de tiempo es 1/30 y, a la velocidad
escalar máxima de correr, el personaje avanza unas 0,16 unidades por cuadro.

### Un cuadro de movimiento

`fn_8001AC20(actor, entrada, paso)` actualiza a un actor. Para el jugador, en este orden:

1. En `0x8001AE10` llama a `fn_80049D28`, la actualización propia del jugador: lee el stick y pone
   la dirección y la velocidad escalar. La velocidad escalar sube 0,2 por cuadro. Caminando
   (estado `0x08`) llega a 3,0; si el stick sigue pulsado (se vio con el stick a fondo) el
   personaje pasa a correr (estado `0x0C`) y sigue subiendo 0,2 por cuadro hasta 5,0, donde alterna
   entre 5,0 y 4,9. Los topes son constantes en `0x80322818` y `0x80322848`, que otras partes del
   código también leen. Al soltar el stick la velocidad escalar baja 0,6 por cuadro.
2. Si `actor + 0xC6` vale 0, en `0x8001B0F4` llama a `fn_801BEC2C(nodo, paso)`, que mueve el nodo:
   posición += rotación · (velocidad · paso). Usa la velocidad que el juego guardó **el cuadro
   anterior**.
3. Entre `0x8001B15C` y `0x8001B194` (si `fn_8003465C()` coincide con `actor + 0x156`) guarda la
   velocidad del cuadro siguiente: `nodo.velocidad = dirección × velocidad escalar`, con `fmuls`
   (precisión simple).

### Qué hace el mod

`mods/player_speed` pone un gancho de entrada en `fn_80049D28` (`RECOMP_HOOK`). Se ejecuta antes del
paso 1, cuando la dirección y la velocidad escalar aún valen lo que valían al guardar la velocidad
(paso 3 del cuadro anterior). Entonces:

1. No hace nada si el estado principal no es 0, si el primer argumento (el actor) no es de tipo 1 o
   si el actor no tiene nodo de física.
2. Para x y para z (no para y, que es de la caída): calcula `dirección × velocidad escalar` en
   precisión simple, igual que el juego. Si la velocidad guardada en el nodo es **exactamente** ese
   valor (bit por bit), la reemplaza por el valor multiplicado por el factor. Si es otra cosa (un
   empujón, un retroceso), la deja.
3. El movimiento del paso 2 del cuadro usa entonces la velocidad multiplicada, y el paso 3 vuelve a
   escribir la velocidad normal para el cuadro siguiente.

Lo que sale de ahí:

- El factor se aplica una vez por cuadro y nunca se acumula, y como no hay estado, cargar una
  partida, cambiar de área o empezar un día no lo afecta.
- Si ModernGekko corre el gancho más de una vez por llamada (puede pasar, ver
  [`mods/README.md`](../mods/README.md)), la segunda pasada ya no reconoce la velocidad y la deja
  como está.
- El factor se lee una vez, al cargar el mod, de la variable de entorno `HM_MM_PLAYER_SPEED_PERCENT`
  (un número entero, 25 a 300; sin ella, 150). Con un valor inválido el mod escribe una línea en el
  registro y usa 150. Con el paquete de Windows se da con `Play.bat -PlayerSpeed 200`.
- La animación no cambia: a un factor alto los pies podrían verse resbalar (no se ha visto, las
  pruebas corren sin vídeo).

### Alternativas descartadas

| Idea | Por qué no |
|---|---|
| Cambiar los topes 3,0 y 5,0 | Son constantes que el juego comparte con código que no tiene que ver, y no cambian cómo crece la velocidad escalar. |
| Multiplicar la dirección | Se escribe una sola vez al empezar a caminar y se conserva, así que se multiplicaría otra vez en cada cuadro. |
| Multiplicar la velocidad escalar (`actor + 0x58`) | La leen otras partes del código además de la física. |
| Un gancho en `fn_8001AC20` | Corre una vez por cada actor y cada cuadro, y en la prueba no se disparó nunca (ver abajo). |
| Un gancho de salida (`RECOMP_HOOK_RETURN`) | No se probó: no se revisó cómo el runtime maneja los retornos pendientes, y el de entrada bastó. |

### Pruebas

Material desechable (guiones y estados guardados), que no está en el repositorio. Las pruebas cargan
un estado de emulación guardado con el personaje dentro de su casa (índice de campo 27), en juego
libre. En las caminatas se mantiene el stick durante un número fijo de cuadros (los cuenta el
runner, no el reloj), se suelta, se dejan pasar 150 cuadros hasta que el personaje se detiene y
luego otros 120 por si se desplaza. Sin vídeo (`MG_GRAPHICS=Null`).

**Caminata de 15 cuadros con el stick hacia abajo (z)**

| Factor | Distancia (unidades del juego) | Razón con el original |
|---|---|---|
| Sin el mod | 0,923308 | 1,000 |
| 100 % | 0,923308 | 1,000 (idéntica, bit por bit) |
| 150 % | 1,384962 | 1,500 |
| 200 % | 1,846616 | 2,000 |
| 300 % | 2,415375 | 2,616 (ver «Qué no está verificado») |

En ninguna configuración hubo deriva tras detenerse, y el área y el estado principal no cambiaron.
Con el mod cargado el registro dice `[player_speed] the player moves at N% of the original speed` y
`mod loaded: player_speed 0.1.0`.

**Valores de la variable**

Con `HM_MM_PLAYER_SPEED_PERCENT` en 25 y en 300 el registro dice que el personaje se mueve al 25 % y
al 300 %. Con 24, 301, 400 y `abc` dice `is not a whole number from 25 to 300: using 150` y que se
mueve al 150 %; sin la variable, también al 150 %.

**Caminata de 40 cuadros hacia la izquierda, contra un obstáculo**

Con el stick a la izquierda durante 40 cuadros el personaje llega a un obstáculo de la casa (en
la prueba actúa como una pared, pero no se sabe qué objeto es). Sin el mod, con el factor en 200 %
y en 400 % termina exactamente en el mismo punto (x = 3,2254, z = 1,4589; desplazamiento −3,374510
en x y −0,128773 en z): un paso más largo no lo atraviesa.

**Los eventos de la boda, sin el mod y con él al 200 %**

Es la misma cadena de la [prueba del mod de Jamie](SCRIPTS.md#prueba-en-ejecución-del-mod-2026-11-04)
(estado guardado del cambio de día, condiciones de la boda escritas a mano, un guion que pulsa A
mientras corre un evento y `jamie_marriage` cargado en las dos corridas). Corrieron a la vez, una
con `player_speed` al 200 % (el registro lo confirma) y otra sin él:

| | Sin `player_speed` | Con `player_speed` al 200 % |
|---|---|---|
| Eventos, en orden | `0x10D`, `0x10B` | `0x10D`, `0x10B` |
| Estados principales | `0x25` → `0x28` → 4 → `0x28` → 4 → `0x28` → 6 → 0 | los mismos |
| Bandera 230 al final | apagada | apagada |
| Área y posición al final | área 0, (10, 3,3, 28) | área 0, (10, 3,3, 28) |

Solo demuestra que el mod no altera los eventos: en esos tramos nadie mueve al personaje con el
stick, y el gancho no hace nada fuera del juego libre.

**La puerta de la casa: con 400 % el personaje la atraviesa**

Un paso más largo por cuadro puede cruzar una pared delgada sin tocarla (lo más probable; no se
midió el grosor de la puerta). La puerta de la casa del jugador está en el muro sur, hacia x = 4,3,
y se abre pulsando A. Se probó de dos formas, cada una sin el mod y con el factor en 150, 200, 300 y
400 %, cargando el mismo estado guardado.

*Empujar contra el muro.* Se coloca al personaje en x = 4,3 y a seis distancias del muro
(z = 2,8; 3,2; 3,6; 4,0; 4,4 y 4,8), se empuja el stick hacia abajo durante 50 cuadros y se lee
dónde termina. El muro está en z = 5,51.

| Factor | Dónde terminó el personaje (seis empujones) |
|---|---|
| Sin el mod | 4 en z = 5,510; 2 en z = 5,597 y 5,630, con y = 0 |
| 150 % | 6 en z = 5,510 |
| 200 % | 5 en z = 5,510; 1 en z = 5,650, con y = 0 |
| 300 % | 6 en z = 5,510 |
| 400 % | **los 6 atravesaron**: z entre 23,6 y 25,5, y entre −1,9 y −2,5 (cayendo) |

Los finales entre 5,597 y 5,650 (sin el mod y con 200 %) quedan a menos de 0,15 del muro, sobre el
suelo (y = 0) y sin que cambie el área. El juego original también los produce, así que no cuentan
como atravesar; puede ser el fondo del vano de la puerta (no se comprobó).

*Salir por la puerta.* Se coloca al personaje en x = 4,3 y z = 4,8, se empuja el stick hacia abajo
50 cuadros, se pulsa A hasta que cambia el área (hasta 200 cuadros) y se dejan pasar 250 cuadros más.
El área 27 es la casa del jugador y la 0 es el exterior.

| Factor | Tras empujar | Área | Dónde terminó (x, z) |
|---|---|---|---|
| Sin el mod | z = 5,5099 | 27 → 0 | (118,38; 84,38), estado principal 0 |
| 150 % | z = 5,5099 | 27 → 0 | (118,41; 84,41), estado principal 0 |
| 200 % | z = 5,5099 | 27 → 0 | (118,43; 84,43), estado principal 0 |
| 300 % | z = 5,5099 | 27 → 0 | (118,48; 84,48), estado principal 0 |
| 400 % | z = 24,47 y y = −2,04: atravesó | sigue en 27 (A no hizo nada) | (4,54; 25,55) con y = −35,12 |

Hasta 300 % la puerta aguanta y se sale por ella como sin el mod; el punto donde el personaje termina
fuera difiere en menos de 0,1. Con 400 % el personaje atravesó el muro las seis veces y no se
recuperó en los 250 cuadros que se dejaron pasar: seguía en el área 27, fuera del cuarto y por
debajo del suelo. Por eso el máximo del mod es 300 %.

Un máximo no es una garantía: son seis posiciones en un solo muro. Los muros delgados de otros
lugares (el pueblo, las tiendas, las demás casas) no se probaron, así que por encima de 200 % queda
bajo el propio riesgo del jugador. Con 150 %, el valor por omisión, pasaron las dos pruebas.

**Cuadro por cuadro, con una versión de depuración del mod**

Una versión temporal del mod, que no está en el repositorio, escribió en el registro, en cada cuadro
de juego libre en que el personaje se movía, el estado del jugador, la velocidad escalar, la
velocidad que el juego había guardado, la que quedó tras el gancho y la posición. Se corrió una
caminata de 15 cuadros con el stick a medias (hacia abajo) con el factor en 100 % y en 150 %. El
paso que avanza el personaje en cada cuadro (la diferencia de posición entre dos cuadros seguidos)
es, al 150 %, 1,500 veces el del 100 % en los 18 cuadros, con un margen de ±0,001 por el redondeo
del registro a cinco decimales. Algunos cuadros:

| Cuadro | Estado | Velocidad escalar | Velocidad guardada por el juego | Tras el gancho (150 %) | Paso al 100 % | Paso al 150 % |
|---|---|---|---|---|---|---|
| 1 | `0x08` | 0,2 | 0,192 | 0,288 | 0,00406 | 0,00609 |
| 8 | `0x08` | 1,6 | 1,536 | 2,304 | 0,05084 | 0,07626 |
| 15 | `0x08` | 3,0 | 2,880 | 4,320 | 0,09599 | 0,14398 |
| 16 (stick suelto) | `0x00` | 2,4 | 2,016 | 3,024 | 0,06719 | 0,10080 |
| 18 | `0x00` | 1,2 | 1,008 | 1,512 | 0,03361 | 0,05040 |

Con el factor en 100 % el gancho deja la velocidad como la guardó el juego; con 150 % la deja
multiplicada, y el cuadro siguiente el juego vuelve a guardar la suya: no se acumula. Una caminata
más larga (30 cuadros hacia la izquierda, con el factor en 100 %) dio la secuencia de estados y de
velocidades que describe «Un cuadro de movimiento».

**Una variación de la prueba, no del mod.** Una vez, la caminata de 15 cuadros con el stick a medias
y sin el mod dio 0,804920 en vez de 0,923308, y parecía que el mod multiplicaba por 1,72 en lugar de
1,5. El registro cuadro por cuadro (arriba) mostró que el mod multiplica exactamente por 1,5 en cada
cuadro, y al repetir las corridas sin el mod y con el factor en 100 % salió 0,923308 otra vez. La
diferencia, 0,118388, es la que dan las fórmulas de «Un cuadro de movimiento» para un cuadro menos
con el stick pulsado (0,1184: 0,9360 con 15 cuadros y 0,8176 con 14). Es decir, en esa corrida la
orden de soltar el stick llegó un cuadro antes (se hizo con la CPU muy cargada; no se investigó más).
Cuando una distancia no cuadra conviene repetir la corrida antes de buscar la causa en el mod.

**Costo en velocidad**

Se midieron los cuadros por segundo del runner, sin vídeo, en la nube (2 CPU): carga el estado
guardado de la casa, mantiene el stick a la derecha unos 1 200 cuadros (el personaje camina hasta
la pared y empuja contra ella, y el gancho corre en cada cuadro) y divide los cuadros entre los
segundos que pasaron.

| Configuración | Cuadros por segundo |
|---|---|
| Sin el mod | 18,9 · 21,3 · 19,7 (tres corridas) |
| Un gancho que solo cuenta las llamadas a `fn_80049D28` (medida anterior) | 18,8 |
| `player_speed` al 150 % | 20,4 · 18,1 (dos corridas) |

Tres corridas iguales sin el mod van de 18,9 a 21,3, una variación de más del 10 %. Las del mod
caen en torno a ese rango (en las dos parejas medidas una tras otra salió 4 % y 8 % más lento, y
con el gancho que solo cuenta 0,5 %), así que no se pudo distinguir su costo de la variación de
la prueba: si existe, no se vio mayor que ella. No se midió en Windows.

### Para quien escriba mods

Un gancho de entrada en `fn_8001AC20` no se disparó **ni una vez** en unos 1 200 cuadros de juego
libre, mientras que otro en `fn_80049D28` se disparó en cada llamada. `fn_8001AC20` solo se llama
desde `fn_80019AA4`, que está en el mismo bloque de 16 KB; `fn_80049D28` se llama desde otro bloque.
La hipótesis (sin confirmar) es que un gancho solo se dispara cuando la función se llama desde
fuera de su propio bloque. Por eso conviene enganchar una función que llame otro bloque y
comprobar con una línea en el registro que el gancho corre.

### Qué no está verificado

- Todo se midió en la nube, sin vídeo y con el runner de Linux. Falta probarlo en Windows: que el
  paquete compile el mod con su compilador y que `play.ps1 -PlayerSpeed` llegue hasta él.
- No se vio cómo se ve el personaje con la animación sin cambiar a factores altos.
- Las pruebas inclinaron el stick a fondo. No se sabe qué decide entre caminar (tope 3,0) y correr
  (tope 5,0); el mod no depende del tope, pero eso no se comprobó con otras inclinaciones salvo lo
  que diga la prueba de media inclinación de arriba.
- A 300 % la distancia de la caminata de 15 cuadros no es proporcional (2,616 veces la original en
  vez de 3,0): el personaje salió de z = 1,5877 y se detuvo en z = 4,0031, con 0,016 de desvío en
  x. Algo de la casa lo detuvo, algo que los factores menores no alcanzaron en 15 cuadros; no se
  comprobó qué objeto es.
- Solo se probaron los muros de la casa del jugador (el oeste y el de la puerta) y solo el cambio
  de área por esa puerta. Los muros delgados del resto del juego y otros cambios de área no se
  probaron, y con 400 % el de la puerta no aguantó: por encima de 200 % queda bajo el propio
  riesgo del jugador.
- Que las conversaciones también sean el estado principal 4 no se comprobó; solo se vio la boda de
  Jamie (evento `0x10D`), que corre entera en el estado 4.
- No se probó en festivales ni en minijuegos que corran con el estado principal en 0: ahí el mod sí
  acelera al jugador.
- No se midió el efecto en la economía del juego: con el mismo tiempo de juego se recorre más
  distancia.

---

## English

### Summary

- The player's character moves with a velocity that the game stores each frame as
  `direction × scalar speed` and that the next frame uses to move it. The
  [`player_speed`](../mods/player_speed/mod.c) mod hooks the player's update (`fn_80049D28`) and
  multiplies that horizontal velocity by a factor: 150 % by default, from 25 % to 300 %.
- It does not change any game constant and remembers nothing between frames. At the end of each
  frame the game writes the unscaled velocity again, so the factor is applied once per frame and
  does not build up; and the mod only touches a velocity that is, bit for bit, what the game wrote.
- It only acts in free play (main state 0). In events (the cutscenes) it does nothing.
- The maximum is 300 %. At 400 % the character went through the door of its house in all six
  tests and fell out of the map (see "The door of the house"). The thin walls in the rest of the
  game were not tried, so above 200 % it is at the player's own risk.
- Measured in the running recompiled game: with the stick fully tilted for 15 frames the character
  covers 1.0 / 1.5 / 2.0 times the original distance with the factor at 100 / 150 / 200 %, and
  there is no drift after the stick is released.
- The character brakes quickly when the stick is released: the scalar speed falls by 0.6 per frame
  (three times what it grows by), so it stops in about 4 frames when walking and about 9 when running
  (0.13 s and 0.3 s). There is no long slide to remove.

### What is known about the movement

Addresses of the USA version (`G4AEE9`); offsets are inside each structure.

| What | Where |
|---|---|
| Actor manager | pointer at `0x80321D58`; at `manager + 0x74` there is a table of 100 pointers to actors |
| The player's actor | the first one in the table; its type byte (`actor + 0x94`) is 1 |
| The actor's physics node | word at `actor + 0x30` |
| Position | three floats at `node + 0x44` (x), `+ 0x48` (y, vertical) and `+ 0x4C` (z) |
| Velocity | three floats at `node + 0x98`, `+ 0x9C` and `+ 0xA0`, in the actor's own frame: z is "forward" |
| Direction | three floats at `actor + 0x44`, `+ 0x48` and `+ 0x4C` (in the actor's frame; z ≈ 0.96 while walking, with the stick half or fully tilted, and 0.84 once it is released) |
| Scalar speed | float at `actor + 0x58` |
| Player state | halfword at `actor + 0xC8` (0 idle, `0x08` walking, `0x0C` running) |
| The game's main state | word at `0x80321D18`: 0 free play, 4 event running, `0x25` and `0x26` day change, `0x28` field check (see [SCRIPTS.md](SCRIPTS.md)) |

The game logic runs at 30 frames a second: the time step is 1/30 and, at the maximum running scalar
speed, the character advances about 0.16 units per frame.

### One frame of movement

`fn_8001AC20(actor, input, step)` updates one actor. For the player, in this order:

1. At `0x8001AE10` it calls `fn_80049D28`, the player's own update: it reads the stick and sets the
   direction and the scalar speed. The scalar speed grows by 0.2 per frame. Walking (state `0x08`)
   it reaches 3.0; if the stick stays held (seen with the stick fully tilted) the character
   starts running (state `0x0C`) and the speed keeps growing by 0.2 per frame up to 5.0, where it
   alternates between 5.0 and 4.9. The caps are constants at `0x80322818` and `0x80322848`, which
   other code also reads. When the stick is released the scalar speed falls by 0.6 per frame.
2. If `actor + 0xC6` is 0, at `0x8001B0F4` it calls `fn_801BEC2C(node, step)`, which moves the node:
   position += rotation · (velocity · step). It uses the velocity that the game stored **one frame
   ago**.
3. Between `0x8001B15C` and `0x8001B194` (if `fn_8003465C()` equals `actor + 0x156`) it stores the
   velocity for the next frame: `node.velocity = direction × scalar speed`, with `fmuls` (single
   precision).

### What the mod does

`mods/player_speed` puts an entry hook (`RECOMP_HOOK`) on `fn_80049D28`. It runs before step 1,
when the direction and the scalar speed still hold what they held when the velocity was stored
(step 3 of the previous frame). Then:

1. It does nothing if the main state is not 0, if the first argument (the actor) is not of type 1,
   or if the actor has no physics node.
2. For x and for z (not y, which belongs to falling): it computes `direction × scalar speed` in
   single precision, like the game. If the velocity stored in the node is **exactly** that value
   (bit for bit), it replaces it with the value multiplied by the factor. If it is anything else
   (a push, a knock-back), it leaves it.
3. The move of step 2 of the frame then uses the multiplied velocity, and step 3 writes the plain
   velocity again for the next frame.

What follows from that:

- The factor is applied once per frame and never builds up, and since there is no state, loading
  a save, changing area or starting a day does not affect it.
- If ModernGekko runs the hook more than once per call (it can, see
  [`mods/README.md`](../mods/README.md)), the second pass no longer recognises the velocity and
  leaves it as it is.
- The factor is read once, when the mod loads, from the environment variable
  `HM_MM_PLAYER_SPEED_PERCENT` (a whole number, 25 to 300; 150 without it). With an invalid value
  the mod writes a line in the log and uses 150. With the Windows package it is given with
  `Play.bat -PlayerSpeed 200`.
- The animation is not changed: at a high factor the feet might look as if they slide (not seen:
  the tests run without video).

### Alternatives that were rejected

| Idea | Why not |
|---|---|
| Change the caps 3.0 and 5.0 | They are constants that the game shares with unrelated code, and they do not change how the scalar speed grows. |
| Multiply the direction | It is written once when the walk starts and kept, so it would be multiplied again on every frame. |
| Multiply the scalar speed (`actor + 0x58`) | Other code reads it besides the physics. |
| A hook on `fn_8001AC20` | It runs once for each actor on every frame, and in the test it never fired (see below). |
| An exit hook (`RECOMP_HOOK_RETURN`) | Not tried: how the runtime handles pending returns was not checked, and the entry hook was enough. |

### Tests

Throwaway material (scripts and saved states) that is not in the repository. The tests load a saved
emulation state with the character inside their house (field index 27), in free play. In the walks
the stick is held for a fixed number of frames (counted by the runner, not by the clock), released,
150 frames go by until the character stops, and then 120 more in case it drifts. No video
(`MG_GRAPHICS=Null`).

**15-frame walk with the stick down (z)**

| Factor | Distance (game units) | Ratio to the original |
|---|---|---|
| Without the mod | 0.923308 | 1.000 |
| 100 % | 0.923308 | 1.000 (identical, bit for bit) |
| 150 % | 1.384962 | 1.500 |
| 200 % | 1.846616 | 2.000 |
| 300 % | 2.415375 | 2.616 (see "What is not verified") |

No configuration drifted after stopping, and the area and the main state did not change. With the
mod loaded the log says `[player_speed] the player moves at N% of the original speed` and
`mod loaded: player_speed 0.1.0`.

**Values of the variable**

With `HM_MM_PLAYER_SPEED_PERCENT` at 25 and at 300 the log says that the character moves at 25 % and
at 300 %. With 24, 301, 400 and `abc` it says `is not a whole number from 25 to 300: using 150` and
that it moves at 150 %; without the variable, also at 150 %.

**40-frame walk to the left, against an obstacle**

With the stick to the left for 40 frames the character reaches an obstacle in the house (in the
test it acts as a wall, but what object it is is not known). Without the mod, with the factor at
200 % and at 400 % it ends at exactly the same point (x = 3.2254, z = 1.4589; displacement
−3.374510 in x and −0.128773 in z): a longer step does not go through it.

**The wedding events, without the mod and with it at 200 %**

It is the same chain as in the
[test of the Jamie mod](SCRIPTS.md#run-time-test-of-the-mod-2026-11-04) (saved state of the day
change, wedding conditions written by hand, a script that presses A while an event runs, and
`jamie_marriage` loaded in both runs). They ran at the same time, one with `player_speed` at 200 %
(the log confirms it) and one without it:

| | Without `player_speed` | With `player_speed` at 200 % |
|---|---|---|
| Events, in order | `0x10D`, `0x10B` | `0x10D`, `0x10B` |
| Main states | `0x25` → `0x28` → 4 → `0x28` → 4 → `0x28` → 6 → 0 | the same |
| Flag 230 at the end | cleared | cleared |
| Area and position at the end | area 0, (10, 3.3, 28) | area 0, (10, 3.3, 28) |

It only shows that the mod does not alter the events: in those stretches nobody moves the
character with the stick, and the hook does nothing outside free play.

**The door of the house: at 400 % the character goes through it**

A longer step per frame can cross a thin wall without touching it (most likely; the thickness of
the door was not measured). The door of the player's house is in the south wall, around x = 4.3, and
it opens by pressing A. It was tried in two ways, each without the mod and with the factor at 150,
200, 300 and 400 %, loading the same saved state.

*Pushing against the wall.* The character is placed at x = 4.3 and at six distances from the wall
(z = 2.8, 3.2, 3.6, 4.0, 4.4 and 4.8), the stick is pushed down for 50 frames and where it ends is
read. The wall is at z = 5.51.

| Factor | Where the character ended (six pushes) |
|---|---|
| Without the mod | 4 at z = 5.510; 2 at z = 5.597 and 5.630, with y = 0 |
| 150 % | 6 at z = 5.510 |
| 200 % | 5 at z = 5.510; 1 at z = 5.650, with y = 0 |
| 300 % | 6 at z = 5.510 |
| 400 % | **all 6 went through**: z between 23.6 and 25.5, and y between −1.9 and −2.5 (falling) |

The ends between 5.597 and 5.650 (without the mod and at 200 %) are less than 0.15 from the wall, on
the floor (y = 0), and the area did not change. The original game produces them too, so they do not
count as going through; it may be the depth of the door opening (not checked).

*Leaving through the door.* The character is placed at x = 4.3 and z = 4.8, the stick is pushed down
for 50 frames, A is pressed until the area changes (up to 200 frames) and 250 more frames go by.
Area 27 is the player's house and 0 is the outside.

| Factor | After pushing | Area | Where it ended (x, z) |
|---|---|---|---|
| Without the mod | z = 5.5099 | 27 → 0 | (118.38, 84.38), main state 0 |
| 150 % | z = 5.5099 | 27 → 0 | (118.41, 84.41), main state 0 |
| 200 % | z = 5.5099 | 27 → 0 | (118.43, 84.43), main state 0 |
| 300 % | z = 5.5099 | 27 → 0 | (118.48, 84.48), main state 0 |
| 400 % | z = 24.47 and y = −2.04: went through | still 27 (A did nothing) | (4.54, 25.55) with y = −35.12 |

Up to 300 % the door holds and the character leaves through it as without the mod; the point where
it ends outside differs by less than 0.1. At 400 % the character went through the wall all six
times and did not recover in the 250 frames that went by: it was still in area 27, outside the
room and below the floor. That is why the maximum of the mod is 300 %.

A maximum is not a guarantee: it is six positions on a single wall. The thin walls of other places
(the town, the shops, the other houses) were not tried, so above 200 % it is at the player's own
risk. At 150 %, the default, both tests passed.

**Frame by frame, with a debugging build of the mod**

A temporary build of the mod, which is not in the repository, wrote to the log, on every frame of
free play in which the character moved, the player state, the scalar speed, the velocity that the
game had stored, the one left after the hook, and the position. A 15-frame walk with the stick half
tilted (down) was run with the factor at 100 % and at 150 %. The step that the character advances
on each frame (the position difference between two consecutive frames) at 150 % is 1.500 times
that at 100 % on all 18 frames, within ±0.001 because the log rounds to five decimals. Some frames:

| Frame | State | Scalar speed | Velocity stored by the game | After the hook (150 %) | Step at 100 % | Step at 150 % |
|---|---|---|---|---|---|---|
| 1 | `0x08` | 0.2 | 0.192 | 0.288 | 0.00406 | 0.00609 |
| 8 | `0x08` | 1.6 | 1.536 | 2.304 | 0.05084 | 0.07626 |
| 15 | `0x08` | 3.0 | 2.880 | 4.320 | 0.09599 | 0.14398 |
| 16 (stick released) | `0x00` | 2.4 | 2.016 | 3.024 | 0.06719 | 0.10080 |
| 18 | `0x00` | 1.2 | 1.008 | 1.512 | 0.03361 | 0.05040 |

With the factor at 100 % the hook leaves the velocity as the game stored it; at 150 % it leaves it
multiplied, and on the next frame the game stores its own again: it does not build up. A longer walk
(30 frames to the left, with the factor at 100 %) gave the sequence of states and speeds described in
"One frame of movement".

**A variation of the test, not of the mod.** Once, the 15-frame walk with the stick half tilted and
without the mod gave 0.804920 instead of 0.923308, and it looked as if the mod multiplied by 1.72
instead of 1.5. The frame-by-frame log (above) showed that the mod multiplies by exactly 1.5 on every
frame, and when the runs without the mod and with the factor at 100 % were repeated the result was
0.923308 again. The difference, 0.118388, is what the formulas of "One frame of movement" give for
one frame less with the stick held (0.1184: 0.9360 with 15 frames and 0.8176 with 14). That is, in
that run the order to release the stick arrived one frame early (it was done with the CPU heavily
loaded; it was not investigated further). When a distance does not add up it is better to repeat
the run before looking for the cause in the mod.

**Cost in speed**

The frames per second of the runner were measured, without video, in the cloud (2 CPUs): it loads the
saved state of the house, holds the stick to the right for about 1,200 frames (the character walks to
the wall and pushes against it, and the hook runs on every frame) and divides the frames by the
seconds that went by.

| Configuration | Frames per second |
|---|---|
| Without the mod | 18.9 · 21.3 · 19.7 (three runs) |
| A hook that only counts the calls to `fn_80049D28` (earlier measurement) | 18.8 |
| `player_speed` at 150 % | 20.4 · 18.1 (two runs) |

Three identical runs without the mod go from 18.9 to 21.3, a variation of more than 10 %. Those of
the mod fall around that range (in the two pairs measured one after the other it came out 4 % and
8 % slower, and with the hook that only counts 0.5 %), so its cost could not be told apart from the
variation of the test: if it exists, it was not seen to be larger than that. It was not measured on
Windows.

### For mod authors

An entry hook on `fn_8001AC20` did **not fire once** in about 1,200 frames of free play, while one
on `fn_80049D28` fired on every call. `fn_8001AC20` is only called from `fn_80019AA4`, which is in
the same 16 KB block; `fn_80049D28` is called from another block. The hypothesis (unconfirmed) is
that a hook only fires when the function is called from outside its own block. So it is better to
hook a function that another block calls, and to check with a line in the log that the hook runs.

### What is not verified

- Everything was measured in the cloud, without video and with the Linux runner. It still has to be
  tried on Windows: that the package builds the mod with its compiler and that
  `play.ps1 -PlayerSpeed` reaches it.
- It was not seen how the character looks with an unchanged animation at high factors.
- The tests tilted the stick fully. What decides between walking (cap 3.0) and running (cap 5.0)
  is not known; the mod does not depend on the cap, but that was not checked with other tilts
  except for what the half-tilt test above says.
- At 300 % the distance of the 15-frame walk is not proportional (2.616 times the original
  instead of 3.0): the character started at z = 1.5877 and stopped at z = 4.0031, 0.016 off in
  x. Something in the house stopped it, something that the lower factors did not reach in 15
  frames; what object it is was not checked.
- Only the walls of the player's house were tried (the west one and the one with the door) and
  only the change of area through that door. The thin walls in the rest of the game and other
  changes of area were not tried, and at 400 % the one with the door did not hold: above 200 %
  it is at the player's own risk.
- That conversations are also main state 4 was not checked; only Jamie's wedding (event `0x10D`)
  was seen, and it runs entirely in state 4.
- It was not tried in festivals or minigames that run with the main state at 0: there the mod does
  speed up the player.
- The effect on the game's economy was not measured: in the same play time the character covers
  more distance.
