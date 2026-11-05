# Validation of the recompilation / Validación de la recompilación

[Español](#español) · [English](#english)

---

## Español

La recompilación traduce cada instrucción PowerPC del juego a C. Este
documento explica cómo se comprueba que ese C hace lo mismo que el
procesador, qué se comprobó hasta ahora y, con la misma importancia, qué no.
Hay dos comprobaciones que se complementan: el verificador lockstep, que compara
el código recompilado con el intérprete de Dolphin mientras se juega, y una
prueba de cada instrucción con operandos aleatorios, que tiene dos variantes:
con las palabras que usa el juego y con palabras inventadas, que llegan a casi
todo el procesador y no necesitan el juego.

### Cómo se comprueba

ModernGekko trae un verificador «lockstep» (a paso sincronizado) que compara
el código recompilado con el intérprete de CPU de Dolphin, que es la
referencia. Con `STATICRECOMP_LOCKSTEP=1`, **la primera vez** que el juego
ejecuta cada bloque de código recompilado:

1. guarda el estado de la CPU a la entrada y empieza a anotar lo que el
   bloque escribe;
2. deja correr el bloque recompilado;
3. vuelve al estado de entrada y corre el mismo bloque en el intérprete de
   Dolphin, instrucción por instrucción, hasta la dirección donde el bloque
   recompilado devolvió el control;
4. compara los dos finales: r0–r31, los registros de punto flotante (las dos
   mitades de cada uno), LR, CTR, CR, XER, FPSCR, MSR, SRR0, SRR1 y PC; cada
   byte que el bloque escribió en RAM, en la caché bloqueada y en la
   «memoria virtual» falsa de Dolphin; y la secuencia de escrituras y lecturas
   a registros de hardware (MMIO);
5. deja la memoria como la dejó el bloque recompilado y el juego sigue.

Cada diferencia sale en el log como `[lockstep] DIVERGE`, y al cerrar el
juego una línea `[lockstep] summary:` da los contadores. Un «bloque» es lo que
el código recompilado ejecuta entre dos devoluciones del control al
despachador; no es un bloque básico clásico. No se comparan los 25 bloques
que contienen instrucciones que el módulo ejecuta llamando al intérprete de
Dolphin (`mtspr`, `dcbi`, ... : también en el juego normal) ni los unos cien
que cobran cero ciclos.

### Qué se encontró

| Corrida | Qué se jugó | Verificador | Bloques comparados | Divergencias |
|---|---|---|---:|---:|
| A | partida guardada en la casa → casa y granja | de serie | 7 593 | 136 |
| B | arranque en frío → intro → título → partida nueva | de serie | 11 629 | 161 |
| D | B, más casa, granja y camino | con parche | 11 296 | **0** |
| E | repetición de D | con parche | 11 286 | **0** (378 coincidencias tardías) |
| F | D, más un jugador aleatorio en la casa, la granja y el camino (sin video) | con parche | 12 195 | **0** (392 coincidencias tardías) |

«De serie» es el verificador que trae ModernGekko; «con parche» es el de
[patches/moderngekko/lockstep-verifier.patch](../patches/README.md). En las
cinco corridas de la tabla: `fallback=0` (nada se ejecutó fuera del módulo
recompilado), `smc_failed=0`, `cap_hits=0` (el intérprete siempre terminó el
bloque) y `undercharges=0`. La corrida C no aparece: probó otra idea (detener
el intérprete cuando iguala los ciclos cobrados), dio 72 divergencias y 8
bloques que llegaron al tope de pasos, y la descarté por la de «cualquier
llegada».

La corrida F es la única que guardó el archivo de cobertura (más abajo). Repite
el recorrido de D y después deja jugar a un jugador aleatorio (palanca, botones
y palanca C al azar, semilla 5, que recarga la partida de la casa cada 100
acciones para no quedarse atrapado) con `--graphics Null`, que no dibuja nada y
por eso va más rápido. La detuve a los 37 minutos, tras unas 200 de las 1 500
acciones previstas, para medir la cobertura.

Las 136 y 161 divergencias del verificador de serie tienen tres causas, y
ninguna es un error de cálculo del código recompilado:

| Causa | A | B | Qué es |
|---|---:|---:|---|
| `fpscr_result_flags` | 122 | 122 | Solo difiere el FPSCR, y solo en FPRF, FR y FI (máscara `0x0007F000`), que cada instrucción de punto flotante reescribe con la descripción de su resultado. En todos los casos el recompilado deja un solo bit de FPCC encendido y el intérprete entre uno y tres. El juego lee el FPSCR en tres sitios (`mffs`, en el código del SDK que guarda y restaura el contexto de punto flotante): el valor se guarda, se enmascara con `0x6005F8FF` o se devuelve, y se vuelve a escribir con `mtfsf`; ninguno de esos sitios decide nada con los bits de resultado. No hay ningún `mcrfs` en el ejecutable. |
| `end_inside_loop` | 12 | 36 | El bloque termina dentro de un bucle. Los bloques recompilados salen de un bucle o de una recursión en línea cuando se agota un presupuesto de ciclos (`DOLRECOMP_C_LOOP_CYCLE_BUDGET`), y vuelven al despachador en la cabecera del bucle. El intérprete del verificador se detenía la **primera** vez que llegaba a esa dirección, y los dos pueden parar en iteraciones distintas del mismo bucle, con registros distintos. |
| sin explicar | 2 | 3 | Lo mismo, pero en bucles que contienen llamadas o recursión (`fn_800CE9CC`; `fn_80086C7C` → `fn_8008477C`), donde mi clasificador no reconoce el bucle. |

El parche ataca esas dos últimas causas de raíz: con
`STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` el intérprete sigue más allá de la
primera llegada y el bloque pasa si el estado recompilado es **exactamente**
igual al del intérprete en *cualquier* llegada (todos los registros, todos
los bytes escritos y toda la secuencia de MMIO), y con
`STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1` no compara esos tres bits. Cada
bloque que solo pasó en una llegada posterior sale como
`[lockstep] LATE-MATCH`; en la corrida E son 378, y entre ellos están tres de
los cinco bloques que antes no tenían explicación (entradas `0x800CECB4`,
`0x80086D9C` y `0x80086DC8`). En 339 de las 378 el intérprete necesita
entre 200 y 320 pasos para llegar a la coincidencia, que es lo que corresponde
a una salida por presupuesto de ciclos. Entre las otras 39 hay 18 con menos de
100 pasos; las que revisé a mano (`0x801C3CC0`, `0x8017079C`) son bucles de
espera que leen una bandera de memoria o de hardware hasta que cambia, y su
número de vueltas depende del tiempo y no de la lógica del juego.

De los cinco bloques sin explicación quedan dos (`0x800CEAC4` y `0x800CED10`,
en `fn_800CE9CC`, que se llama a sí misma). Difieren en `r1` exactamente `0x30`,
el tamaño del marco de pila de esa función, y en los registros que guardan datos
de cada nivel: el código recompilado y el intérprete se detuvieron con un nivel
de recursión de diferencia, que es el mismo caso. Pero ninguna corrida
posterior los reportó, ni como divergencia ni como coincidencia tardía, así que
no he visto al verificador con parche aceptar esos dos bloques concretos. Sus
instrucciones sí se compararon sin diferencias en la corrida F.

**Controles negativos.** Un verificador que nunca reporta nada no prueba
nada, así que se le inyectó un fallo a propósito
(`STATICRECOMP_LOCKSTEP_INJECT_FAULT`: un bit de `r3` cambiado en el estado
final recompilado de un bloque) en un bloque recto y en un bloque de bucle, con
las opciones del parche activadas, y las dos veces lo reportó:

```
[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: r3:N=0x804f2468,I=0x804f2478
[lockstep] DIVERGE #1 entry=0x801C49CC end=0x801C32C0: r3:N=0x10,I=0x0
```

### Cobertura

`tools/lockstep_coverage.py` lee el archivo de cobertura de una o más corridas y
dice cuánto del juego se comparó de verdad. Una instrucción cuenta como
comparada cuando el intérprete la ejecutó dentro de un bloque que coincidió en
su primera visita; las de una rama que el bloque no tomó en esa visita no
cuentan. La corrida F, la única que guardó el archivo, comparó:

| Medida | Resultado |
|---|---:|
| Instrucciones | 115 028 de 449 523 (25,6 %) |
| Funciones con alguna instrucción comparada | 2 207 de 4 486 (49,2 %) |
| Formas de instrucción comparadas al menos una vez | 151 de 171 |
| Formas comparadas en dos sitios o menos | 14 |

Las 20 formas que ninguna corrida comparó son `eqv`, `psq_lx`, `mfsr`, `mtsr`,
`dcbi`, `dcbf`, `creqv`, `lfsu`, `subfze`, `icbi`, `twi`, `stfdu`, `dcbst`,
`andis.`, `mffs`, `mcrxr`, `lfdu`, `stfdx`, `rlwnm` y `bla`. Un error en la
traducción de cualquiera de ellas no habría aparecido, y de eso se ocupa la
prueba por instrucción de la sección siguiente.

La mayor parte del juego no se pisó. La función más grande que ninguna corrida
llegó a ejecutar es `fn_80166888` (6 990 instrucciones), seguida de
`fn_80125564` (3 050) y `fn_80091B10` (2 191).

`fn_80166888` es la revisión del «campo»: con ella el juego decide si arranca un
evento de la historia, entre ellos los de la boda. Según el código, el estado
0x28 la llama y pasa de inmediato al estado siguiente, así que corre una vez
cada vez que el juego entra en ese estado (al empezar un día nuevo y al terminar
un evento) y no mientras el personaje camina. Lo comprobé con el mod
[`state_logger`](../mods/state_logger/mod.c), que solo anota los cambios de
estado: en unos 8 000 cuadros caminando y pulsando botones, desde la casa, el
juego se quedó en el estado 0 y la revisión no entró nunca. Al irse a dormir pasó
por los estados 0x12, 0x13, 7, 0x29, 9, 0x26 y 0x25; entonces entró la revisión y
el estado 0x28 fue seguido de inmediato por el 4, el que espera a que termine un
evento. (El mod anota un cambio de estado un cuadro después de que ocurre, así
que la línea de la revisión sale antes que la que anuncia el 0x28, y puede
salir dos veces: un gancho puede repetirse en una sola llamada.) Ninguna corrida
verificada llegó a un día nuevo ni al final de un evento, así que esa parte del
juego quedó sin comparar.

### Prueba por instrucción

La cobertura de arriba dice que jugar compara una cuarta parte de las
instrucciones del juego. `tools/isa_conformance.py` ataca esa brecha por otro
lado: en vez de preguntar «¿qué código ejecuta una partida?», pregunta «¿hace
cada instrucción del juego lo mismo que el procesador, con operandos de todo
tipo?».

1. Recorre las funciones del juego y reúne las palabras de instrucción
   distintas de cada una de las 171 formas (`lwz`, `fmadds`, `ps_sum0`, ...):
   103 649 palabras.
2. De cada forma toma hasta 24, **tal como las tiene el juego** (mismos
   registros, desplazamientos y máscaras). Las reparte primero entre las
   combinaciones de registros que el juego usa, es decir, qué operandos son el
   mismo registro y cuáles son `r0`, porque una traducción que escribe el
   resultado antes de haber leído todos los operandos solo falla cuando un
   operando es el destino (`lwz r3, 8(r3)`), y esas palabras son pocas. En las
   157 formas que se pueden ejecutar el juego tiene 581 combinaciones y cada
   corrida prueba 579; las dos que quedan fuera no se pueden ejecutar tal como
   están (un `lwz` sin registro base leería la parte baja de la memoria, y un
   `lmw` cuyo rango de registros incluye su propia base es una forma inválida).
3. Arma con ellas un programa PowerPC, un DOL común, con un caso por palabra:
   la instrucción, las pocas instrucciones que apuntan un registro a memoria de
   trabajo si es un `load` o un `store`, y `blr`.
4. Antes de cada grupo de casos carga un estado aleatorio completo: los 32
   registros generales, CR, XER, el modo de redondeo del FPSCR y los 32
   registros de punto flotante, con valores de borde frecuentes (0, 1,
   `0x7FFFFFFF`, `0x80000000`, ±infinito, NaN, denormales...), y 64 KB de
   memoria aleatoria.
5. `moderngekko-port` recompila el programa como si fuera el juego y el
   verificador lockstep compara cada caso con el intérprete de Dolphin. Como
   cada caso termina en `blr`, es un bloque completo y se compara con todos los
   registros y todo lo que escribió en memoria.

`tools/run_isa_conformance.py` hace los cinco pasos con un solo comando.

**Resultado.** 10 semillas (cada una es otro programa con otros estados
aleatorios):

| Casos | Formas | Iguales | Distintos | Sin comparar |
|---:|---:|---:|---:|---:|
| 26 450 | 157 | 26 181 | **0** | 269 |

Los «sin comparar» son los de `dcbf`, `dcbi`, `dcbst` e `icbi`, y los de
`mfspr` y `mtspr` sobre SPRG0 a SPRG3 (registros 272 a 275) y, solo `mfspr`, PVR
(287): el módulo los ejecuta llamando al intérprete de Dolphin
(`ppc_fallback_instruction`), así que no hay código recompilado que comparar. En
las diez corridas el verificador terminó con `reports=0`, `skipped_zero=0`,
`cap_hits=0`, `undercharges=0` y `late_matches=0`.

Antes de repartir las palabras por combinaciones de registros, seis semillas con
palabras elegidas al azar (15 864 casos) tampoco habían mostrado diferencias.

**Tres diferencias que sí aparecieron al principio, y que no son errores del
recompilador.** Las tres son casos que el manual deja indefinidos, y el generador
ya no los da:

- `fmadds` y las demás instrucciones de precisión simple, con un operando doble
  que no cabe en un single: el manual dice que el resultado es indefinido, y
  Dolphin y el código recompilado difieren en la carga útil de un NaN. Ahora
  esas instrucciones solo reciben operandos que son singles.
- `mtxer` con bits reservados: el código recompilado conserva los 32 bits
  escritos y el intérprete borra los reservados. El juego solo escribe lo que
  leyó con `mfxer`. Ahora solo se escriben SO, OV, CA y la cuenta de bytes.
- `mtfsf` con el bit 20 del FPSCR (reservado): lo mismo, y la misma razón. Ahora
  el registro de origen pasa antes por una máscara que lo borra.

Esta tercera la encontró la autoprueba de abajo, porque una corrida que debía
reportar un solo caso reportó dos.

**Controles.** Una prueba que nunca falla no prueba nada:

- *Fallo inyectado.* `run_isa_conformance.py --inject-fault lwz` hace que el
  verificador cambie un bit de `r3` al final del primer caso `lwz`, y la
  corrida tiene que reportar ese caso y ningún otro. Termina con
  `self-check passed` o con `self-check FAILED`.
- *Código roto a mano.* En el C generado de un programa se cambió `u32 res = a
  + b;` por `a - b;` en un `add` y se invirtió el bit de acarreo de un `addc`.
  Tras recompilar, la corrida reportó esos dos casos y ninguno más, y los otros
  2 616 salieron iguales:

  ```
  entry=0x800164F4  add: add r3, r28, r0  [r3:N=0xffffff72,I=0x54]
  entry=0x80016674  addc: addc r4, r0, r28  [xer:N=0xa0000009,I=0x80000009]
  ```

**Lo que esta prueba no cubre.** No ejecuta saltos, llamadas ni retornos, así
que cómo se enlazan los bloques y cómo salen de un bucle solo lo comprueba
jugar. Tampoco prueba las 14 formas de control, trampa y sistema (`b`, `bc`,
`bcctr`, `bcctrl`, `bclr`, `bclrl`, `bl`, `bla`, `mftb`, `mtmsr`, `mtsr`, `rfi`,
`sc` y `twi`). De ellas, `bla`, `mtsr` y `twi` tampoco las comparó ninguna
corrida de juego. Y cada semilla da un solo estado aleatorio a cada grupo de
casos: un error que solo aparezca con una combinación de valores muy rara
podría necesitar más semillas.

### Prueba por instrucción sin el juego

La prueba de arriba solo ejercita las 171 formas de instrucción que usa el
juego. Las tablas de códigos de operación del proyecto nombran 349. Con
`--synthetic`, el programa de prueba se arma con palabras **inventadas** que
alcanzan 345 de ellas (las otras cuatro, `eciwx`, `ecowx`, `lswx` y `stswx`, no
las lee Capstone, que hace de juez, y quedan sin probar). Se pueden ejecutar
316; las otras 29 son saltos, llamadas al sistema y trampas, las instrucciones
que cambian el mapa de memoria o leen el reloj, y unas pocas que el juego no usa
y el generador no ejecuta (las de cadenas de bytes `lswi` y `stswi`, la pareja
de reserva `lwarx` y `stwcx.`, y `dcba` y `dcbz_l`). Los tres archivos de `sys/`
también son inventados (`tools/synthetic_disc.py`: `boot.bin`, `bi2.bin` y un
`apploader.img` mínimo que copia el programa a memoria), así que **no hace falta
el juego ni el disco**: la puede correr cualquier máquina que tenga ModernGekko
compilado con el parche.

**Cómo se inventan las palabras.** Por cada instrucción que nombran las tablas
se arman palabras con todas las combinaciones de cuatro registros (`r0`, `r3`,
`r4` y `r5`) en los tres campos de operando, para que un mismo registro aparezca
dos veces o aparezca `r0`, y otras con operandos al azar: 127 867 palabras en la
semilla 1. Una palabra solo cuenta si es una instrucción definida. El juez es
Capstone, que no es perfecto y necesita cuatro ayudas:

- No conoce el bit de habilitación de desbordamiento de la familia `addo` ni el
  bit de registro de `mtfsb0.` y `mtfsb1.`; se le muestra la palabra sin ellos.
- No decodifica `fcmpo` ni `mcrxr`; sus bits reservados se revisan aparte.
- Sigue versiones de la arquitectura más nuevas que el manual del 750CL, y en
  ellas algunos bits que el manual deja reservados significan algo: el que vuelve
  de 64 bits una comparación (`cmpd`), la pista de `dcbt`, el nivel de `dcbf`, el
  tipo de barrera de `mbar`, los bits L y W de `mtfsf`, el W de `mtfsfi`, el bit
  11 que convierte `mfcr` y `mtcrf` en `mfocrf` y `mtocrf`, y los campos de
  `mffs` que lo vuelven `mffsl`, `mffscdrn` u otra variante. Una palabra que
  active uno no es una instrucción del Gekko: el recompilador se la deja al
  intérprete y el caso no compararía nada. `RESERVED_BITS_OF_FORMS`, en
  `tools/isa_conformance.py`, los enumera por forma; y el bit 0 es reservado en
  toda forma con código extendido que no lo usa (como bit de registro, de
  enlace, o parte del nombre).
- El código de operación de los pares de simples (4) es también el de AltiVec, y
  una palabra de par de simples con un bit reservado activo se lee a menudo como
  una instrucción AltiVec, que el Gekko no tiene. Capstone dice cuál leyó, y el
  nombre tiene que ser el de la forma.

Las formas con actualización (`lwzux`, `stwu`, ...) se ejecutan solo si son
válidas: con `rA` igual a `r0` la dirección sería `r0 + rB`, en cualquier lugar
de la memoria, y las cargas con actualización cuyo destino es el registro base
tampoco lo son.

**Resultado.** Seis semillas con 24 palabras por forma (cada una inventa sus
propias palabras y sus propios estados aleatorios):

| Semilla | Casos | Formas | Iguales | Distintos | Sin comparar |
|---:|---:|---:|---:|---:|---:|
| 1 | 6 837 | 316 | 6 752 | **0** | 85 |
| 2 | 6 850 | 316 | 6 774 | **0** | 76 |
| 3 | 6 843 | 316 | 6 760 | **0** | 83 |
| 4 | 6 854 | 316 | 6 770 | **0** | 84 |
| 5 | 6 823 | 316 | 6 744 | **0** | 79 |
| 6 | 6 838 | 316 | 6 760 | **0** | 78 |
| Total | 41 045 | 316 | 40 560 | **0** | 485 |

Los «sin comparar» (485) son todos de instrucciones que el módulo ejecuta
llamando al intérprete de Dolphin (`ppc_fallback_instruction`), así que no hay
código recompilado que comparar: `dcbf` (96), `dcbi` (95), `dcbst` (88) e `icbi`
(98), y `mfspr` (33) y `mtspr` (75) sobre SPRG0 a SPRG3 (registros 272 a 275) y,
solo `mfspr`, PVR (287). Son exactamente esos casos, ni uno más: los demás
registros especiales que el generador lee o escribe (XER, LR, CTR, SRR0, SRR1 y
GQR0 a GQR7) sí se comparan. En las seis corridas el verificador terminó con
`reports=0`, `skipped_zero=0`, `cap_hits=0`, `undercharges=0` y
`late_matches=0`.

**En otros procesadores y sistemas.** Las semillas 1 y 2 se repitieron en los
corredores de GitHub con el flujo `.github/workflows/isa-conformance.yml` (el mismo
código, con ModernGekko compilado allí). Dieron exactamente las cifras de la
corrida local para esas dos semillas (13 687 casos, 13 526 iguales, **0** distintos
y 161 sin comparar) en cuatro combinaciones:

| Sistema y procesador | Compilador del módulo recompilado |
|---|---|
| Linux, x86-64 | GCC |
| Linux, ARM64 (aarch64) | GCC |
| Windows, ARM64 | MSVC (Visual Studio, `cl.exe` 19.51) |
| Windows, ARM64 | Clang de llvm-mingw (el compilador portátil que lleva [el paquete para Windows](../packaging/windows/README.md)) |

Que ARM64 coincida importa porque los procesadores ARM y x86 difieren justo en los
casos límite del punto flotante (NaN, conversiones que desbordan, denormales), y
esta prueba ejercita valores de borde en cada instrucción. Que coincida Windows con
MSVC importa porque es el sistema y el compilador que usará la laptop, y porque
MSVC no tiene `-ffp-contract=off` sino `/fp:strict` (ver «Qué NO demuestra»). El
compilador portátil importa porque es el que usará el jugador: el paquete no lleva
Visual Studio, y el módulo del juego se compila en su computadora, a partir de su
propio disco, con llvm-mingw. Las dos compilaciones en Windows tardan distinto
(unos tres minutos por semilla con MSVC y uno con llvm-mingw) pero el resultado es
el mismo.

**Lo que apareció por el camino.** La prueba no salió limpia a la primera, y cada
tropiezo enseñó algo:

1. **Un programa que se colgó (error del generador, no del recompilador).** La
   primera corrida completa no terminó. Tras el último caso comparado venía
   `lbzux r0, 0, r0`, una forma con actualización cuyo registro base es `r0`: el
   manual la llama inválida, y la dirección que calcula el procesador es `r0 +
   rB`, que puede ser cualquiera (aquí no estaba mapeada). La carga produce una
   excepción, el procesador salta al vector de excepciones, donde el programa de
   prueba no tiene código, y se queda ahí. El generador ya no da esas formas, y el
   ejecutor detecta el cuelgue: `main` anota en la memoria el número del grupo de
   casos que va a correr, y si el programa pasa `--stall-timeout` segundos (300
   por defecto) en un mismo grupo, la corrida se detiene y lista los casos de ese
   grupo.

2. **Un error del intérprete de Dolphin: el acarreo de `addme` y `subfme`.**
   «Sumar a menos uno» calcula `rA + CA - 1`, es decir, `rA + 0xFFFFFFFF + CA`.
   Con `CA = 1` la suma es `rA + 2^32` y siempre acarrea. El intérprete
   (`Interpreter_Integer.cpp`, `addmex` y `subfmex`) calcula el acarreo de salida
   como `Helper_Carry(a, carry - 1)`, y `Helper_Carry(a, b)` es `b > ~a`: con
   `CA = 0` eso es `0xFFFFFFFF > ~a`, que es cierto si y solo si `a > 0` (bien),
   pero con `CA = 1` es `0 > ~a`, que nunca lo es. El código C recompilado hace
   `(u64)rA + 0xFFFFFFFF + carry` y toma el acarreo del bit 32, que está bien en
   los dos casos:

   | `rA` | `CA` antes | Resultado | `CA` después según el manual | Intérprete de Dolphin |
   |---|---|---|---|---|
   | `0` | 0 | `0xFFFFFFFF` | 0 | 0 |
   | `5` | 0 | `4` | 1 | 1 |
   | `5` | 1 | `5` | 1 | **0** |
   | `0` | 1 | `0` | 1 | **0** |

   Las mismas líneas están hoy en la rama `master` de Dolphin. El juego no usa
   ninguna de las dos instrucciones, así que no le afecta. El generador las
   ejecuta con el acarreo en cero (un `addic` de cero a `r0` lo borra sin
   cambiar nada más), que es donde coinciden.

3. **Accesos desalineados (49 de las 53 diferencias de la segunda corrida).** El
   intérprete (`Interpreter_LoadStore.cpp`) levanta una excepción de alineación
   cuando la dirección de `lfs`, `lfsu`, `lfd`, `lfdu`, `stfs`, `stfsu`, `stfd`,
   `stfdu`, `lmw` o `stmw` no es múltiplo de 4 (`lmw` y `stmw` también con el
   procesador en modo little-endian). El código recompilado hace el acceso.
   Ningún programa que funcione en la consola hace eso, así que al juego no le
   afecta, pero los dos no se pueden comparar; el generador ahora baja el registro
   base el resto del desplazamiento para que la dirección sea múltiplo de 4. Los
   accesos de enteros no cambian: el procesador resuelve solo los desalineados.

4. **Una laguna del recompilador: `mtfsb1` y el bit FX (las otras 4).** En el
   intérprete, `mtfsb1` sobre un bit de excepción del FPSCR (OX, UX, ZX, XX y los
   de operación inválida: bits 3 a 12 y 21 a 23) llama a `SetFPException`, que
   enciende también FX (si el bit no estaba ya encendido) y recalcula VX y FEX; el
   código fuente lo avisa con «This instruction can affect FX». El C que genera
   DolRecomp solo enciende el bit:

   ```c
   u32 mask = 0x80000000u >> 18;
   if (18 != 1 && 18 != 2) ctx->fpscr |= mask;
   ```

   Es una laguna del recompilador, no un comportamiento indefinido. El juego solo
   ejecuta `mtfsb1 29` (el bit NI), que no es de esos, así que no le afecta, y el
   generador no da `mtfsb1` sobre esos 13 bits hasta que se corrija en DolRecomp
   (todavía no se avisó a sus autores). `mtfsb1` sobre cualquier otro bit y
   `mtfsb0` sobre todos coinciden.

5. **Un error de disposición de la memoria en el generador (la campaña grande).**
   La primera campaña de seis semillas con 24 palabras por forma dio unas 370
   diferencias por semilla, todas en las formas que quedan al final del programa
   (en la semilla 2, de `lfdu` a `stfsu`) y todas en el XER y el FPSCR: el código
   recompilado conservaba bits que el intérprete borra o recalcula (los reservados
   del XER, el bit 20 del FPSCR y algún bit derivado como FEX o VX), igual que con
   el `mtxer` y el `mtfsf` de la sección anterior. Pero esta vez los casos no
   habían recibido el estado que el generador escribió. La tabla de estados, con
   una fila de 656 bytes por grupo de cuatro casos, llegaba a 1,1 MB, y solo había
   1 MB entre ella y la memoria de trabajo de las cargas y escrituras; el cargador
   copió esa memoria, de bytes aleatorios, sobre un centenar de filas del final de
   la tabla, y los casos que las usaban arrancaron con un estado cualquiera. No era
   un error del recompilador, sino mío, y la corrida corta (463 filas, 0,3 MB) no
   podía verlo. Ahora la memoria de trabajo queda 3 MB más lejos (caben unos 25 000
   casos) y `build_program` comprueba que ninguna de las secciones del programa (el
   código, la tabla, la memoria de trabajo y las palabras de control) invada a la
   siguiente; si no caben, se detiene con un mensaje que dice qué cambiar
   (`--words-per-form` o `--group-size`). Las cifras de arriba son de después de
   la corrección.

6. **Un juez que dejaba pasar palabras que no son del Gekko.** Con la
   disposición corregida, el decodificador de DolRecomp no conocía unas cuatro
   palabras de cada semilla: son palabras con un bit reservado que Capstone, que
   sigue arquitecturas más nuevas, lee como otra instrucción (`mfocrf`,
   `mtocrf`, `mffscdrn`, ...). DolRecomp las deja como un comentario `.long` sin
   traducir, así que esos casos no comparaban nada. Para encontrarlas todas se
   dieron al decodificador las palabras de 60 semillas (más de 400 000 casos) y
   2,1 millones más con operandos al azar, y se miraron las que Capstone aceptaba
   y DolRecomp no: eran todas de `mfcr`, `mtcrf`, `mffs` y `mtfsfi`. Ya están en
   `RESERVED_BITS_OF_FORMS`, y la misma búsqueda no encuentra ninguna más. Por eso
   las cifras de arriba son de después de este arreglo también.

**Controles.** La autoprueba del fallo inyectado funciona también aquí:
`run_isa_conformance.py --synthetic --seeds 1 --words-per-form 6 --inject-fault lwz`
hizo que el verificador cambiara un bit de `r3` al final del primer caso `lwz`
(`lwz r8, 0x2d23(r5)`), y la corrida reportó ese caso y ningún otro:

```
seed 1: entry=0x80015E74  lwz: lwz r8, 0x2d23(r5)  [r3:N=0x80,I=0x90]
self-check passed: the verifier reported the injected case and nothing else
```

**Lo que esta variante no cubre, además de lo de la prueba anterior.**

- El juez de «¿esto es una instrucción?» es imperfecto. Si deja pasar una palabra
  que el Gekko no tiene, el recompilador la deja al intérprete y el caso sale «sin
  comparar» (por eso importa ese número); si rechaza una que sí es válida, esa
  palabra no se prueba, y nada lo avisa.
- Las exclusiones de arriba (el acarreo de `addme` y `subfme`, los accesos
  desalineados, `mtfsb1` y las formas inválidas) son diferencias conocidas, no una
  garantía de que no haya otras que el generador esquiva sin querer.
- Cada caso es una sola instrucción con un estado aleatorio: no prueba secuencias
  de instrucciones ni cómo se encadenan los bloques.
- Corrió en x86-64 (en local y en GitHub), en ARM64 con Linux y GCC y en
  Windows ARM64 con MSVC y con llvm-mingw (estas tres, en GitHub), siempre con
  las mismas cifras. No corrió en la laptop del jugador: los corredores de GitHub son
  máquinas virtuales ARM64 de un servidor, y aunque las dos son ARM64, el
  procesador exacto puede cambiar. Tampoco se probó con varias versiones de cada
  compilador: en Windows fueron MSVC 19.51 y Clang 23.1, y otra versión podría
  optimizar de otra manera.

### Cómo repetirlo

Una vez, aplicar el parche y recompilar el ejecutable
([patches/README.md](../patches/README.md)). Después, cada vez:

```bash
STATICRECOMP_LOCKSTEP=1 STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1 \
STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1 \
STATICRECOMP_LOCKSTEP_COVERAGE_FILE="$PWD/coverage.txt" \
    scripts/build_and_run_linux.sh 2>&1 | tee lockstep.log
python3 tools/lockstep_report.py lockstep.log        # divergencias por causa
python3 tools/lockstep_coverage.py coverage.txt      # cuánto se comparó
```

Hay que **cerrar el juego con normalidad** (la ventana): el resumen y el
archivo de cobertura se escriben al salir. Va despacio, porque cada bloque
nuevo se ejecuta dos veces; con el juego ya «caliente» la velocidad vuelve a
ser la de siempre. Más variables del verificador (todas opcionales):

| Variable | Qué hace |
|---|---|
| `STATICRECOMP_LOCKSTEP_START`, `_LIMIT` | Solo verifica entre esos números de despacho nativo (`0` = sin límite). |
| `STATICRECOMP_LOCKSTEP_FILTER=texto` | Solo imprime las divergencias cuya línea contiene el texto; el resto se cuenta en `filtered`. |
| `STATICRECOMP_LOCKSTEP_NODEDUP=1` | Verifica **cada** visita a un bloque y no solo la primera (mucho más lento). |
| `STATICRECOMP_LOCKSTEP_REPEAT=0xDIRECCIÓN` | Verifica cada visita, pero solo del bloque que empieza ahí. |
| `STATICRECOMP_LOCKSTEP_MAXREPORT=n` | Imprime a lo sumo `n` divergencias (`0` = todas). |
| `STATICRECOMP_LOCKSTEP_STEPCAP=n` | Máximo de pasos del intérprete por bloque (20 000 por defecto). |
| `STATICRECOMP_LOCKSTEP_TRACE=0xDIRECCIÓN` | Imprime paso a paso lo que hace el intérprete en el bloque que empieza ahí. |
| `STATICRECOMP_LOCKSTEP_WHITELIST=0xA,0xB` | Nunca reporta los bloques que empiezan en esas direcciones. |

Si sale una divergencia nueva, `lockstep_report.py` la deja en `control_flow` o
en `unexplained` y la imprime entera: esas son las que hay que leer. El flujo es
mirar `entry` y `end` con `tools/disasm.py`, repetir solo ese bloque con
`_TRACE` y comparar, instrucción por instrucción, lo que hace el intérprete
con lo que dice el código C generado.

**La prueba por instrucción** se repite con un solo comando. Necesita el disco
extraído en `orig/G4AEE9`, ModernGekko compilado con el parche y, si no hay
pantalla, `xvfb` (`sudo apt install xvfb`):

```bash
python3 tools/run_isa_conformance.py --template ~/ModernGekko-Template --seeds 1 2 3
python3 tools/run_isa_conformance.py --template ~/ModernGekko-Template --seeds 7 --inject-fault lwz  # autoprueba
```

Cada semilla tarda de dos a cuatro minutos con dos núcleos. Deja en
`build/isa_conformance/seed_N/` el programa de prueba, la lista de casos
(`cases.tsv`), el informe (`report.txt`) y el registro del verificador. El
código de salida es 0 si ningún caso difirió, 1 si alguno difirió y 2 si falló
la preparación (falta una herramienta, el compilador o la pantalla) o si el
verificador no comparó ningún caso: el verificador se apaga solo, sin hacer
fallar la corrida, cuando el ejecutor no se compiló con el parche o cuando el
módulo no exporta `ppc_set_mem_write_journal`, y una corrida así no prueba nada
aunque salga "sin diferencias". Las opciones están en `--help`.

Con `--headless` el ejecutor no abre ninguna ventana, así que no hace falta
pantalla, ni real ni virtual (ni `xvfb`); es lo que usan las corridas de Windows en
GitHub. En Windows y macOS, que tienen su propio sistema de ventanas, tampoco se
inicia una pantalla virtual.

Otras tres opciones sirven para probar lo que lleva el paquete de Windows en vez de
una compilación propia de ModernGekko:

- `--tools-directory CARPETA` en lugar de `--template`: una carpeta que tiene
  `moderngekko-port` y `moderngekko-run` lado a lado, como la `bin` del paquete.
  El compilador, CMake, Ninja y Python los busca `moderngekko-port` en la `PATH`,
  así que para probar el paquete tal cual se le pone una `PATH` con solo las
  carpetas de `toolchain` del paquete y las de Windows (lo hace el flujo
  `package-windows-arm64.yml`).
- `--toolchain clang` (o `gcc`, o `msvc`) elige el compilador del módulo
  recompilado. `auto` toma el primero que encuentra.
- `--synthetic-game-id ID`, con `--synthetic`: el identificador del disco inventado.
  El paquete se compila para un solo juego y rechaza cualquier otro, así que para
  probarlo hay que decirle que el disco inventado es `G4AEE9`.

Ejemplo, desde PowerShell, con el paquete desempaquetado en `C:\hm`:

```powershell
$python = (Get-Command python).Source   # el Python que tiene Capstone (requirements.txt), no el del paquete
$package = 'C:\hm\HarvestMoonMM-windows-arm64'
$toolchain = "$package\toolchain"
$env:PATH = "$toolchain\llvm-mingw\bin;$toolchain\cmake\bin;$toolchain\ninja;$toolchain\python;$env:SystemRoot\System32;$env:SystemRoot"
$env:Python3_ROOT_DIR = "$toolchain\python"
& $python tools\run_isa_conformance.py --synthetic --headless --synthetic-game-id G4AEE9 `
    --tools-directory "$package\bin" --toolchain clang --seeds 1
```

**La variante sin el juego** pide lo mismo menos el disco y los símbolos:

```bash
python3 tools/run_isa_conformance.py --synthetic --template ~/ModernGekko-Template --seeds 1 2 3
python3 tools/run_isa_conformance.py --synthetic --template ~/ModernGekko-Template --seeds 7 --inject-fault lwz  # autoprueba
```

Con las 24 palabras por forma de siempre cada semilla tarda unos 5 minutos con dos
núcleos; con `--words-per-form 6`, unos dos. Si el programa se cuelga, el ejecutor lo
nota cuando pasa `--stall-timeout` segundos (300 por defecto) en un mismo grupo de
casos: se detiene y lista los casos de ese grupo. Y si las palabras no caben en la
memoria del programa, no lo arma y dice qué cambiar (`--words-per-form` o
`--group-size`).

También se puede lanzar en GitHub, sin instalar nada: pestaña **Actions** →
**Instruction conformance** → **Run workflow**. El flujo
(`.github/workflows/isa-conformance.yml`) compila ModernGekko (de 20 a 40
minutos) y corre la prueba en Linux x86-64, en Linux ARM64 y en Windows ARM64
(en este último, dos veces: con MSVC y con el compilador portátil llvm-mingw); los
informes quedan como archivos adjuntos de la ejecución.

El paquete de Windows ARM64 se prueba en GitHub con otro flujo,
`.github/workflows/package-windows-arm64.yml` (**Actions** → **Windows ARM64
package** → **Run workflow**), siempre con un disco inventado y sin datos del
juego. Comprueba, en este orden: que los programas del paquete sean ARM64 y
arranquen con solo las carpetas de Windows en la `PATH`; esta misma prueba por
instrucción, con el compilador del paquete; `play.ps1` dos veces, con Windows
PowerShell 5.1 y con las rutas largas desactivadas, como en la mayoría de las
computadoras (la primera lee el disco, lo extrae, recompila e instala el módulo;
la segunda debe encontrar todo hecho; las dos construyen e instalan los mods de
`game.psd1`; el camino más largo que hizo medía 253 caracteres, de los 259 que
Windows permite así); y que el juego instalado arranque como lo hace el launcher,
sin dibujar nada y sin nada del compilador en la `PATH`, cargue esos mods, siga
corriendo unos segundos y se cierre con código 0. **No** comprueba el juego real,
la ventana ni la GPU (DirectX 12 o Vulkan con el controlador de Snapdragon), el
control ni el teclado, ni una computadora de verdad. Lo que sí se probó a mano en
una laptop con Snapdragon (el juego real, con DirectX 12) está en
[PERFORMANCE.md](PERFORMANCE.md).

### Qué NO demuestra

- **Solo comprueba el código al que llegó la corrida.** «Cero divergencias»
  vale tanto como lo que cubre; los números están en «Cobertura».
- **Cada bloque se comprueba una vez**, la primera vez que corre, con los
  registros y la memoria de ese momento, y la cobertura solo cuenta el camino
  que el intérprete recorrió entonces. Un bloque que se equivoca solo con otros
  valores, o en una rama que esa visita no tomó, no se vería (`_NODEDUP` cubre
  lo primero, a un costo enorme; la prueba por instrucción, las instrucciones
  de esas ramas).
- **La referencia es el intérprete de Dolphin.** Si el intérprete se
  equivocara de la misma manera, los dos coincidirían. Es una referencia muy
  probada, pero es una referencia, no una especificación, y no es perfecta: la
  prueba sin el juego le encontró un error de acarreo (`addme` y `subfme`, arriba).
- **La aceptación en «cualquier llegada» no distingue cuántas iteraciones**
  hace un bucle que sale por presupuesto de ciclos. Eso lo decide un
  convenio de tiempo y no la lógica del juego, y lo que sí se exige es que el
  estado sea idéntico en la iteración en que sale, con todas las escrituras
  a memoria, pero un error que solo cambie el número de iteraciones no se
  vería.
- **El tiempo es aproximado.** El código recompilado cobra menos ciclos que el
  intérprete en algunos bucles (en el de `0x80185568`, alrededor del 65 % por
  iteración). Los valores que calcula son los mismos, pero el reparto exacto
  de ciclos entre instrucciones y eventos de hardware no.
- **No cubre el hardware.** El video (GX), el audio (DSP), el disco, la
  tarjeta de memoria y los controles son los de Dolphin tanto con el código
  recompilado como sin él; el verificador compara qué escribe la CPU al
  hardware, no cómo responde.
- **El modo de redondeo del procesador anfitrión.** El verificador corre el
  código recompilado y el intérprete en el mismo procesador y con la misma
  configuración de punto flotante, así que no puede ver si el código
  recompilado deja de cambiarla. El C que genera DolRecomp no la cambia cuando
  el juego escribe el FPSCR con `mtfsf`, `mtfsfi`, `mtfsb0` o `mtfsb1` (solo lo
  hacen las funciones auxiliares del tiempo de ejecución y el generador LLVM),
  de modo que el redondeo del anfitrión se queda en «al más cercano». Al juego
  no le afecta mientras no pida otro: el ejecutable escribe el FPSCR en cinco
  instrucciones (cuatro `mtfsf` y un `mtfsb1`) que inicializan los registros de
  punto flotante al arrancar (`__OSFPRInit`), restauran un contexto, cambian
  bits de excepciones a partir de un valor leído con `mffs` o encienden el bit
  NI (`mtfsb1 29`, en `OSInit`; el tiempo de ejecución emula ese bit por
  software), y por lo que vi ninguna elige un redondeo propio. No lo he
  comprobado con el juego en marcha.
- **Hay áreas enteras del juego sin visitar:** las tiendas, el pueblo, los
  minijuegos y los festivales, y también el comienzo de un día nuevo y el final
  de un evento, donde corre la revisión del campo (`fn_80166888`, la función más
  grande que ninguna corrida ejecutó).
- **Las corridas que usan el juego fueron todas en x86-64.** La prueba por
  instrucción sin el juego sí se repitió en ARM64 (Linux con GCC, y Windows con
  MSVC y con llvm-mingw) con las mismas cifras. El C generado es el mismo en
  ARM64, pero lo compilan otros compiladores, y los procesadores ARM difieren de
  los x86 justo en los casos límite del punto flotante (NaN, conversiones que
  desbordan, denormales): eso sí lo cubre la prueba por instrucción, porque
  ejercita esos valores en cada instrucción. Lo que no cubre es que un compilador
  fusione una multiplicación y una suma en una sola operación, que redondea
  distinto de la de PowerPC: eso necesita dos instrucciones seguidas y cada caso de
  la prueba es una sola. El módulo se compila con `-ffp-contract=off` en GCC y
  Clang y con `/fp:strict` en MSVC, que desde Visual Studio 2022 17.0 [no genera
  contracciones](https://devblogs.microsoft.com/cppblog/the-fpcontract-flag-and-changes-to-fp-modes-in-vs2022/)
  (así lo fija `moderngekko_port.cpp`), pero ninguna corrida con el juego en
  Windows ARM64 lo ha comprobado. Correr el verificador con el juego en esa
  laptop sigue siendo la comprobación más útil que queda.

---

## English

The recompilation turns every PowerPC instruction of the game into C. This
document explains how to check that the C does what the processor does, what
has been checked so far and, just as important, what has not. There are two
checks that complement each other: the lockstep verifier, which compares the
recompiled code with Dolphin's interpreter while the game is played, and a test
of every instruction on random operands, in two variants: with the words the game
uses and with made-up words, which reach almost the whole processor and do not
need the game.

### How it is checked

ModernGekko ships a "lockstep" verifier that compares the recompiled code with
Dolphin's CPU interpreter, which is the reference. With
`STATICRECOMP_LOCKSTEP=1`, **the first time** the game runs each block of
recompiled code:

1. it saves the CPU state at entry and starts noting what the block writes;
2. it lets the recompiled block run;
3. it goes back to the entry state and runs the same block on Dolphin's
   interpreter, one instruction at a time, up to the address where the
   recompiled block handed control back;
4. it compares the two end states: r0–r31, the floating-point registers (both
   halves of each), LR, CTR, CR, XER, FPSCR, MSR, SRR0, SRR1 and PC; every
   byte the block wrote to RAM, to the locked cache and to Dolphin's fake
   "virtual memory"; and the sequence of hardware (MMIO) writes and reads;
5. it leaves memory as the recompiled block left it and the game goes on.

Every difference is printed in the log as `[lockstep] DIVERGE`, and when the
game closes a `[lockstep] summary:` line gives the counters. A "block" is what
the recompiled code runs between two returns of control to the dispatcher; it
is not a classic basic block. Not compared: the 25 blocks that contain
instructions the module runs by calling Dolphin's interpreter (`mtspr`,
`dcbi`, ...: in a normal game too) and the hundred or so that charge zero
cycles.

### What was found

| Run | What was played | Verifier | Blocks compared | Divergences |
|---|---|---|---:|---:|
| A | saved game in the house → house and farm | stock | 7,593 | 136 |
| B | cold boot → intro → title → new game | stock | 11,629 | 161 |
| D | B, plus house, farm and road | patched | 11,296 | **0** |
| E | repeat of D | patched | 11,286 | **0** (378 late matches) |
| F | D, plus a random player in the house, farm and road (no video) | patched | 12,195 | **0** (392 late matches) |

"Stock" is the verifier ModernGekko ships; "patched" is the one from
[patches/moderngekko/lockstep-verifier.patch](../patches/README.md). In the five
runs of the table: `fallback=0` (nothing ran outside the recompiled module),
`smc_failed=0`, `cap_hits=0` (the interpreter always finished the block) and
`undercharges=0`. Run C is not in it: it tried another idea (stopping the
interpreter when it matches the cycles charged), gave 72 divergences and 8
blocks that hit the step cap, and I dropped it for the "any arrival" approach.

Run F is the only one that saved the coverage file (below). It repeats D's
route and then lets a random player play (stick, buttons and C-stick at random,
seed 5, reloading the house save every 100 actions so it cannot get stuck) with
`--graphics Null`, which draws nothing and so runs faster. I stopped it after
37 minutes, about 200 of its 1,500 planned actions, to measure coverage.

The 136 and 161 divergences of the stock verifier have three causes, and none
is a calculation error of the recompiled code:

| Cause | A | B | What it is |
|---|---:|---:|---|
| `fpscr_result_flags` | 122 | 122 | Only the FPSCR differs, and only in FPRF, FR and FI (mask `0x0007F000`), which every floating-point instruction rewrites with a description of its result. In every case the recompiled code leaves one FPCC bit set and the interpreter one to three. The game reads the FPSCR in three places (`mffs`, in the SDK code that saves and restores the floating-point context): the value is stored, masked with `0x6005F8FF` or returned, and written back with `mtfsf`; none of those places decides anything with the result bits. There is no `mcrfs` in the executable. |
| `end_inside_loop` | 12 | 36 | The block ends inside a loop. Recompiled blocks leave an inlined loop or recursion when a cycle budget runs out (`DOLRECOMP_C_LOOP_CYCLE_BUDGET`) and return to the dispatcher at the loop header. The verifier's interpreter stopped the **first** time it reached that address, and the two can stop on different iterations of the same loop, with different registers. |
| unexplained | 2 | 3 | The same, in loops that contain calls or recursion (`fn_800CE9CC`; `fn_80086C7C` → `fn_8008477C`), where my classifier does not recognise the loop. |

The patch removes the last two causes at the root: with
`STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` the interpreter runs on past the first
arrival and the block passes if the recompiled state is **exactly** equal to
the interpreter's at *any* arrival (every register, every byte written and the
whole MMIO sequence), and with `STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1`
it does not compare those three bits. Each block that only passed at a later
arrival is printed as `[lockstep] LATE-MATCH`; run E has 378, and among them
are three of the five blocks that previously had no explanation (entries
`0x800CECB4`, `0x80086D9C` and `0x80086DC8`). In 339 of the 378 the interpreter needs
between 200 and 320 steps to reach the match, which is what a cycle-budget
exit corresponds to. Among the other 39 there are 18 with fewer than 100
steps; the ones I checked by hand (`0x801C3CC0`, `0x8017079C`) are wait loops
that read a memory or hardware flag until it changes, and their number of
turns depends on timing, not on the game's logic.

Two of the five blocks without an explanation remain (`0x800CEAC4` and
`0x800CED10`, in `fn_800CE9CC`, which calls itself). They differ in `r1` by
exactly `0x30`, the size of that function's stack frame, and in the registers
that hold per-level data: the recompiled code and the interpreter stopped one
recursion level apart, which is the same case. But no later run reported them,
neither as a divergence nor as a late match, so I have not seen the patched
verifier accept those two particular blocks. Their instructions were compared
without differences in run F.

**Negative controls.** A verifier that never reports anything proves nothing,
so a fault was injected on purpose (`STATICRECOMP_LOCKSTEP_INJECT_FAULT`: one
bit of `r3` flipped in the recompiled end state of one block) in a
straight-line block and in a loop block, with the patch options on, and both
times it was reported:

```
[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: r3:N=0x804f2468,I=0x804f2478
[lockstep] DIVERGE #1 entry=0x801C49CC end=0x801C32C0: r3:N=0x10,I=0x0
```

### Coverage

`tools/lockstep_coverage.py` reads the coverage file of one or more runs and
says how much of the game was really compared. An instruction counts as
compared when the interpreter ran it inside a block that matched on its first
visit; those on a branch the block did not take on that visit do not count.
Run F, the only one that saved the file, compared:

| Measure | Result |
|---|---:|
| Instructions | 115,028 of 449,523 (25.6%) |
| Functions with at least one instruction compared | 2,207 of 4,486 (49.2%) |
| Instruction forms compared at least once | 151 of 171 |
| Forms compared at two sites or fewer | 14 |

The 20 forms no run compared are `eqv`, `psq_lx`, `mfsr`, `mtsr`, `dcbi`,
`dcbf`, `creqv`, `lfsu`, `subfze`, `icbi`, `twi`, `stfdu`, `dcbst`, `andis.`,
`mffs`, `mcrxr`, `lfdu`, `stfdx`, `rlwnm` and `bla`. A bug in the translation
of any of them would not have shown up, and the instruction-level test of the
next section is there for that.

Most of the game was not touched. The biggest function no run ever executed is
`fn_80166888` (6,990 instructions), followed by `fn_80125564` (3,050) and
`fn_80091B10` (2,191).

`fn_80166888` is the "field" check: the game uses it to decide whether a story
event starts, among them those of the wedding. According to the code, state 0x28
calls it and moves straight on to the next state, so it runs once each time the
game enters that state (when a new day starts and when an event ends) and not
while the character walks. I checked it with the
[`state_logger`](../mods/state_logger/mod.c) mod, which only logs state changes:
in about 8,000 frames of walking and pressing buttons, starting in the house, the
game stayed in state 0 and the check was never entered. Going to bed took it
through states 0x12, 0x13, 7, 0x29, 9, 0x26 and 0x25; then the check was entered
and state 0x28 was followed at once by state 4, the one that waits for an event
to end. (The mod logs a state change one frame after it happens, so the check's
line comes before the one announcing 0x28, and it may be printed twice: a hook
can repeat within a single call.) No verified run reached a new day or the end
of an event, so that part of the game was left uncompared.

### Instruction-level test

The coverage above says playing compares a quarter of the game's instructions.
`tools/isa_conformance.py` attacks that gap from the other side: instead of
asking "what code does a playthrough execute?", it asks "does every
instruction of the game do what the processor does, with operands of every
kind?".

1. It walks the game's functions and gathers the distinct instruction words of
   each of the 171 forms (`lwz`, `fmadds`, `ps_sum0`, ...): 103,649 words.
2. From each form it takes up to 24, **exactly as the game has them** (same
   registers, displacements and masks). It deals them out first among the
   register combinations the game uses, that is, which operands are the same
   register and which are `r0`, because a translation that writes its result
   before it has read every operand only goes wrong when an operand is the
   destination (`lwz r3, 8(r3)`), and the game has few such words. In the 157
   forms that can be run the game has 581 combinations and each run tests 579;
   the two left out cannot be run as they are (an `lwz` without a base register
   would read the bottom of memory, and an `lmw` whose register range includes
   its own base is an invalid form).
3. It builds a PowerPC program out of them, an ordinary DOL, with one case per
   word: the instruction, the few instructions that point a register at
   scratch memory if it is a load or a store, and `blr`.
4. Before each group of cases it loads a complete random state: the 32 general
   registers, CR, XER, the FPSCR rounding mode and the 32 floating-point
   registers, with frequent edge values (0, 1, `0x7FFFFFFF`, `0x80000000`,
   ±infinity, NaN, denormals...), and 64 KB of random memory.
5. `moderngekko-port` recompiles the program as if it were the game and the
   lockstep verifier compares each case with Dolphin's interpreter. Since every
   case ends in `blr`, it is a whole block and is compared on all registers and
   everything it wrote to memory.

`tools/run_isa_conformance.py` does the five steps with a single command.

**Result.** 10 seeds (each one is another program with other random
states):

| Cases | Forms | Equal | Different | Not compared |
|---:|---:|---:|---:|---:|
| 26,450 | 157 | 26,181 | **0** | 269 |

The "not compared" are those of `dcbf`, `dcbi`, `dcbst` and `icbi`, and those of
`mfspr` and `mtspr` on SPRG0 to SPRG3 (registers 272 to 275) and, `mfspr` only,
PVR (287): the module runs them by calling Dolphin's interpreter
(`ppc_fallback_instruction`), so there is no recompiled code to compare. In all
ten runs the verifier ended with `reports=0`, `skipped_zero=0`, `cap_hits=0`,
`undercharges=0` and `late_matches=0`.

Before the words were dealt out by register pattern, six seeds with randomly
chosen words (15,864 cases) had not shown differences either.

**Three differences that did show up at first, and that are not recompiler
bugs.** All three are cases the manual leaves undefined, and the generator no
longer gives them:

- `fmadds` and the other single-precision instructions, given a double operand
  that does not fit in a single: the manual says the result is undefined, and
  Dolphin and the recompiled code differ in the payload of a NaN. Those
  instructions now only receive operands that are singles.
- `mtxer` with reserved bits: the recompiled code keeps all 32 bits written and
  the interpreter clears the reserved ones. The game only writes what it read
  with `mfxer`. Now only SO, OV, CA and the byte count are written.
- `mtfsf` with bit 20 of the FPSCR (reserved): the same, for the same reason.
  Now the source register first goes through a mask that clears it.

The third was found by the self-check below, because a run that should have
reported one case reported two.

**Controls.** A test that never fails proves nothing:

- *Injected fault.* `run_isa_conformance.py --inject-fault lwz` makes the
  verifier flip a bit of `r3` at the end of the first `lwz` case, and the run
  has to report that case and no other. It ends with `self-check passed` or
  `self-check FAILED`.
- *Code broken by hand.* In the generated C of a program, `u32 res = a + b;`
  was changed to `a - b;` in one `add` and the carry bit of one `addc` was
  inverted. After recompiling, the run reported those two cases and no other,
  and the other 2,616 came out equal:

  ```
  entry=0x800164F4  add: add r3, r28, r0  [r3:N=0xffffff72,I=0x54]
  entry=0x80016674  addc: addc r4, r0, r28  [xer:N=0xa0000009,I=0x80000009]
  ```

**What this test does not cover.** It runs no jumps, calls or returns, so how
blocks are chained and how they leave a loop is only checked by playing. It
does not test the 14 control-flow, trap and system forms either (`b`, `bc`,
`bcctr`, `bcctrl`, `bclr`, `bclrl`, `bl`, `bla`, `mftb`, `mtmsr`, `mtsr`, `rfi`,
`sc` and `twi`), and of those, no gameplay run compared `bla`, `mtsr` or `twi`
either. And each seed gives one random state to each group of cases: a bug that
only appears with a very rare combination of values might need more seeds.

### Instruction-level test without the game

The test above only exercises the 171 instruction forms that the game uses. The
project's opcode tables name 349. With `--synthetic`, the test program is built
from **made-up** words that reach 345 of them (the other four, `eciwx`, `ecowx`,
`lswx` and `stswx`, are not read by Capstone, which acts as the judge, and stay
untested). 316 can be run; the other 29 are jumps, system calls and traps, the
instructions that change the memory map or read the clock, and a few that the
game does not use and the generator does not run (the byte-string ones `lswi`
and `stswi`, the reservation pair `lwarx` and `stwcx.`, and `dcba` and
`dcbz_l`). The three files in `sys/` are made up as well
(`tools/synthetic_disc.py`: `boot.bin`, `bi2.bin` and a minimal `apploader.img`
that copies the program into memory), so **neither the game nor the disc is
needed**: any machine with ModernGekko built with the patch can run it.

**How the words are made up.** For every instruction the tables name, it builds
words with every combination of four registers (`r0`, `r3`, `r4` and `r5`) in
the three operand fields, so that one register shows up twice or `r0` shows up,
and others with random operands: 127,867 words in seed 1. A word counts only if
it is a defined instruction. The judge is Capstone, which is not perfect and
needs four helps:

- It does not know the overflow-enable bit of the `addo` family nor the record
  bit of `mtfsb0.` and `mtfsb1.`; it is shown the word without them.
- It does not decode `fcmpo` or `mcrxr`; their reserved bits are checked
  separately.
- It follows later versions of the architecture than the 750CL manual, and in
  them some bits that the manual keeps reserved mean something: the one that makes
  a compare 64 bits wide (`cmpd`), the hint of `dcbt`, the level of `dcbf`, the
  kind of barrier of `mbar`, the L and W bits of `mtfsf`, the W bit of `mtfsfi`,
  the bit 11 that turns `mfcr` and `mtcrf` into `mfocrf` and `mtocrf`, and the
  fields of `mffs` that make it `mffsl`, `mffscdrn` or another variant. A word
  that sets one is not a Gekko instruction: the recompiler leaves it to the
  interpreter and the case would compare nothing. `RESERVED_BITS_OF_FORMS`, in
  `tools/isa_conformance.py`, lists them by form; and bit 0 is reserved in every
  form that has an extended opcode and no use for it (as a record bit, a link bit,
  or part of the name).
- The paired-single opcode (4) is also AltiVec's, and a paired-single word with a
  reserved bit set is often read as an AltiVec instruction, which the Gekko does
  not have. Capstone says which one it read, and the name has to be the form's own.

The update forms (`lwzux`, `stwu`, ...) are run only if they are valid: with `rA`
equal to `r0` the address would be `r0 + rB`, anywhere in memory, and the update
loads whose destination is the base register are invalid too.

**Result.** Six seeds with 24 words per form (each one makes up its own words and
its own random states):

| Seed | Cases | Forms | Equal | Different | Not compared |
|---:|---:|---:|---:|---:|---:|
| 1 | 6,837 | 316 | 6,752 | **0** | 85 |
| 2 | 6,850 | 316 | 6,774 | **0** | 76 |
| 3 | 6,843 | 316 | 6,760 | **0** | 83 |
| 4 | 6,854 | 316 | 6,770 | **0** | 84 |
| 5 | 6,823 | 316 | 6,744 | **0** | 79 |
| 6 | 6,838 | 316 | 6,760 | **0** | 78 |
| Total | 41,045 | 316 | 40,560 | **0** | 485 |

The "not compared" (485) are all instructions that the module runs by calling
Dolphin's interpreter (`ppc_fallback_instruction`), so there is no recompiled
code to compare: `dcbf` (96), `dcbi` (95), `dcbst` (88) and `icbi` (98), and
`mfspr` (33) and `mtspr` (75) on SPRG0 to SPRG3 (registers 272 to 275) and,
`mfspr` only, PVR (287). They are exactly those cases and no others: the other
special registers that the generator reads or writes (XER, LR, CTR, SRR0, SRR1
and GQR0 to GQR7) are compared. In all six runs the verifier ended with
`reports=0`, `skipped_zero=0`, `cap_hits=0`, `undercharges=0` and
`late_matches=0`.

**On other processors and systems.** Seeds 1 and 2 were repeated on GitHub's
runners with the workflow `.github/workflows/isa-conformance.yml` (the same code,
with ModernGekko built there). They gave exactly the figures of the local run for
those two seeds (13,687 cases, 13,526 equal, **0** different and 161 not compared)
in four combinations:

| System and processor | Compiler of the recompiled module |
|---|---|
| Linux, x86-64 | GCC |
| Linux, ARM64 (aarch64) | GCC |
| Windows, ARM64 | MSVC (Visual Studio, `cl.exe` 19.51) |
| Windows, ARM64 | Clang from llvm-mingw (the portable compiler that [the Windows package](../packaging/windows/README.md) carries) |

That ARM64 agrees matters because ARM and x86 processors differ exactly in
floating-point corner cases (NaN, overflowing conversions, denormals), and this
test exercises edge values on every instruction. That Windows with MSVC agrees
matters because it is the system and the compiler of the laptop, and because MSVC
has no `-ffp-contract=off` but `/fp:strict` (see "What it does NOT prove"). The
portable compiler matters because it is the one the player will use: the package
does not carry Visual Studio, and the game's module is built on their computer,
from their own disc, with llvm-mingw. The two builds on Windows take different
times (about three minutes per seed with MSVC and one with llvm-mingw) but the
result is the same.

**What turned up on the way.** The test did not come out clean the first time, and
each stumble taught something:

1. **A program that hung (a generator error, not a recompiler one).** The first
   full run did not finish. After the last case that was compared came `lbzux r0,
   0, r0`, an update form whose base register is `r0`: the manual calls it
   invalid, and the address the processor computes is `r0 + rB`, which can be
   anything (here it was unmapped). The load raises an exception, the processor
   jumps to the exception vector, where the test program has no code, and stays
   there. The generator no longer gives those forms, and the runner detects the
   hang: `main` writes to memory the number of the group of cases it is about to
   run, and if the program spends `--stall-timeout` seconds (300 by default) in one
   group, the run stops and lists the cases of that group.

2. **A bug in Dolphin's interpreter: the carry of `addme` and `subfme`.** "Add to
   minus one" computes `rA + CA - 1`, that is, `rA + 0xFFFFFFFF + CA`. With `CA =
   1` the sum is `rA + 2^32` and it always carries out. The interpreter
   (`Interpreter_Integer.cpp`, `addmex` and `subfmex`) works the carry out as
   `Helper_Carry(a, carry - 1)`, and `Helper_Carry(a, b)` is `b > ~a`: with `CA =
   0` that is `0xFFFFFFFF > ~a`, true exactly when `a > 0` (right), but with `CA =
   1` it is `0 > ~a`, which never is. The recompiled C does `(u64)rA + 0xFFFFFFFF +
   carry` and takes the carry from bit 32, which is right in both cases:

   | `rA` | `CA` before | Result | `CA` after, per the manual | Dolphin's interpreter |
   |---|---|---|---|---|
   | `0` | 0 | `0xFFFFFFFF` | 0 | 0 |
   | `5` | 0 | `4` | 1 | 1 |
   | `5` | 1 | `5` | 1 | **0** |
   | `0` | 1 | `0` | 1 | **0** |

   The same lines are in Dolphin's `master` branch today. The game uses neither
   instruction, so it is not affected. The generator runs them with the carry
   clear (an `addic` of zero to `r0` clears it and changes nothing else), which is
   where the two agree.

3. **Misaligned accesses (49 of the 53 differences of the second run).** The
   interpreter (`Interpreter_LoadStore.cpp`) raises an alignment exception when
   the address of `lfs`, `lfsu`, `lfd`, `lfdu`, `stfs`, `stfsu`, `stfd`, `stfdu`,
   `lmw` or `stmw` is not a multiple of 4 (`lmw` and `stmw` also with the
   processor in little-endian mode). The recompiled code makes the access. No
   program that works on the console does that, so the game is not affected, but
   the two cannot be compared; the generator now lowers the base register by the
   remainder of the displacement so that the address is a multiple of 4. Integer
   accesses are unchanged: the processor handles the misaligned ones itself.

4. **A gap in the recompiler: `mtfsb1` and the FX bit (the other 4).** In the
   interpreter, `mtfsb1` on an exception bit of the FPSCR (OX, UX, ZX, XX and the
   invalid-operation ones: bits 3 to 12 and 21 to 23) calls `SetFPException`,
   which also sets FX (if the bit was not already set) and recomputes VX and FEX;
   the source warns about it with "This instruction can affect FX". The C that
   DolRecomp generates only sets the bit:

   ```c
   u32 mask = 0x80000000u >> 18;
   if (18 != 1 && 18 != 2) ctx->fpscr |= mask;
   ```

   It is a gap of the recompiler, not undefined behavior. The game only runs
   `mtfsb1 29` (the NI bit), which is not one of those, so it is not affected, and
   the generator does not give `mtfsb1` on those 13 bits until DolRecomp is
   fixed (its authors have not been told yet). `mtfsb1` on any other bit and
   `mtfsb0` on all of them agree.

5. **A memory-layout error in the generator (the large campaign).** The first
   campaign of six seeds with 24 words per form gave about 370 differences per
   seed, all in the forms that sort last in the program (in seed 2, from `lfdu` to
   `stfsu`) and all in XER and FPSCR: the recompiled code kept bits that the
   interpreter clears or recomputes (the reserved ones of XER, bit 20 of FPSCR and
   an odd derived bit such as FEX or VX), like the `mtxer` and `mtfsf` of the
   previous section. But this time the cases had not received the state the
   generator wrote. The state table, with a row of 656 bytes per group of four
   cases, came to 1.1 MB, and there was only 1 MB between it and the scratch memory
   that loads and stores work on; the loader copied that memory, random bytes, over
   about a hundred rows at the end of the table, and the cases that used them
   started from an arbitrary state. It was not a recompiler error but my own, and
   the short run (463 rows, 0.3 MB) could not see it. The scratch memory now sits
   3 MB further on (room for about 25,000 cases) and `build_program` checks that
   none of the sections of the program (the code, the table, the scratch memory and
   the control words) runs into the next one; if they do not fit, it stops with a
   message that says what to change (`--words-per-form` or `--group-size`). The
   figures above are from after the fix.

6. **A judge that let through words the Gekko does not have.** With the layout
   fixed, DolRecomp's decoder did not know about four words per seed: they have a
   reserved bit that Capstone, which follows newer architectures, reads as another
   instruction (`mfocrf`, `mtocrf`, `mffscdrn`, ...). DolRecomp leaves them as a
   `.long` comment with no translation, so those cases compared nothing. To find
   them all, the decoder was given the words of 60 seeds (over 400,000 cases) and
   2.1 million more with random operands, and the ones that Capstone accepted and
   DolRecomp did not were looked at: all of them were `mfcr`, `mtcrf`, `mffs` and
   `mtfsfi`. They are now in `RESERVED_BITS_OF_FORMS`, and the same search finds no
   more. So the figures above are from after this fix too.

**Controls.** The injected-fault self-check works here too:
`run_isa_conformance.py --synthetic --seeds 1 --words-per-form 6 --inject-fault lwz`
made the verifier flip a bit of `r3` at the end of the first `lwz` case
(`lwz r8, 0x2d23(r5)`), and the run reported that case and no other:

```
seed 1: entry=0x80015E74  lwz: lwz r8, 0x2d23(r5)  [r3:N=0x80,I=0x90]
self-check passed: the verifier reported the injected case and nothing else
```

**What this variant does not cover, besides what the previous test does not.**

- The judge of "is this an instruction?" is imperfect. If it lets through a word
  that the Gekko does not have, the recompiler leaves it to the interpreter and
  the case comes out "not compared" (which is why that number matters); if it
  rejects a valid one, that word is not tested, and nothing says so.
- The exclusions above (the carry of `addme` and `subfme`, misaligned accesses,
  `mtfsb1` and the invalid forms) are known differences, not a guarantee that
  there are no others that the generator avoids by accident.
- Each case is a single instruction with a random state: it does not test
  sequences of instructions or how blocks are chained.
- It ran on x86-64 (locally and on GitHub), on ARM64 with Linux and GCC and on
  Windows ARM64 with MSVC and with llvm-mingw (the last three on GitHub), always
  with the same figures. It did not run on the player's laptop: GitHub's runners are
  ARM64 virtual machines on a server, and although both are ARM64, the exact
  processor may differ. Nor was it tried with several versions of each compiler: on
  Windows they were MSVC 19.51 and Clang 23.1, and another version might optimize
  differently.

### How to repeat it

Once, apply the patch and rebuild the runner
([patches/README.md](../patches/README.md)). Then, each time:

```bash
STATICRECOMP_LOCKSTEP=1 STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1 \
STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1 \
STATICRECOMP_LOCKSTEP_COVERAGE_FILE="$PWD/coverage.txt" \
    scripts/build_and_run_linux.sh 2>&1 | tee lockstep.log
python3 tools/lockstep_report.py lockstep.log        # divergences by cause
python3 tools/lockstep_coverage.py coverage.txt      # how much was compared
```

**Close the game normally** (the window): the summary and the coverage file
are written on exit. It is slow, because each new block runs twice; once the
game is "warm" the speed goes back to normal. More verifier variables (all
optional):

| Variable | What it does |
|---|---|
| `STATICRECOMP_LOCKSTEP_START`, `_LIMIT` | Verify only between those native-dispatch numbers (`0` = no limit). |
| `STATICRECOMP_LOCKSTEP_FILTER=text` | Print only the divergences whose line contains the text; the rest are counted in `filtered`. |
| `STATICRECOMP_LOCKSTEP_NODEDUP=1` | Verify **every** visit to a block, not only the first (much slower). |
| `STATICRECOMP_LOCKSTEP_REPEAT=0xADDRESS` | Verify every visit, but only of the block that starts there. |
| `STATICRECOMP_LOCKSTEP_MAXREPORT=n` | Print at most `n` divergences (`0` = all). |
| `STATICRECOMP_LOCKSTEP_STEPCAP=n` | Most interpreter steps per block (20,000 by default). |
| `STATICRECOMP_LOCKSTEP_TRACE=0xADDRESS` | Print step by step what the interpreter does in the block that starts there. |
| `STATICRECOMP_LOCKSTEP_WHITELIST=0xA,0xB` | Never report the blocks that start at those addresses. |

If a new divergence appears, `lockstep_report.py` files it under
`control_flow` or `unexplained` and prints it in full: those are the ones to
read. The way to go is to look at `entry` and `end` with `tools/disasm.py`,
repeat only that block with `_TRACE`, and compare, instruction by instruction,
what the interpreter does with what the generated C code says.

**The instruction-level test** is repeated with a single command. It needs the
disc extracted in `orig/G4AEE9`, ModernGekko built with the patch and, if there
is no screen, `xvfb` (`sudo apt install xvfb`):

```bash
python3 tools/run_isa_conformance.py --template ~/ModernGekko-Template --seeds 1 2 3
python3 tools/run_isa_conformance.py --template ~/ModernGekko-Template --seeds 7 --inject-fault lwz  # self-check
```

Each seed takes two to four minutes on two cores. It leaves the test program, the
list of cases (`cases.tsv`), the report (`report.txt`) and the verifier's log in
`build/isa_conformance/seed_N/`. The exit code is 0 if no case differed, 1 if
any did and 2 if the set-up failed (a tool, the compiler or the screen is
missing) or if the verifier compared no case at all: the verifier switches
itself off, without failing the run, when the runner was not built with the
patch or when the module does not export `ppc_set_mem_write_journal`, and such a
run proves nothing even if it ends "with no differences". The options are in
`--help`.

With `--headless` the runner opens no window, so no screen is needed, real or
virtual (nor `xvfb`); the Windows runs on GitHub use it. On Windows and macOS,
which have a window system of their own, no virtual screen is started either.

Three more options test what the Windows package carries instead of a build of
ModernGekko of our own:

- `--tools-directory FOLDER` instead of `--template`: a folder that has
  `moderngekko-port` and `moderngekko-run` side by side, like the package's `bin`.
  `moderngekko-port` looks for the compiler, CMake, Ninja and Python in the `PATH`,
  so to test the package as it is, the test gets a `PATH` with only the package's
  `toolchain` folders and Windows's own (the workflow `package-windows-arm64.yml`
  does it).
- `--toolchain clang` (or `gcc`, or `msvc`) chooses the compiler of the recompiled
  module. `auto` takes the first one it finds.
- `--synthetic-game-id ID`, with `--synthetic`: the ID of the made-up disc. The
  package is built for one game and refuses any other, so to test it the made-up
  disc has to say it is `G4AEE9`.

An example, from PowerShell, with the package unpacked in `C:\hm`:

```powershell
$python = (Get-Command python).Source   # the Python that has Capstone (requirements.txt), not the package's
$package = 'C:\hm\HarvestMoonMM-windows-arm64'
$toolchain = "$package\toolchain"
$env:PATH = "$toolchain\llvm-mingw\bin;$toolchain\cmake\bin;$toolchain\ninja;$toolchain\python;$env:SystemRoot\System32;$env:SystemRoot"
$env:Python3_ROOT_DIR = "$toolchain\python"
& $python tools\run_isa_conformance.py --synthetic --headless --synthetic-game-id G4AEE9 `
    --tools-directory "$package\bin" --toolchain clang --seeds 1
```

**The variant without the game** asks for the same minus the disc and the symbols:

```bash
python3 tools/run_isa_conformance.py --synthetic --template ~/ModernGekko-Template --seeds 1 2 3
python3 tools/run_isa_conformance.py --synthetic --template ~/ModernGekko-Template --seeds 7 --inject-fault lwz  # self-check
```

With the usual 24 words per form each seed takes about 5 minutes on two cores; with
`--words-per-form 6`, about two. If the program hangs, the runner notices when it
spends `--stall-timeout` seconds (300 by default) in one group of cases: it stops
and lists the cases of that group. And if the words do not fit in the program's
memory, it refuses to build it and says what to change (`--words-per-form` or
`--group-size`).

It can also be started on GitHub, with nothing to install: the **Actions** tab →
**Instruction conformance** → **Run workflow**. The workflow
(`.github/workflows/isa-conformance.yml`) builds ModernGekko (20 to 40
minutes) and runs the test on Linux x86-64, on Linux ARM64 and on Windows ARM64
(on the last one twice: with MSVC and with the portable compiler llvm-mingw); the
reports stay as attachments of the run.

The Windows ARM64 package is tested on GitHub by another workflow,
`.github/workflows/package-windows-arm64.yml` (**Actions** → **Windows ARM64
package** → **Run workflow**), always with a made-up disc and no game data. In
this order, it checks that the package's programs are ARM64 and start with only
the folders of Windows on the `PATH`; this same instruction-level test, with the
package's compiler; `play.ps1` twice, with Windows PowerShell 5.1 and with long
paths turned off, as on most computers (the first run reads the disc, extracts
it, recompiles it and installs the module; the second must find everything done;
both build and install the mods of `game.psd1`; the longest path it made had 253
characters, of the 259 that Windows allows that way); and that the installed game
starts the way the launcher starts it, drawing nothing and with nothing of the
compiler on the `PATH`, loads those mods, keeps running for a few seconds and
closes with exit code 0. It does **not** check the real game, the window or
the GPU (DirectX 12 or Vulkan on the Snapdragon driver), the controller or the
keyboard, nor a real computer. What was tried by hand on a laptop with a
Snapdragon (the real game, with DirectX 12) is in [PERFORMANCE.md](PERFORMANCE.md).

### What it does NOT prove

- **It only checks the code the run reached.** "Zero divergences" is worth
  what it covers; the numbers are under "Coverage".
- **Each block is checked once**, the first time it runs, with the registers
  and memory of that moment, and the coverage only counts the path the
  interpreter took then. A block that is only wrong with other values, or on a
  branch that visit did not take, would not show (`_NODEDUP` covers the first,
  at a huge cost; the instruction-level test covers the instructions on those
  branches).
- **The reference is Dolphin's interpreter.** If the interpreter were wrong in
  the same way, the two would agree. It is a very well tested reference, but
  it is a reference, not a specification, and it is not perfect: the test
  without the game found a carry bug in it (`addme` and `subfme`, above).
- **Accepting "any arrival" cannot tell how many iterations** a loop that
  leaves on the cycle budget makes. That is decided by a timing convention,
  not by the game's logic, and what is demanded is that the state is
  identical at the iteration where it leaves, with every memory write, but an
  error that only changed the number of iterations would not show.
- **Timing is approximate.** The recompiled code charges fewer cycles than the
  interpreter in some loops (in the one at `0x80185568`, about 65% per
  iteration). The values it computes are the same, but the exact split of
  cycles between instructions and hardware events is not.
- **It does not cover the hardware.** Video (GX), audio (DSP), disc, memory
  card and controllers are Dolphin's with and without the recompiled code;
  the verifier compares what the CPU writes to the hardware, not how the
  hardware answers.
- **The host processor's rounding mode.** The verifier runs the recompiled
  code and the interpreter on the same processor with the same floating-point
  configuration, so it cannot see whether the recompiled code stops changing
  it. The C that DolRecomp generates does not change it when the game writes
  the FPSCR with `mtfsf`, `mtfsfi`, `mtfsb0` or `mtfsb1` (only the runtime's
  helper functions and the LLVM generator do), so the host's rounding stays at
  "to nearest". The game is not affected as long as it does not ask for
  another: the executable writes the FPSCR in five instructions (four `mtfsf`
  and one `mtfsb1`) that initialise the floating-point registers at start-up
  (`__OSFPRInit`), restore a context, change exception bits starting from a
  value read with `mffs`, or set the NI bit (`mtfsb1 29`, in `OSInit`; the
  runtime emulates that bit in software), and as far as I saw none picks a
  rounding of its own. I have not checked it with the game running.
- **Whole areas of the game are unvisited:** shops, the village, minigames and
  festivals, and also the start of a new day and the end of an event, where the
  field check runs (`fn_80166888`, the biggest function no run executed).
- **The runs that use the game were all on x86-64.** The instruction-level
  test without the game was repeated on ARM64 (Linux with GCC, and Windows with
  MSVC and with llvm-mingw) with the same figures. The generated C is the same on
  ARM64, but other compilers build it, and ARM processors differ from x86 ones
  exactly in floating-point corner cases (NaN, overflowing conversions,
  denormals): the instruction-level test does cover that, because it exercises
  those values on every instruction. What it does not cover is a compiler fusing a
  multiplication and an addition into one operation, which rounds differently from
  PowerPC's: that needs two instructions in a row, and each case of the test is a
  single one. The module is built with `-ffp-contract=off` on GCC and Clang and
  with `/fp:strict` on MSVC, which [generates no
  contractions](https://devblogs.microsoft.com/cppblog/the-fpcontract-flag-and-changes-to-fp-modes-in-vs2022/)
  since Visual Studio 2022 17.0 (that is what `moderngekko_port.cpp` sets), but no
  run with the game on Windows ARM64 has checked it. Running the verifier with the
  game on that laptop is still the most useful check left.

