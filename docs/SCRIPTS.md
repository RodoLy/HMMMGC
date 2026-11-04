# Event scripts / Scripts de eventos

Target: `Harvest Moon: Magical Melody (USA)` · Game ID `G4AEE9` · intérprete `lsrun.cpp`

[Español](#español) · [English](#english)

Reproduce / Reproducir:

```sh
python3 tools/lsb.py Eve_rm01        # disassemble one script, by name or by path
python3 tools/lsb.py --check         # validate the 291 scripts of the disc
python3 tools/lsb.py --builtins      # native function behind each built-in index
HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest tests/test_lsb.py
```

Everything here comes from static analysis of `main.dol` and the disc files;
nothing was run on the recompiled game. / Todo lo de aquí sale de análisis
estático de `main.dol` y de los archivos del disco; nada se ejecutó en el
juego recompilado.

---

## Español

### Resumen

- Un `.lsb` es el bytecode de una máquina de pila pequeña, el intérprete
  `lsrun.cpp` (`fn_800522EC`). Tiene 52 opcodes (0x00-0x33); 29 aparecen en el
  disco. Los 291 scripts se leen completos y todo salto, llamada y punto de
  entrada cae al inicio de una instrucción (`tools/lsb.py --check`).
- Las funciones nativas ("built-ins") se llaman por número: `CALL_BUILTIN n`
  toma el puntero `n` de una tabla de 230 entradas en `0x80219EAC` (188
  llenas, 160 distintas usadas por los scripts). Ocho están identificadas con
  evidencia; las demás conservan su número.
- `Eve_rm01` / `Eve_rm02` (la boda con Jamie, una por sexo del jugador)
  terminan con la tarjeta "Several Seasons Later...", un fundido a negro y
  `END_EVENT`. **Ningún script de la boda ni del epílogo termina el juego.**
- **Lo que termina el juego es código nativo.** Con el flag principal 230
  (`0xE6`) puesto, la revisión del campo (`fn_80166888`, área 26) encadena los
  eventos `Eve_rm00` → `Eve_rm01`/`Eve_rm02` → `Eve_EP00` … `Eve_EP08` (los
  primeros `EP` según contadores; `EP06`, `EP07` y `EP08` siempre). Cuando
  termina `Eve_EP08` (id `0x117`), el cierre de eventos (`fn_800FC528`) llama
  `fn_8003468C(2)` en `0x800FC738`: pide el estado 2, que construye
  `TitleTask` con el parámetro 1 (el estado 1 la construye con 0). Ese es el
  único lugar del DOL que pide el estado 2. Confianza: alta (análisis
  estático de todos los puntos de entrada), sin prueba en ejecución.
- Mod recomendado: un gancho en la entrada de la revisión del campo
  (`fn_80166888`, `0x80166888`) que apague el flag 230 en cuanto la boda
  (`0x10D` o `0x10E`) esté marcada como consumida. Es una escritura de un
  byte, no depende de cómo termine el evento y también deshace que `talk.lsb`
  vuelva a encender el flag. Variantes (gancho en el cierre del evento,
  amanecer con el estado 9, parche del script) en "Diseño recomendado para el
  mod".

### Cómo corre un script

```
fn_80166888  revisión del campo (estado 0x28), una vez por cuadro según el área
  └─ fn_800FC2B4  StartEvent(gestor, id): valida, marca el evento como consumido,
       │           guarda el nombre del script (tabla 0x8026BBA0) y el archivo
       │           de mensajes (0x8026B740)
       └─ fn_800FCBD0  prepara el gestor: id, permisos, punto de regreso, estado = 1
fn_800FCE04  actualización por cuadro del gestor (`*0x80321D88`)
  ├─ estado 1: crea el contexto (fn_80051904) y arranca el hilo (fn_80051784)
  ├─ cada cuadro: fn_8005162C → fn_800522EC (el intérprete)
  └─ el script terminó: fn_800FC528 (cierre) → máquina de estados fn_80034868
```

| Función | Qué hace | Evidencia |
|---|---|---|
| `fn_80055068` | Caché de scripts: busca por nombre; si no está, arma `"/Script/" + nombre + ".lsb"` y lee el archivo con `fn_80029E74` | Cadenas en `0x801CF990` y `0x80322E38`, usadas en `0x80055138` y `0x8005515C` |
| `fn_800FC2B4` | `StartEvent`: exige el bit 8 de la máscara de permisos (`fn_8003449C`) y que el evento no esté consumido (bit `id` del conjunto `save+0x6C`); lo marca consumido (y deja sin consumir los eventos `0x0F` y `0x10B`), enciende el flag `0x1A7`, llama a `fn_800FCBD0` y guarda el archivo de mensajes (`mgr+0xB0`, tabla `0x8026B740`) y el nombre del script (`mgr+0xA8`, tabla `0x8026BBA0`) | `0x800FC2D4`-`0x800FC3CC`; devuelve 1 solo si arrancó |
| `fn_800FCBD0` | Prepara el gestor para el evento `id`: guarda `mgr+0x94 = id`, limpia las banderas del evento (`+0xB8`, `+0xE0`-`+0xE9`), pone la variable de flujo en 4, apaga los bits 1, 2, 4, 8, 0x10, 0x20 y 0x40 de la máscara de permisos `0x80321DC0` (`fn_800344B4`; `fn_800FC528` los vuelve a encender), llama a `fn_800FC858` y deja el estado `mgr+0xD4 = 1` | `0x800FCBEC`, `0x800FCC38`-`0x800FCC94`, `0x800FCCEC`, `0x800FCCF4` |
| `fn_800FCD48` | Arranque forzado: como `StartEvent`, pero sin revisar permisos ni bits de consumido y sin marcar nada; solo lo llama `TitleTask` (ids `0x10A`, `0x35` y `0x41`-`0x45`) | `0x800FCD64` llama a `fn_800FCBD0`; llamadores `0x800F2788`, `0x800F3260`, `0x800F33A0`-`0x800F33E0` |
| `fn_800FC858` | Punto de regreso del evento (`mgr+0x70..0x7C` posición, `+0x80` área) y siguiente estado `mgr+0x84 = 0x28`; lee la tabla de colocación `0x8026C000` (8 bytes por evento; `-3` = "regresar donde estaba el jugador") | `0x800FC880`-`0x800FC888`, `0x800FC9A8` |
| `fn_800FCE04` | Actualización del gestor; con estado 1 crea el contexto (`fn_80051904`) y arranca el hilo (`fn_80051784`); cada cuadro llama `fn_8005162C`; si el hilo terminó (o hay bandera de salto `+0xE9`) llama `fn_800FC528` | `0x800FCEE0`, `0x800FCF24`, `0x800FCF54`, `0x800FCF64`, `0x800FCF80` |
| `fn_8005162C` | `run(contexto, dt)`: ejecuta `fn_800522EC` sobre el hilo actual y guarda el siguiente; devuelve si sigue vivo | `0x8005164C`-`0x80051660` |
| `fn_800522EC` | El intérprete: si el temporizador de espera (`hilo+0x54`) supera `dt` lo reduce y vuelve; si no, ejecuta instrucciones hasta que una función nativa enciende el bit 2 de `hilo+0x48` (ceder) | `0x80052304`-`0x80052318`, tabla de saltos `0x80219D0C` |
| `fn_800FC528` | Cierre del evento (único): libera actores, pone `mgr+0x94 = 0x119` (ninguno) y `mgr+0xD4 = 4`; si el evento es `0x117` pide el estado 2 | `0x800FC734`-`0x800FC738`, `0x800FC754`, `0x800FC760` |
| `fn_80034868` | Máquina de estados del juego (tabla `0x80215D28`, 43 estados, cola en `*0x80321D1C`); el estado 4 espera `mgr+0xD4 == 4` y encola `mgr+0x84` | `0x80034EE8`-`0x80034F70` |

Objetos: el **contexto** (0x18 bytes, `fn_80051904`) guarda el hilo en curso
(`+0`), el hilo principal (`+4`) y la pila de 128 casillas de 8 bytes (`+8`
cima, `+0xC` base, `+0x10` límite). El **hilo** (0x5C bytes) guarda el
contador de programa (`+4`), la dirección de la instrucción en curso (`+8`,
sirve para reintentar), el `dt` (`+0x24`), el archivo de mensajes (`+0x40`),
las banderas (`+0x48`, bit 2 = ceder), el contexto (`+0x4C`) y el temporizador
de espera en segundos (`+0x54`).

Un script cede el control cuando una función nativa enciende el bit de ceder;
el intérprete devuelve entonces el control al juego hasta el cuadro
siguiente. `WAIT` además arma el temporizador; `WAIT_MESSAGE` retrocede el
contador de programa a su propia instrucción (`fn_80052D48`) mientras la
ventana de mensajes esté ocupada, de modo que se reintenta cada cuadro.

### Formato del archivo `.lsb`

Todo es big-endian. Ejemplo real (`Eve_rm01`): `FFFFFFFF 00000C9F FFFFFFFF
00001C8B 00000000` = sin entrada init, `main` en `0xC9F`, sin entrada con
argumento, tabla de cadenas en `0x1C8B` (el tamaño del archivo), 0 globales.

| Offset | Tamaño | Campo | Evidencia |
|---|---|---|---|
| `0x00` | u32 | Entrada **init** (offset de código, `0xFFFFFFFF` = ninguna); se ejecuta hasta el final antes de `main` | `fn_80052E90`; siempre ausente en el disco |
| `0x04` | u32 | Entrada **main**: donde arranca el evento | `fn_80052E5C` (la llama `fn_80051784`) |
| `0x08` | u32 | Entrada con **un argumento entero** (la usa `fn_80052DDC` para lanzar hilos); presente en 67 scripts | `0x80052E28` |
| `0x0C` | u32 | Offset de archivo de la **tabla de cadenas**; igual al tamaño del archivo (tabla vacía) en los 291 scripts | `fn_80052EEC`, hilo `+0x1C`; verificado con `--check` |
| `0x10` | u32 | **Cantidad de globales** (0, 1 o 4 en el disco) | `fn_80052EEC` |
| `0x14` | 8 × N | Globales: casillas iniciales (todas `int 0`) | `fn_80052EEC` las copia a un arreglo propio de cada hilo |
| `0x14 + 8N` | … | **Código**, hasta la tabla de cadenas | `hilo+0` = `imagen + 0x14 + 8N` |

Las entradas se miden desde el inicio del código; los operandos de `JUMP`,
`JUMP_IF_ZERO` y `CALL` se miden desde el propio opcode (los handlers parten de
`pc - 1`), así que el código se puede mover sin recalcular nada.

**Enteros de operando.** Grupos de 7 bits en big-endian; el bit alto de cada
byte menos el último vale 1; el resultado se extiende en signo desde `7 × n`
bits. `80 5B` = 91, `81 66` = 230, `FF 01` = -127, `97 38` = 3000, `7E` = -2.
`PUSH_FLOAT` es la excepción: lleva 4 bytes IEEE 754 sin codificar.

**Pila.** Casillas de 8 bytes `{s16 tipo, s16 banderas, u32 valor}`. Tipos
vistos en los handlers: 1 entero, 2 flotante (las funciones nativas convierten
con `fctiwz` al leer argumentos), 3 cadena, 4 arreglo, 5 referencia, 6
registro de llamada, 7 marca, 8 referencia a elemento. Los operadores binarios
calculan **TOPE op SEGUNDO** (el operando derecho se empuja primero): `a - b`
se escribe `PUSH b`, `PUSH a`, `SUBTRACT`. `CALL` empuja un registro de
llamada (tipo 6); `ENTER n` reserva `n` casillas de locales; los locales se
numeran `0..n-1`, el registro es `-1` y los argumentos `-2, -3, …`. `RETURN n`
libera `n + 1` casillas (locales, registro y argumentos).

**Instrucciones** (tabla de saltos `0x80219D0C`, 0x34 entradas):

| Código | Nombre | Operando | Efecto | Handler |
|---|---|---|---|---|
| `00`, `32` | `NOP`, `NOP_32` | – | nada | `0x80052C90` |
| `01` | `ENTER` | n | reserva n locales vacíos | `0x80052368` |
| `02` | `PUSH_INT` | n | empuja un entero | `0x80052554` |
| `03` | `PUSH_FLOAT` | 4 bytes | empuja un flotante | `0x80052560` |
| `04` | `PUSH_STRING` | n | puntero a la tabla de cadenas + n (no se usa) | `0x8005256C` |
| `05`-`07` | `LOCAL_REF/GET/SET` | n | referencia / valor / asigna el local n | `0x80052578` / `84` / `90` |
| `08`-`0A` | `GLOBAL_REF/GET/SET` | n | ídem con globales | `0x8005259C` / `A8` / `B4` |
| `0B` | `CALL_BUILTIN_VALUE` | n | llama la función nativa n y empuja su resultado | `0x800525C0` |
| `0C` | `CALL_BUILTIN` | n | llama la función nativa n, descarta el resultado | `0x800526D8` |
| `0D`-`10` | `UNKNOWN_0D`…`10` | n | no aparecen en el disco; sin analizar | `0x800527E4`-`0x80052810` |
| `11` | `JUMP` | n | salta a `dirección + n` | `0x8005281C` |
| `12` | `JUMP_IF_ZERO` | n | saca un entero; salta si es 0 | `0x800528DC` |
| `13`, `14` | `CALL_VALUE`, `CALL` | n | llama al código en `dirección + n` | `0x800529F0`, `0x80052A00` |
| `15` | `RETURN` | n | vuelve; libera n + 1 casillas | `0x80052A10` |
| `16` | `RETURN_VOID` | – | igual que `RETURN 0` | `0x80052A6C` |
| `17`-`19` | `NEW_ARRAY`, `ARRAY_GET`, `ARRAY_SET` | – | arreglos; no se usan | `0x80052A78`-`0x80052A90` |
| `1A` | `PUSH_RESULT` | – | empuja el último resultado; no se usa | `0x80052A9C` |
| `1B`-`2C` | `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `MODULO`, `SHIFT_LEFT`, `SHIFT_RIGHT`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_OR_EQUAL`, `GREATER`, `GREATER_OR_EQUAL`, `LOGICAL_AND`, `LOGICAL_OR` | – | sacan dos casillas, empujan una | `0x80052AAC` (compartido) |
| `2D`-`2F` | `NEGATE`, `BIT_NOT`, `LOGICAL_NOT` | – | sacan una, empujan una | `0x80052AB8` |
| `30`, `31` | `PRINT`, `PRINT_LINE` | – | imprime por `OSReport` y cede; no se usan | `0x80052AC4`, `0x80052B4C` |
| `33` | `LINE` | n | guarda n en `hilo+0x28` (¿línea de origen?); no se usa | `0x80052BE0` |

### Funciones nativas (built-ins)

`CALL_BUILTIN n` y `CALL_BUILTIN_VALUE n` comparan `n` con el contador de
`0x80322E54` (vale 230) y, si hay un puntero en `0x80219EAC + 4n`, lo llaman
con el hilo en `r3`; una entrada vacía no hace nada. Hay 188 llenas y los
scripts usan 160 distintas (la mayor es la 221). `python3 tools/lsb.py
--builtins` las lista con su dirección y cuántas veces las llaman los
scripts. Ojo: que el contador valga `0xE6` (230) y que el flag de la boda sea
el número 230 (`0xE6`) es una coincidencia sin relación.

Identificadas (la herramienta las nombra; cada una se leyó en el DOL):

| n | Nombre | Función | Qué hace |
|---|---|---|---|
| 6 | `WAIT` | `fn_800631A0` | Saca un número (si es entero lo convierte a flotante), lo guarda en `hilo+0x54` y cede |
| 48 | `YIELD` | `fn_80062EC8` | Solo enciende el bit de ceder |
| 51 | `SHOW_MESSAGE` | `fn_800629D0` | Saca un entero y llama a la ventana de mensajes (`fn_80084FCC` sobre `0x802DBA28`) con el archivo del hilo (`+0x40`) |
| 62 | `WAIT_MESSAGE` | `fn_800627C0` | Si el byte `0x802DBA28 + 0x340` es distinto de 0, reintenta la instrucción y cede |
| 55 | `GET_GAME_VALUE` | `fn_80060D94` | Saca un selector (< 0xC0) y lo despacha por una tabla de 192 entradas (`0x8021A58C`) que lee contadores de la partida; el selector 1 lee `save+0x227C` |
| 56 | `SET_FLAG` | `fn_80060D14` | Saca `n` y enciende el bit `n` del conjunto principal (`save+4`, `fn_800A7824`) |
| 57 | `CLEAR_FLAG` | `fn_80060C94` | Apaga ese bit (`fn_800A7804`) |
| 58 | `TEST_FLAG` | `fn_80060BE8` | Lee ese bit (`fn_800A77DC`) y empuja 0 o 1 |

Los bits se numeran así: el bit `n` es el bit `n & 7` del byte `n >> 3`
(`fn_800A77DC`, `fn_800A7804`, `fn_800A7824`). El flag 230 es el bit 6 del
byte 28 de `save+4`, es decir `save + 0x20`, máscara `0x40`.

Leídas solo en parte (nombres propuestos al final, ninguno aplicado):

| n | Función | Qué hace | Usos |
|---|---|---|---|
| 65 / 66 | `fn_80060BC4` / `fn_80060BA0` | Reanuda / pausa las tareas del campo (`fn_80034820` / `fn_80034844`) y arma un fundido con `*0x802AEDC0` (`fn_8002BCF8` / `fn_8002B920`) | 278 / 284 scripts |
| 67 | `fn_800608E4` | Reintenta mientras `fn_800346CC()` sea 0; si no, arma un temporizador y cede (parece "esperar al fundido") | 287 scripts |
| 68 | `fn_800608B8` | Cede y manda el mensaje 1 a la cola de tareas (`fn_800347F4`) | 414 llamadas en 286 scripts |
| 69 | `fn_80060880` | Cede, destruye los actores del evento (`fn_800FC480`) y manda el mensaje 2 (`fn_800347C8`): es el fin del evento; los scripts de la boda y del epílogo lo llaman al final | 418 llamadas en 287 scripts |
| 70 | `fn_80060804` | Guarda `fn_800A232C(n)` en `0x80321D3C`; `fn_800338A0` (día nuevo) lo copia a `save+0xD454` (¿lugar donde despierta el jugador?) | 286 scripts; la boda llama `70(26)` |
| 91 | `fn_8005B868` | Manda `0x80000002` a un objeto: el mismo mensaje que usa el cierre de evento para destruir cada actor | 118 scripts |
| 105 | `fn_8005B460` | Saca tres enteros y llama `fn_80097FDC` (fundido de pantalla); la boda lo llama con `0, -127, 3000`, los mismos valores que el estado 0x26 de día nuevo (`0x80036280`-`0x80036288`) | 270 scripts |
| 128 / 129 | `fn_8005A594` / `fn_8005A54C` | Piden el estado 26 (`0x1A`) / el estado 42 (`0x2A`, reposo) al juego con `fn_8003468C` | 23 scripts cada una; **ninguno de la boda ni del epílogo** |
| 146 / 147 | `fn_80059380` / `fn_8005929C` | Llegan a la variable de flujo (`fn_80034684`) | `Eve_F02`, `F05`, `F12`, `F13`; ninguno de la boda |

### Herramienta y pruebas

`tools/lsb.py` lee un script (`parse_script`, `load_script`), lo desensambla
(`disassemble`) y ofrece `--check`, `--builtins` y `--file-offsets` (muestra
offsets de archivo, útiles para parches). Las pruebas (`tests/test_lsb.py`)
usan scripts sintéticos armados por un codificador independiente
(`tests/synthetic_script.py`) y, si `HM_MM_DOL` apunta a tu `main.dol`,
comprueban también los datos reales del disco: 291 scripts, cabeceras, el
conjunto de opcodes sin uso, el orden de operandos de la resta, los puntos
donde se toca el flag 230 y los hechos de la tabla nativa.

### La boda con Jamie

| Script | Tamaño | Qué es | Evidencia |
|---|---|---|---|
| `Eve_marry00` | 1 112 B de texto | Consejo del pueblo antes de casarse ("…one trial") | Texto de `eve_marry00_e.bin` |
| `Eve_marry02` | – | Boda normal (ceremonia y banquete); no toca el flag 230, así que no entra en la cadena del final | Texto de `eve_marry02_e.bin`; cierra con `CALL_BUILTIN 91` y `69` |
| `Eve_rm00` (`0x10C`) | 2 639 B | Reacción al compromiso ("Oh, you proposed to Jamie!") | Texto de `eve_rm00_e.bin` |
| `Eve_rm01` (`0x10D`) | 7 307 B | Ceremonia con Jamie, jugador con `save+0x227C == 0` | `0x8016D1B8` |
| `Eve_rm02` (`0x10E`) | 7 271 B | La misma ceremonia con los parámetros de sexo cambiados; comparte con `Eve_rm01` los primeros `0xCCE` bytes de código | `0x8016D1EC` |
| `Eve_EP00`…`EP08` (`0x10F`-`0x117`) | – | Epílogo: ocho escenas con texto y una (`EP08`) sin texto | `eve_EP08_e.bin` no existe en el disco |

- `Eve_rm01.lsb` y `Eve_rm01_e.lsb` son idénticos byte a byte; `Eve_rm01_eu.lsb`
  difiere en un solo `WAIT` (2 en vez de 3, código `0x12B9`). Lo mismo con
  `rm02`. La tabla de nombres del DOL USA (`0x8026BBA0`) apunta a `Eve_rm01`
  y `Eve_rm02` sin sufijo, y el cargador no agrega ninguno
  (`"/Script/" + nombre + ".lsb"`), así que esos son los archivos que se leen.
- Estructura de `Eve_rm01`: tres funciones auxiliares compartidas (`0x0`,
  `0xF9`, `0x11D`; los primeros `0x22D` bytes los comparten 36 scripts, p. ej.
  `Eve_marry02`), una cuarta en `0xBEA` y `main` en `0xC9F`
  (`ENTER 9`, `PUSH_INT 26`, `CALL_BUILTIN 70`, …). La ceremonia son bucles de
  actores y mensajes; **el cierre empieza en `.L1C1E` (código `0x1C1E`)**:

```
  01C40  02 12           PUSH_INT 18
  01C42  0C 33           CALL_BUILTIN 51  ; SHOW_MESSAGE     (tarjeta "Several Seasons Later...")
  01C44  0C 3E           CALL_BUILTIN 62  ; WAIT_MESSAGE
  ...
  01C5D  02 FF 01        PUSH_INT -127
  01C60  02 97 38        PUSH_INT 3000
  01C63  0C 80 69        CALL_BUILTIN 105                    (fundido a negro, 3 s)
  01C66  03 40 00 00 00  PUSH_FLOAT 2
  01C6B  0C 06           CALL_BUILTIN 6  ; WAIT
  01C6D  02 00           PUSH_INT 0
  01C6F  0C 80 67        CALL_BUILTIN 103
  01C72  0C 80 45        CALL_BUILTIN 69                     (fin del evento)
  01C75  15 09           RETURN 9
```

  El mensaje 18 es el último texto de `eve_rm01_e.bin` y es esa tarjeta. En
  `Eve_rm02` la misma secuencia está en `0x1C1C`.
- Ni `Eve_rm00`, `Eve_rm01`, `Eve_rm02` ni los `Eve_EP*` ponen o apagan flags
  con constantes, y ninguno llama a las funciones que piden estados
  (128, 129, 146, 147). El fin de la partida no se decide en los scripts.
- El flag 230 solo lo tocan scripts en `talk` (`PUSH_INT 230` en `0xC0B`
  lo enciende; en `0xCA6` y `0xCDE` lo apaga). La rama de `0xC0B` se alcanza
  cuando una variable que llena `CALL_BUILTIN 117` vale 1119, `CALL_BUILTIN_VALUE
  113` devuelve 2 y `CALL_BUILTIN_VALUE 220` devuelve 1; responde con un
  mensaje según `GET_GAME_VALUE(1)`. Nadie lo prueba desde un script
  (`TEST_FLAG` con 230 no aparece); lo prueba el código nativo.

### Qué termina el juego

Cadena completa, con direcciones (todas de `main.dol`):

| Paso | Dónde | Qué ocurre |
|---|---|---|
| 1 | `talk.lsb` `0xC0B` | Enciende el flag 230 (bit 6 de `save + 0x20`) |
| 2 | `fn_80166888` `0x8016D07C` | Con el jugador en el área 26 (índice 26 de la tabla `0x802A341C`, destino `0x8016D070`) y el flag 230 puesto salta a `0x8016D188`. Es el único sitio nativo que prueba ese flag |
| 3 | `0x8016D190` | `StartEvent(0x10C)` (`Eve_rm00`); cada llamada devuelve 1 solo si arrancó el evento y esa pasada del campo termina ahí |
| 4 | `0x8016D1B8`-`0x8016D204` | Si `save+0x227C == 0` → `StartEvent(0x10D)`; si no → `StartEvent(0x10E)`. Antes pone `save+0x1BC8 = 0` |
| 5 | `0x8016D24C`-`0x8016D544` | `StartEvent(0x10F)` … `StartEvent(0x117)`; lo que falla pasa al siguiente. `EP00`-`EP05` tienen condiciones de contadores y flags; `EP06`, `EP07` y `EP08` no, así que con el flag 230 puesto esos tres acaban jugándose. Justo antes de `EP08` (`0x8016D50C`-`0x8016D538`) escribe `save+0x2234 = 0`, `save+0x2258 = 12` y `save+0x1BC8 = 0`, cada vez que la pasada llega ahí. Cada id se pide desde un único punto del DOL, todos en `fn_80166888` |
| 6 | `fn_800FC528` `0x800FC734`-`0x800FC738` | Si el evento que termina es `0x117` (`Eve_EP08`): `fn_8003468C(2)` |
| 7 | `fn_80034868` estado 2 (`0x80034E4C`) | Crea `TitleTask` (`fn_800F350C`, cadena `0x801E67F8`) con el parámetro 1, que la deja en el subestado 1; los subestados 1 y 2 (`0x800F2850`, `0x800F2994`) esperan 3,0 s y 5,0 s y pasan al subestado 3, el título (con parámetro 0 arranca directo en el 3) |

Estados de `fn_80034868` que intervienen (tabla de saltos `0x80215D28`; el
estado se saca de la cola en `0x800348B0` y se guarda en `0x80321D18`):

| Estado | Qué hace | Dirección |
|---|---|---|
| 1 / 2 | Crea `TitleTask` con parámetro 0 / 1 y empuja el estado `0x2A` | `0x80034DFC` / `0x80034E4C` |
| 3 | Crea `EndingTask` (`fn_80163D98`, nombre `"ENDING"`) y empuja el estado `0x2A`; nadie lo pide | `0x80034E9C` |
| 4 | Espera a que el gestor de eventos llegue a estado 4 y empuja `mgr+0x84` | `0x80034EE8`-`0x80034F6C` |
| 9 | Día nuevo: tras una espera llama a `fn_800338A0` y empuja el estado `0x26` | `0x800361B0` |
| `0x26` | Fundido del cambio de día (`fn_80097FDC` con `0, -127, 3000`) y empuja el `0x25` | `0x80036278` |
| `0x28` | Campo: en el camino normal (`0x80321D2C != 5`) llama a `fn_80166888` una vez por cuadro (es su único llamador); si devuelve 1 (arrancó un evento) empuja el estado 4 | `0x800363C4`, llamada en `0x80036498`, empuje en `0x80036628` |
| `0x2A` | Reposo: no hace nada | `0x80036660` |

Por qué es nativo y no un script:

- `StartEvent` exige que el evento no esté consumido y lo consume al arrancar;
  la cadena avanza porque cada evento se marca y la siguiente pasada del campo
  pide el siguiente. Los scripts no intervienen en esa decisión.
- Siguiendo las llamadas directas hasta ocho niveles, las únicas funciones
  nativas que llegan a la cola de estados (`*0x80321D1C`, que solo cargan
  diez funciones: `fn_80034868`, `fn_8003468C` y ocho vecinas entre
  `0x8003405C` y `0x8003705C`) o a la variable de flujo (`fn_80034684`) son
  las números 128, 129, 146 y 147, y ninguno de los 12 scripts de la cadena
  (`Eve_rm00`-`rm02`, `Eve_EP00`-`EP08`) las llama. Otras (68, 69, 71, 79, 90,
  91, 95, 126, 141, 163, 174, 181) llegan a `fn_801B4B7C`, un "empujar a una
  cola" genérico, pero no a una función que cargue la cola de estados; la 68 y
  la 69 usan la del administrador de tareas (`0x80321D38`).
- `fn_8003468C(2)` tiene un solo llamador en todo el DOL (`0x800FC738`) y la
  función solo se llama con `bl`, nunca por puntero. Sus 50 llamadores pasan
  una constante (0, 2, 4, 8, 20-23, 25, 26, 28-33, 36 o 42; el 2 solo en
  `0x800FC738`) y, de los 73 empujes directos a la cola de estados, 71 llevan
  una constante que nunca es 2 ni 3. Los otros dos son el de `fn_8003468C`
  (su argumento) y el de `fn_80034868` en `0x80034F6C`, que empuja `mgr+0x84`;
  ese campo solo lo escriben el constructor del gestor (`0x2A`),
  `fn_800FC858` (`0x28`) y `TitleTask` (`0x2A`, `9`, `0x27`).
- `Eve_EP08` termina como cualquier otro evento (`CALL_BUILTIN 103`,
  `CALL_BUILTIN 69`, `RETURN 4`) y no tiene archivo de mensajes. Sí existe un
  "staff roll" en el DOL: es `EndingTask` (estado 3), cuyos créditos son
  cadenas desde `0x801ECC34` (`Harvest Moon`, `- Magical Melody -`,
  `Character design`, `Director`...) con la textura `/menu/eng/c_mii.tpl`,
  que está en el disco; las carpetas `message_*/staff/` están vacías. Pero
  nadie pide el estado 3, así que no forma parte de esta cadena: tras
  `Eve_EP08` el juego pasa al estado 2, no al 3.
- Los ids `0x10B`-`0x117` solo se piden desde `fn_80166888`; el id variable de
  `fn_80034868` (`0x80034FC4`) viene de `fn_80034674`, y sus llamadores usan
  ids pequeños (`0x00`-`0x36`, `0x56`; uno más, `0x0F`, vía `fn_801643FC`).

Confianza: **alta** para la cadena (cada paso se leyó en el desensamblado y se
cruzó con todos los puntos de llamada). Lo que no se cubre son llamadas por
puntero a función (no aparecen como `bl`) y cualquier comportamiento en
ejecución.

### Diseño recomendado para el mod

Para que no llegue el final basta con que el flag 230 esté apagado cuando
`fn_80166888` llegue a `0x8016D07C`. Hay tres sitios donde lograrlo; el
primero es el recomendado. En todos, `save = *(u32*)0x80321F48`: el flag 230
es el byte `save + 0x20`, máscara `0x40`, y los bits consumidos de la boda
(`0x10D`, `0x10E`) son el byte `save + 0x8D`, máscara `0x60`.

**Opción A (recomendada): guardia en la revisión del campo.**

- `RECOMP_HOOK(0x80166888, ...)`. `fn_80166888(campo)` solo la llama el
  estado `0x28` (`0x80036498`), una vez por cuadro, y es el único código que
  prueba el flag 230. Un gancho corre antes que el original, así que basta con
  apagar el flag antes de que lo pruebe.
- Regla: si el evento `0x10D` o el `0x10E` ya está consumido, apagar el flag.
  `StartEvent` marca el evento al **arrancar**, no al terminar, así que la
  regla se cumple desde la primera pasada del campo después de la boda.
- Por qué es la más robusta: no necesita saber qué evento terminó ni cómo
  (fin normal o salto); no toca la boda en curso (el campo no se revisa
  mientras corre un evento y nada más lee el flag: en el DOL solo lo prueba
  `0x8016D07C` y en los scripts no aparece `TEST_FLAG 230`); y si `talk.lsb`
  lo vuelve a encender después (proponerle otra vez a Jamie) se apaga en la
  pasada siguiente, de modo que tampoco se repiten las escrituras de
  contadores que hace la cadena antes de `Eve_EP08`. Es idempotente y cuesta
  dos lecturas de un byte por cuadro.

Pseudocódigo (adaptar los accesos a memoria al API real de mods):

```
hook(0x80166888):                          # fn_80166888(field), once per frame in state 0x28
    save = read_u32(0x80321F48)
    if read_u8(save + 0x8D) & 0x60:        # event 0x10D or 0x10E already consumed
        write_u8(save + 0x20, read_u8(save + 0x20) & ~0x40)   # main flag 230 off
```

**Opción A+ (opcional): que la boda termine en una mañana nueva.** Se suma a
la A y no la reemplaza.

- `RECOMP_HOOK(0x800FC528, ...)`: `fn_800FC528(gestor)` es el único cierre de
  eventos y se llama tanto cuando el hilo termina como por la ruta de salto
  (`0x800FCF64`, `0x800FCF80`). `r3` es el gestor (`*0x80321D88`) y
  `[r3 + 0x94]` el id del evento; la función lo restablece a `0x119` más
  adelante, por eso hay que leerlo **al entrar**.
- Si el id es `0x10D` o `0x10E`: `*(u32*)(r3 + 0x84) = 9`. El estado 4 empuja
  `mgr+0x84` cuando el evento termina (`0x80034F68`) y `fn_800FC858` lo
  restablece a `0x28` al preparar cada evento, así que el cambio vale una sola
  vez. El estado 9 es el inicio de un día nuevo (espera, llama a
  `fn_800338A0` y pasa al estado `0x26`, un fundido); el propio juego lo usa
  tras su evento inicial (`0x800F3278`).
- Falta verificar en ejecución que el estado 9 avance el calendario y dónde
  despierta el jugador: la boda llama a `70(26)` y `fn_800338A0` copia ese
  valor (`0x80321D3C`) a `save + 0xD454`.

**Opción B: un solo gancho, en el cierre del evento.** El mismo gancho de
`0x800FC528`, sin gancho por cuadro: si el id es `0x10D` o `0x10E`, apaga el
flag 230 y marca consumidos `0x10F`-`0x117` (`*(u8*)(save + 0x8D) |= 0x80` y
`*(u8*)(save + 0x8E) = 0xFF`). Funciona, pero solo se ejecuta una vez: si
`talk.lsb` vuelve a encender el flag, la cadena reaparece; los `EP*`
consumidos no arrancan, pero la pasada vuelve a escribir los contadores de
`0x8016D50C`-`0x8016D538` (entre ellos `save + 0x2258 = 12`).

**Opción C: parchear los bytes del script.** El cierre de `Eve_rm01` cabe en
seis bytes que se pueden cambiar sin mover nada: en el archivo, en `0x1C54`
(`Eve_rm01`) y `0x1C30` (`Eve_rm02`), `02 12 0C 33 0C 3E` (`PUSH_INT 18;
SHOW_MESSAGE; WAIT_MESSAGE`) pasa a `02 81 66 0C 39 00` (`PUSH_INT 230;
CLEAR_FLAG; NOP`). Apaga el flag desde el propio script y quita de paso la
tarjeta.

**Comparación.**

| | A: guardia en el campo | B: gancho al cerrar el evento | C: parche de script |
|---|---|---|---|
| Dónde | `0x80166888`, cada cuadro | `0x800FC528`, cada fin de evento | bytes de `Eve_rm01` y `Eve_rm02` |
| Cubre ambos sexos y el salto del evento | Sí, un solo punto | Sí, un solo punto | Hay que parchear los dos archivos; no corre si el evento se salta |
| Si `talk.lsb` vuelve a encender el flag | Se apaga en la pasada siguiente | No: la cadena reaparece | No: ídem |
| Puede fijar el estado siguiente (9) | Con la A+ | Sí | No |
| Depende de | Que el API de mods permita escribir memoria del invitado; direcciones fijas de `G4AEE9` | Lo mismo; los ganchos no pueden saltar el original, por eso se usan bits en lugar de impedir llamadas | Aplicar el parche al leer el archivo (nunca distribuir el `.lsb` modificado); direcciones fijas de `G4AEE9`; cambia el texto de la historia |

Una red extra (no necesaria): un gancho en la entrada de `fn_8003468C` que
cambie `r3` de 2 a otro estado. Basta porque el estado 2 se pide desde un solo
sitio, pero deja correr todo el epílogo, así que solo sirve como respaldo.

Qué recuerda la partida de "casado con Jamie": nada en los scripts de la boda
lo escribe; lo único que queda son los bits consumidos `0x10C`-`0x10E`
(`save + 0x6C`). Son la marca natural para el resto del mod (por ejemplo,
`read_u8(save + 0x8D) & 0x60`).

**Qué probar en ejecución** (en este orden): (1) casarse y comprobar que no
arranca `Eve_EP00` ni vuelve al título; (2) guardar y cargar después de la
boda y ver que el flag 230 sigue apagado; (3) volver a hablar con Jamie con el
objeto de la propuesta y ver que no reaparece la cadena ni cambia
`GET_GAME_VALUE(126)` (`save + 0x2258`, que la cadena deja en 12); (4) con la
A+, ver fecha, lugar y música al despertar; (5) repetir como jugador de ambos
sexos.

### Qué no está verificado

- Nada se ejecutó: ni la cadena, ni el gancho, ni el parche.
- No se identificó el área 26 ni el lugar donde queda el jugador tras la boda;
  por la tabla de colocación (`-3`) vuelve a donde estaba al empezar el evento.
- No se sabe si el estado 9 avanza un día, ni qué significa exactamente
  `save + 0xD454` (ver la función nativa 70).
- No se auditaron los demás built-ins que escriben estado (109-112, 133, 151,
  153, 156-160) ni qué cambia una boda normal (`Eve_marry*`) en la partida;
  copiarlo para Jamie sería la forma "completa" de estar casado.
- Qué activaría `EndingTask` (el "staff roll", estado 3): ningún código
  estático pide ese estado. Puede ser código sin uso o llegar por un camino
  que el análisis estático no ve; en ejecución hay que comprobar si tras
  `Eve_EP08` salen los créditos o solo vuelve el título.
- Qué significan `save + 0x2258`, `save + 0x2234` y `save + 0x1BC8`, que la
  cadena reescribe antes de `Eve_EP08`. `0x2258` se lee en unos cien sitios
  del DOL (el estado 0 la compara con 5 y con 6) y los selectores 126 y 129 de
  `GET_GAME_VALUE` leen `0x2258` y `0x1BC8`; no se sabe qué representan. La
  opción A impide la escritura, pero no se sabe si una partida que continúa
  necesitaría otro valor.

### Nombres propuestos

Ninguno se aplicó a `config/G4AEE9/symbols.txt`; hacerlo exige registrar la
evidencia en `docs/FUNCTION_MAP.md`.

| Dirección | Propuesta | Evidencia en este documento |
|---|---|---|
| `0x800522EC` | `lsrun_step` | Intérprete |
| `0x8005162C` | `lsrun_run` | Cómo corre un script |
| `0x80051784` | `lsrun_start_main` | Aserción con `"lsrun.cpp"`, entrada main |
| `0x80055068` | `lsrun_load_script` | Arma `/Script/…lsb` |
| `0x800FC2B4` | `EventManager_StartEvent` | Cómo corre un script |
| `0x800FCBD0` | `EventManager_Setup` | Ídem |
| `0x800FC858` | `EventManager_PlaceReturnPoint` | Ídem |
| `0x800FCE04` | `EventManager_Update` | Ídem |
| `0x800FC528` | `EventManager_Finish` | Único cierre |
| `0x80166888` | `Field_CheckEvents` | Cadena de la boda |
| `0x80034868` | `GameState_Update` | Tabla de 43 estados |
| `0x8003468C` | `GameState_Request` | Empuja un estado en la cola |
| `0x800A77DC` / `0x800A7804` / `0x800A7824` | `BitSet_Test` / `_Clear` / `_Set` | Formato de bits |
| `0x800A7940` / `0x800A7950` | `Counter_Get` / `Counter_Set` | Lectura y escritura de contadores |
| `0x800F350C` / `0x800F26F4` | `TitleTask_Construct` / `TitleTask_Update` | Cadena `"TitleTask"` |
| `0x80163D98` | `EndingTask_Construct` | Estado 3, cadena `"EndingTask"` |
| `0x800FCD48` | `EventManager_ForceStart` | Arranque forzado |
| built-in 65 / 66 / 67 / 69 / 70 / 91 / 105 | `RESUME_AND_FADE` / `PAUSE_AND_FADE` / `WAIT_FADE` / `END_EVENT` / `SET_RETURN_LOCATION` / `DESTROY_ACTOR` / `FADE` | Tabla de funciones nativas |

---

## English

### Summary

- A `.lsb` is the bytecode of a small stack machine, the `lsrun.cpp`
  interpreter (`fn_800522EC`). It has 52 opcodes (0x00-0x33); 29 occur on the
  disc. All 291 scripts decode completely and every jump, call and entry
  point lands on the start of an instruction (`tools/lsb.py --check`).
- Native functions ("built-ins") are called by number: `CALL_BUILTIN n` takes
  pointer `n` from a 230-entry table at `0x80219EAC` (188 filled, 160
  distinct ones used by scripts). Eight are identified with evidence; the
  others keep their number.
- `Eve_rm01` / `Eve_rm02` (the Jamie wedding, one per player gender) end with
  the "Several Seasons Later..." card, a fade to black and `END_EVENT`. **No
  wedding or epilogue script ends the game.**
- **What ends the game is native code.** With main flag 230 (`0xE6`) set, the
  field check (`fn_80166888`, area 26) chains the events `Eve_rm00` →
  `Eve_rm01`/`Eve_rm02` → `Eve_EP00` … `Eve_EP08` (the first `EP`s depending
  on counters; `EP06`, `EP07` and `EP08` always). When `Eve_EP08` (id
  `0x117`) finishes, the event teardown (`fn_800FC528`) calls
  `fn_8003468C(2)` at `0x800FC738`: it requests state 2, which builds
  `TitleTask` with parameter 1 (state 1 builds it with 0). That is the only
  place in the DOL that requests state 2. Confidence: high (static analysis of
  every entry point), not tested at run time.
- Recommended mod: a hook on entry to the field check (`fn_80166888`,
  `0x80166888`) that clears flag 230 as soon as the wedding (`0x10D` or
  `0x10E`) is marked consumed. It is a one-byte write, does not depend on how
  the event ends and also undoes `talk.lsb` setting the flag again. Variants
  (hook on event teardown, waking up through state 9, script patch) in
  "Recommended mod design".

### How a script runs

```
fn_80166888  field check (state 0x28), once per frame, depends on the area
  └─ fn_800FC2B4  StartEvent(manager, id): validates, marks the event consumed,
       │           stores the script name (table 0x8026BBA0) and the message
       │           file (0x8026B740)
       └─ fn_800FCBD0  prepares the manager: id, permissions, return point, status = 1
fn_800FCE04  per-frame update of the manager (`*0x80321D88`)
  ├─ status 1: creates the context (fn_80051904), starts the thread (fn_80051784)
  ├─ every frame: fn_8005162C → fn_800522EC (the interpreter)
  └─ script finished: fn_800FC528 (teardown) → state machine fn_80034868
```

| Function | What it does | Evidence |
|---|---|---|
| `fn_80055068` | Script cache: looks up by name; on a miss builds `"/Script/" + name + ".lsb"` and reads the file with `fn_80029E74` | Strings at `0x801CF990` and `0x80322E38`, used at `0x80055138` and `0x8005515C` |
| `fn_800FC2B4` | `StartEvent`: requires bit 8 of the permission mask (`fn_8003449C`) and that the event is not consumed (bit `id` of the `save+0x6C` set); marks it consumed (and leaves events `0x0F` and `0x10B` unconsumed), sets flag `0x1A7`, calls `fn_800FCBD0` and stores the message file (`mgr+0xB0`, table `0x8026B740`) and the script name (`mgr+0xA8`, table `0x8026BBA0`) | `0x800FC2D4`-`0x800FC3CC`; returns 1 only if it started |
| `fn_800FCBD0` | Prepares the manager for event `id`: stores `mgr+0x94 = id`, clears the event flags (`+0xB8`, `+0xE0`-`+0xE9`), sets the flow variable to 4, clears bits 1, 2, 4, 8, 0x10, 0x20 and 0x40 of the permission mask `0x80321DC0` (`fn_800344B4`; `fn_800FC528` sets them again), calls `fn_800FC858` and leaves status `mgr+0xD4 = 1` | `0x800FCBEC`, `0x800FCC38`-`0x800FCC94`, `0x800FCCEC`, `0x800FCCF4` |
| `fn_800FCD48` | Forced start: like `StartEvent`, but without checking permissions or consumed bits and without marking anything; only `TitleTask` calls it (ids `0x10A`, `0x35` and `0x41`-`0x45`) | `0x800FCD64` calls `fn_800FCBD0`; callers `0x800F2788`, `0x800F3260`, `0x800F33A0`-`0x800F33E0` |
| `fn_800FC858` | Event return point (`mgr+0x70..0x7C` position, `+0x80` area) and next state `mgr+0x84 = 0x28`; reads the placement table `0x8026C000` (8 bytes per event; `-3` = "come back where the player was") | `0x800FC880`-`0x800FC888`, `0x800FC9A8` |
| `fn_800FCE04` | Manager update; on status 1 creates the context (`fn_80051904`) and starts the thread (`fn_80051784`); every frame calls `fn_8005162C`; if the thread ended (or the skip flag `+0xE9` is set) calls `fn_800FC528` | `0x800FCEE0`, `0x800FCF24`, `0x800FCF54`, `0x800FCF64`, `0x800FCF80` |
| `fn_8005162C` | `run(context, dt)`: runs `fn_800522EC` on the current thread and stores the next one; returns whether it is still alive | `0x8005164C`-`0x80051660` |
| `fn_800522EC` | The interpreter: if the wait timer (`thread+0x54`) exceeds `dt` it lowers it and returns; otherwise it executes instructions until a native function sets bit 2 of `thread+0x48` (yield) | `0x80052304`-`0x80052318`, jump table `0x80219D0C` |
| `fn_800FC528` | The only event teardown: frees actors, sets `mgr+0x94 = 0x119` (none) and `mgr+0xD4 = 4`; if the event is `0x117` it requests state 2 | `0x800FC734`-`0x800FC738`, `0x800FC754`, `0x800FC760` |
| `fn_80034868` | Game state machine (table `0x80215D28`, 43 states, queue in `*0x80321D1C`); state 4 waits for `mgr+0xD4 == 4` and queues `mgr+0x84` | `0x80034EE8`-`0x80034F70` |

Objects: the **context** (0x18 bytes, `fn_80051904`) holds the running thread
(`+0`), the main thread (`+4`) and the stack of 128 eight-byte slots (`+8`
top, `+0xC` base, `+0x10` limit). The **thread** (0x5C bytes) holds the program
counter (`+4`), the address of the current instruction (`+8`, used to retry),
`dt` (`+0x24`), the message file (`+0x40`), the flags (`+0x48`, bit 2 =
yield), the context (`+0x4C`) and the wait timer in seconds (`+0x54`).

A script gives up control when a native function sets the yield bit; the
interpreter then returns to the game until the next frame. `WAIT` also arms the
timer; `WAIT_MESSAGE` winds the program counter back to its own instruction
(`fn_80052D48`) while the message window is busy, so it is retried every
frame.

### The `.lsb` file format

Everything is big-endian. Real example (`Eve_rm01`): `FFFFFFFF 00000C9F
FFFFFFFF 00001C8B 00000000` = no init entry, `main` at `0xC9F`, no argument
entry, string table at `0x1C8B` (the file size), 0 globals.

| Offset | Size | Field | Evidence |
|---|---|---|---|
| `0x00` | u32 | **init** entry (code offset, `0xFFFFFFFF` = none); runs to completion before `main` | `fn_80052E90`; always absent on the disc |
| `0x04` | u32 | **main** entry: where the event starts | `fn_80052E5C` (called by `fn_80051784`) |
| `0x08` | u32 | Entry that takes **one integer argument** (used by `fn_80052DDC` to spawn threads); present in 67 scripts | `0x80052E28` |
| `0x0C` | u32 | File offset of the **string table**; equal to the file size (empty table) in all 291 scripts | `fn_80052EEC`, thread `+0x1C`; checked by `--check` |
| `0x10` | u32 | **Global count** (0, 1 or 4 on the disc) | `fn_80052EEC` |
| `0x14` | 8 × N | Globals: initial slots (all `int 0`) | `fn_80052EEC` copies them into an array of each thread's own |
| `0x14 + 8N` | … | **Code**, up to the string table | `thread+0` = `image + 0x14 + 8N` |

Entries are measured from the start of the code; the operands of `JUMP`,
`JUMP_IF_ZERO` and `CALL` are measured from the opcode itself (the handlers
start from `pc - 1`), so code can move without recomputing anything.

**Operand integers.** Big-endian groups of 7 bits; the top bit of every byte
but the last is 1; the result is sign-extended from `7 × n` bits. `80 5B` =
91, `81 66` = 230, `FF 01` = -127, `97 38` = 3000, `7E` = -2. `PUSH_FLOAT` is
the exception: it carries 4 raw IEEE 754 bytes.

**Stack.** 8-byte slots `{s16 type, s16 flags, u32 value}`. Types seen in the
handlers: 1 integer, 2 float (native functions convert with `fctiwz` when
reading arguments), 3 string, 4 array, 5 reference, 6 call record, 7 marker, 8
element reference. Binary operators compute **TOP op SECOND** (the right
operand is pushed first): `a - b` is `PUSH b`, `PUSH a`, `SUBTRACT`. `CALL`
pushes a call record (type 6); `ENTER n` reserves `n` local slots; locals are
numbered `0..n-1`, the record is `-1` and the arguments `-2, -3, …`. `RETURN
n` releases `n + 1` slots (locals, record and arguments).

**Instructions** (jump table `0x80219D0C`, 0x34 entries):

| Code | Name | Operand | Effect | Handler |
|---|---|---|---|---|
| `00`, `32` | `NOP`, `NOP_32` | – | nothing | `0x80052C90` |
| `01` | `ENTER` | n | reserve n empty locals | `0x80052368` |
| `02` | `PUSH_INT` | n | push an integer | `0x80052554` |
| `03` | `PUSH_FLOAT` | 4 bytes | push a float | `0x80052560` |
| `04` | `PUSH_STRING` | n | pointer into the string table + n (unused) | `0x8005256C` |
| `05`-`07` | `LOCAL_REF/GET/SET` | n | reference / value / assign local n | `0x80052578` / `84` / `90` |
| `08`-`0A` | `GLOBAL_REF/GET/SET` | n | same for globals | `0x8005259C` / `A8` / `B4` |
| `0B` | `CALL_BUILTIN_VALUE` | n | call native function n and push its result | `0x800525C0` |
| `0C` | `CALL_BUILTIN` | n | call native function n, drop the result | `0x800526D8` |
| `0D`-`10` | `UNKNOWN_0D`…`10` | n | not on the disc; not analysed | `0x800527E4`-`0x80052810` |
| `11` | `JUMP` | n | go to `address + n` | `0x8005281C` |
| `12` | `JUMP_IF_ZERO` | n | pop an integer; jump if it is 0 | `0x800528DC` |
| `13`, `14` | `CALL_VALUE`, `CALL` | n | call the code at `address + n` | `0x800529F0`, `0x80052A00` |
| `15` | `RETURN` | n | return; release n + 1 slots | `0x80052A10` |
| `16` | `RETURN_VOID` | – | same as `RETURN 0` | `0x80052A6C` |
| `17`-`19` | `NEW_ARRAY`, `ARRAY_GET`, `ARRAY_SET` | – | arrays; unused | `0x80052A78`-`0x80052A90` |
| `1A` | `PUSH_RESULT` | – | push the last result; unused | `0x80052A9C` |
| `1B`-`2C` | `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `MODULO`, `SHIFT_LEFT`, `SHIFT_RIGHT`, `BIT_AND`, `BIT_OR`, `BIT_XOR`, `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_OR_EQUAL`, `GREATER`, `GREATER_OR_EQUAL`, `LOGICAL_AND`, `LOGICAL_OR` | – | pop two slots, push one | `0x80052AAC` (shared) |
| `2D`-`2F` | `NEGATE`, `BIT_NOT`, `LOGICAL_NOT` | – | pop one, push one | `0x80052AB8` |
| `30`, `31` | `PRINT`, `PRINT_LINE` | – | print through `OSReport` and yield; unused | `0x80052AC4`, `0x80052B4C` |
| `33` | `LINE` | n | stores n in `thread+0x28` (source line?); unused | `0x80052BE0` |

### Native functions (built-ins)

`CALL_BUILTIN n` and `CALL_BUILTIN_VALUE n` compare `n` with the counter at
`0x80322E54` (it holds 230) and, if there is a pointer at `0x80219EAC + 4n`,
call it with the thread in `r3`; an empty entry does nothing. 188 are filled
and scripts use 160 distinct ones (the highest is 221). `python3 tools/lsb.py
--builtins` lists them with their address and how often scripts call them.
Mind the coincidence: the counter holding `0xE6` (230) and the wedding flag
being number 230 (`0xE6`) are unrelated.

Identified (the tool names them; each one was read in the DOL):

| n | Name | Function | What it does |
|---|---|---|---|
| 6 | `WAIT` | `fn_800631A0` | Pops a number (an integer is converted to float), stores it in `thread+0x54` and yields |
| 48 | `YIELD` | `fn_80062EC8` | Only sets the yield bit |
| 51 | `SHOW_MESSAGE` | `fn_800629D0` | Pops an integer and calls the message window (`fn_80084FCC` on `0x802DBA28`) with the thread's file (`+0x40`) |
| 62 | `WAIT_MESSAGE` | `fn_800627C0` | If the byte at `0x802DBA28 + 0x340` is non-zero, retries the instruction and yields |
| 55 | `GET_GAME_VALUE` | `fn_80060D94` | Pops a selector (< 0xC0) and dispatches through a 192-entry table (`0x8021A58C`) that reads game counters; selector 1 reads `save+0x227C` |
| 56 | `SET_FLAG` | `fn_80060D14` | Pops `n` and sets bit `n` of the main set (`save+4`, `fn_800A7824`) |
| 57 | `CLEAR_FLAG` | `fn_80060C94` | Clears that bit (`fn_800A7804`) |
| 58 | `TEST_FLAG` | `fn_80060BE8` | Reads that bit (`fn_800A77DC`) and pushes 0 or 1 |

Bits are numbered like this: bit `n` is bit `n & 7` of byte `n >> 3`
(`fn_800A77DC`, `fn_800A7804`, `fn_800A7824`). Flag 230 is bit 6 of byte 28 of
`save+4`, that is `save + 0x20`, mask `0x40`.

Read only in part (proposed names at the end, none applied):

| n | Function | What it does | Uses |
|---|---|---|---|
| 65 / 66 | `fn_80060BC4` / `fn_80060BA0` | Resumes / pauses the field tasks (`fn_80034820` / `fn_80034844`) and starts a fade on `*0x802AEDC0` (`fn_8002BCF8` / `fn_8002B920`) | 278 / 284 scripts |
| 67 | `fn_800608E4` | Retries while `fn_800346CC()` is 0; otherwise arms a timer and yields (looks like "wait for the fade") | 287 scripts |
| 68 | `fn_800608B8` | Yields and posts message 1 to the task queue (`fn_800347F4`) | 414 calls in 286 scripts |
| 69 | `fn_80060880` | Yields, destroys the event's actors (`fn_800FC480`) and posts message 2 (`fn_800347C8`): the end of the event; the wedding and epilogue scripts call it last | 418 calls in 287 scripts |
| 70 | `fn_80060804` | Stores `fn_800A232C(n)` at `0x80321D3C`; `fn_800338A0` (new day) copies it to `save+0xD454` (where the player wakes up?) | 286 scripts; the wedding calls `70(26)` |
| 91 | `fn_8005B868` | Sends `0x80000002` to an object: the same message the event teardown uses to destroy each actor | 118 scripts |
| 105 | `fn_8005B460` | Pops three integers and calls `fn_80097FDC` (screen fade); the wedding calls it with `0, -127, 3000`, the same values as the new-day state 0x26 (`0x80036280`-`0x80036288`) | 270 scripts |
| 128 / 129 | `fn_8005A594` / `fn_8005A54C` | Ask the game for state 26 (`0x1A`) / state 42 (`0x2A`, idle) through `fn_8003468C` | 23 scripts each; **none of the wedding or the epilogue** |
| 146 / 147 | `fn_80059380` / `fn_8005929C` | Reach the flow variable (`fn_80034684`) | `Eve_F02`, `F05`, `F12`, `F13`; none of the wedding |

### Tool and tests

`tools/lsb.py` reads a script (`parse_script`, `load_script`), disassembles it
(`disassemble`) and offers `--check`, `--builtins` and `--file-offsets` (shows
file offsets, handy for patches). The tests (`tests/test_lsb.py`) use
synthetic scripts built by an independent encoder (`tests/synthetic_script.py`)
and, if `HM_MM_DOL` points to your `main.dol`, also check the real data of the
disc: 291 scripts, headers, the set of unused opcodes, the operand order of
subtraction, the places that touch flag 230 and the facts about the native
table.

### The Jamie wedding

| Script | Size | What it is | Evidence |
|---|---|---|---|
| `Eve_marry00` | 1,112 B of text | Village advice before marrying ("…one trial") | Text of `eve_marry00_e.bin` |
| `Eve_marry02` | – | Ordinary wedding (ceremony and banquet); does not touch flag 230, so it does not enter the ending chain | Text of `eve_marry02_e.bin`; ends with `CALL_BUILTIN 91` and `69` |
| `Eve_rm00` (`0x10C`) | 2,639 B | Reaction to the engagement ("Oh, you proposed to Jamie!") | Text of `eve_rm00_e.bin` |
| `Eve_rm01` (`0x10D`) | 7,307 B | Ceremony with Jamie, player with `save+0x227C == 0` | `0x8016D1B8` |
| `Eve_rm02` (`0x10E`) | 7,271 B | The same ceremony with the gender parameters swapped; shares the first `0xCCE` bytes of code with `Eve_rm01` | `0x8016D1EC` |
| `Eve_EP00`…`EP08` (`0x10F`-`0x117`) | – | Epilogue: eight scenes with text and one (`EP08`) without | `eve_EP08_e.bin` does not exist on the disc |

- `Eve_rm01.lsb` and `Eve_rm01_e.lsb` are byte-identical; `Eve_rm01_eu.lsb`
  differs in a single `WAIT` (2 instead of 3, code `0x12B9`). The same goes for
  `rm02`. The USA DOL's name table (`0x8026BBA0`) points to `Eve_rm01` and
  `Eve_rm02` without suffix, and the loader appends none
  (`"/Script/" + name + ".lsb"`), so those are the files that are read.
- Structure of `Eve_rm01`: three shared helper functions (`0x0`, `0xF9`,
  `0x11D`; the first `0x22D` bytes are shared by 36 scripts, e.g.
  `Eve_marry02`), a fourth at `0xBEA` and `main` at `0xC9F` (`ENTER 9`,
  `PUSH_INT 26`, `CALL_BUILTIN 70`, …). The ceremony is loops of actors and
  messages; **the closing starts at `.L1C1E` (code `0x1C1E`)**:

```
  01C40  02 12           PUSH_INT 18
  01C42  0C 33           CALL_BUILTIN 51  ; SHOW_MESSAGE     ("Several Seasons Later..." card)
  01C44  0C 3E           CALL_BUILTIN 62  ; WAIT_MESSAGE
  ...
  01C5D  02 FF 01        PUSH_INT -127
  01C60  02 97 38        PUSH_INT 3000
  01C63  0C 80 69        CALL_BUILTIN 105                    (fade to black, 3 s)
  01C66  03 40 00 00 00  PUSH_FLOAT 2
  01C6B  0C 06           CALL_BUILTIN 6  ; WAIT
  01C6D  02 00           PUSH_INT 0
  01C6F  0C 80 67        CALL_BUILTIN 103
  01C72  0C 80 45        CALL_BUILTIN 69                     (end of the event)
  01C75  15 09           RETURN 9
```

  Message 18 is the last text of `eve_rm01_e.bin` and is that card. In
  `Eve_rm02` the same sequence is at `0x1C1C`.
- Neither `Eve_rm00`, `Eve_rm01`, `Eve_rm02` nor the `Eve_EP*` set or clear
  flags with constants, and none of them calls the functions that request
  states (128, 129, 146, 147). The end of the game is not decided in the
  scripts.
- Flag 230 is touched only by scripts in `talk` (`PUSH_INT 230` at `0xC0B`
  sets it; at `0xCA6` and `0xCDE` it clears it). The branch at `0xC0B` is
  reached when a variable filled by `CALL_BUILTIN 117` is 1119,
  `CALL_BUILTIN_VALUE 113` returns 2 and `CALL_BUILTIN_VALUE 220` returns 1; it
  answers with a message that depends on `GET_GAME_VALUE(1)`. No script tests
  it (`TEST_FLAG` with 230 does not occur); native code does.

### What ends the game

The whole chain, with addresses (all in `main.dol`):

| Step | Where | What happens |
|---|---|---|
| 1 | `talk.lsb` `0xC0B` | Sets flag 230 (bit 6 of `save + 0x20`) |
| 2 | `fn_80166888` `0x8016D07C` | With the player in area 26 (index 26 of the table `0x802A341C`, target `0x8016D070`) and flag 230 set it jumps to `0x8016D188`. It is the only native place that tests that flag |
| 3 | `0x8016D190` | `StartEvent(0x10C)` (`Eve_rm00`); each call returns 1 only if the event started, and that pass over the field ends there |
| 4 | `0x8016D1B8`-`0x8016D204` | If `save+0x227C == 0` → `StartEvent(0x10D)`; otherwise → `StartEvent(0x10E)`. First it sets `save+0x1BC8 = 0` |
| 5 | `0x8016D24C`-`0x8016D544` | `StartEvent(0x10F)` … `StartEvent(0x117)`; whatever fails falls through to the next. `EP00`-`EP05` have counter and flag conditions; `EP06`, `EP07` and `EP08` do not, so with flag 230 set those three end up playing. Right before `EP08` (`0x8016D50C`-`0x8016D538`) it writes `save+0x2234 = 0`, `save+0x2258 = 12` and `save+0x1BC8 = 0`, every time a pass gets there. Every id is requested from exactly one place in the DOL, all inside `fn_80166888` |
| 6 | `fn_800FC528` `0x800FC734`-`0x800FC738` | If the event that ends is `0x117` (`Eve_EP08`): `fn_8003468C(2)` |
| 7 | `fn_80034868` state 2 (`0x80034E4C`) | Creates `TitleTask` (`fn_800F350C`, string `0x801E67F8`) with parameter 1, which leaves it in sub-state 1; sub-states 1 and 2 (`0x800F2850`, `0x800F2994`) wait 3.0 s and 5.0 s and move on to sub-state 3, the title (with parameter 0 it starts straight in 3) |

States of `fn_80034868` that matter here (jump table `0x80215D28`; the state
is popped from the queue at `0x800348B0` and stored at `0x80321D18`):

| State | What it does | Address |
|---|---|---|
| 1 / 2 | Creates `TitleTask` with parameter 0 / 1 and pushes state `0x2A` | `0x80034DFC` / `0x80034E4C` |
| 3 | Creates `EndingTask` (`fn_80163D98`, name `"ENDING"`) and pushes state `0x2A`; nobody requests it | `0x80034E9C` |
| 4 | Waits for the event manager to reach status 4 and pushes `mgr+0x84` | `0x80034EE8`-`0x80034F6C` |
| 9 | New day: after a wait it calls `fn_800338A0` and pushes state `0x26` | `0x800361B0` |
| `0x26` | New-day fade (`fn_80097FDC` with `0, -127, 3000`) and pushes `0x25` | `0x80036278` |
| `0x28` | Field: on the normal path (`0x80321D2C != 5`) it calls `fn_80166888` once per frame (its only caller); if it returns 1 (an event started) it pushes state 4 | `0x800363C4`, call at `0x80036498`, push at `0x80036628` |
| `0x2A` | Idle: does nothing | `0x80036660` |

Why it is native and not a script:

- `StartEvent` requires the event not to be consumed and consumes it as it
  starts; the chain advances because each event gets marked and the next pass
  over the field asks for the next one. Scripts take no part in that decision.
- Following direct calls up to eight levels deep, the only native functions
  that reach the state queue (`*0x80321D1C`, loaded by only ten functions:
  `fn_80034868`, `fn_8003468C` and eight neighbours between `0x8003405C` and
  `0x8003705C`) or the flow variable (`fn_80034684`) are numbers 128, 129, 146
  and 147, and none of the 12 scripts of the chain (`Eve_rm00`-`rm02`,
  `Eve_EP00`-`EP08`) calls them. Others (68, 69, 71, 79, 90, 91, 95, 126,
  141, 163, 174, 181) reach `fn_801B4B7C`, a generic "push to a queue", but
  none of them reaches a function that loads the state queue; 68 and 69 use
  the task manager's queue (`0x80321D38`).
- `fn_8003468C(2)` has a single caller in the whole DOL (`0x800FC738`) and
  the function is only ever called with `bl`, never through a pointer. Its 50
  callers pass a constant (0, 2, 4, 8, 20-23, 25, 26, 28-33, 36 or 42; the 2
  only at `0x800FC738`) and, of the 73 direct pushes to the state queue, 71
  carry a constant that is never 2 or 3. The other two are the one inside
  `fn_8003468C` (its argument) and the one in `fn_80034868` at `0x80034F6C`,
  which pushes `mgr+0x84`; that field is written only by the manager's
  constructor (`0x2A`), `fn_800FC858` (`0x28`) and `TitleTask` (`0x2A`, `9`,
  `0x27`).
- `Eve_EP08` ends like any other event (`CALL_BUILTIN 103`,
  `CALL_BUILTIN 69`, `RETURN 4`) and has no message file. A staff roll does
  exist in the DOL: it is `EndingTask` (state 3), whose credits are strings
  from `0x801ECC34` (`Harvest Moon`, `- Magical Melody -`, `Character design`,
  `Director`...) with the texture `/menu/eng/c_mii.tpl`, which is on the disc;
  the `message_*/staff/` folders are empty. But nobody requests state 3, so it
  is not part of this chain: after `Eve_EP08` the game goes to state 2, not 3.
- Ids `0x10B`-`0x117` are requested only from `fn_80166888`; the variable id
  in `fn_80034868` (`0x80034FC4`) comes from `fn_80034674`, whose callers use
  small ids (`0x00`-`0x36`, `0x56`; one more, `0x0F`, through `fn_801643FC`).

Confidence: **high** for the chain (every step was read in the disassembly and
cross-checked against all call sites). What is not covered: calls through
function pointers (they do not show up as `bl`) and any run-time behaviour.

### Recommended mod design

For the ending not to arrive it is enough that flag 230 is clear when
`fn_80166888` reaches `0x8016D07C`. There are three places to achieve that;
the first is the recommended one. In all of them, `save = *(u32*)0x80321F48`:
flag 230 is the byte `save + 0x20`, mask `0x40`, and the wedding's consumed
bits (`0x10D`, `0x10E`) are the byte `save + 0x8D`, mask `0x60`.

**Option A (recommended): guard in the field check.**

- `RECOMP_HOOK(0x80166888, ...)`. `fn_80166888(field)` is called only by state
  `0x28` (`0x80036498`), once per frame, and is the only code that tests flag
  230. A hook runs before the original, so clearing the flag before it tests
  it is enough.
- Rule: if event `0x10D` or `0x10E` is already consumed, clear the flag.
  `StartEvent` marks the event when it **starts**, not when it ends, so the
  rule holds from the first pass over the field after the wedding.
- Why it is the most robust: it does not need to know which event ended or
  how (normal end or skip); it does not touch the wedding in progress (the
  field is not checked while an event runs and nothing else reads the flag:
  in the DOL only `0x8016D07C` tests it and no script has `TEST_FLAG 230`);
  and if `talk.lsb` sets it again later (proposing to Jamie again) it is
  cleared on the next pass, so the counter writes that the chain makes before
  `Eve_EP08` are not repeated either. It is idempotent and costs two byte
  reads per frame.

Pseudo-code (adapt the memory accesses to the real mod API):

```
hook(0x80166888):                          # fn_80166888(field), once per frame in state 0x28
    save = read_u32(0x80321F48)
    if read_u8(save + 0x8D) & 0x60:        # event 0x10D or 0x10E already consumed
        write_u8(save + 0x20, read_u8(save + 0x20) & ~0x40)   # main flag 230 off
```

**Option A+ (optional): end the wedding on a new morning.** It adds to A and
does not replace it.

- `RECOMP_HOOK(0x800FC528, ...)`: `fn_800FC528(manager)` is the only event
  teardown and is called both when the thread ends and on the skip path
  (`0x800FCF64`, `0x800FCF80`). `r3` is the manager (`*0x80321D88`) and
  `[r3 + 0x94]` the event id; the function resets it to `0x119` later, so it
  has to be read **on entry**.
- If the id is `0x10D` or `0x10E`: `*(u32*)(r3 + 0x84) = 9`. State 4 pushes
  `mgr+0x84` when the event ends (`0x80034F68`) and `fn_800FC858` resets it to
  `0x28` when preparing each event, so the change lasts only once. State 9 is
  the start of a new day (it waits, calls `fn_800338A0` and moves to state
  `0x26`, a fade); the game itself uses it after its opening event
  (`0x800F3278`).
- It still has to be checked at run time that state 9 advances the calendar
  and where the player wakes up: the wedding calls `70(26)` and `fn_800338A0`
  copies that value (`0x80321D3C`) to `save + 0xD454`.

**Option B: a single hook, on event teardown.** The same `0x800FC528` hook,
with no per-frame hook: if the id is `0x10D` or `0x10E`, it clears flag 230
and marks `0x10F`-`0x117` as consumed (`*(u8*)(save + 0x8D) |= 0x80` and
`*(u8*)(save + 0x8E) = 0xFF`). It works, but it runs only once: if `talk.lsb`
sets the flag again the chain comes back; the consumed `EP*` events do not
start, but the pass writes the counters at `0x8016D50C`-`0x8016D538` again
(among them `save + 0x2258 = 12`).

**Option C: patch the script bytes.** The closing of `Eve_rm01` has six bytes
that can be changed without moving anything: in the file, at `0x1C54`
(`Eve_rm01`) and `0x1C30` (`Eve_rm02`), `02 12 0C 33 0C 3E` (`PUSH_INT 18;
SHOW_MESSAGE; WAIT_MESSAGE`) becomes `02 81 66 0C 39 00` (`PUSH_INT 230;
CLEAR_FLAG; NOP`). It clears the flag from the script itself and drops the
card as well.

**Comparison.**

| | A: guard in the field | B: hook on event teardown | C: script patch |
|---|---|---|---|
| Where | `0x80166888`, every frame | `0x800FC528`, every event end | bytes of `Eve_rm01` and `Eve_rm02` |
| Covers both genders and a skipped event | Yes, one single point | Yes, one single point | Both files need the patch; it does not run if the event is skipped |
| If `talk.lsb` sets the flag again | Cleared on the next pass | No: the chain comes back | No: same |
| Can set the next state (9) | With A+ | Yes | No |
| Depends on | The mod API allowing writes to guest memory; fixed `G4AEE9` addresses | Same; hooks cannot skip the original, so bits are used instead of blocking calls | Applying the patch when the file is read (never ship the modified `.lsb`); fixed `G4AEE9` addresses; changes the story text |

An extra net (not needed): a hook on entry to `fn_8003468C` that changes `r3`
from 2 to another state. It works because state 2 is requested from one place
only, but it lets the whole epilogue play, so it is only a fallback.

What the save remembers about "married to Jamie": nothing in the wedding
scripts writes it; all that is left is the consumed bits `0x10C`-`0x10E`
(`save + 0x6C`). They are the natural marker for the rest of the mod (for
example `read_u8(save + 0x8D) & 0x60`).

**What to test at run time** (in this order): (1) marry and check that
`Eve_EP00` does not start and the game does not return to the title; (2) save
and load after the wedding and check that flag 230 is still clear; (3) talk to
Jamie again with the proposal item and check the chain does not come back and
that `GET_GAME_VALUE(126)` (`save + 0x2258`, which the chain leaves at 12) does
not change; (4) with A+, check the date, place and music on waking up; (5)
repeat with a player of each gender.

### What is not verified

- Nothing was run: not the chain, not the hook, not the patch.
- Area 26 was not identified, nor where the player ends up after the wedding;
  from the placement table (`-3`) they return to where they were when the
  event started.
- It is not known whether state 9 advances a day, nor what exactly
  `save + 0xD454` is (see native function 70).
- The other state-writing built-ins (109-112, 133, 151, 153, 156-160) were not
  audited, nor what an ordinary wedding (`Eve_marry*`) changes in the save;
  copying that for Jamie would be the "complete" way of being married.
- What would start `EndingTask` (the staff roll, state 3): no static code
  requests that state. It may be unused code or be reached by a path that
  static analysis does not see; at run time it has to be checked whether the
  credits appear after `Eve_EP08` or the title simply comes back.
- What `save + 0x2258`, `save + 0x2234` and `save + 0x1BC8` mean; the chain
  rewrites them before `Eve_EP08`. `0x2258` is read in about a hundred places
  in the DOL (state 0 compares it with 5 and 6) and `GET_GAME_VALUE`
  selectors 126 and 129 read `0x2258` and `0x1BC8`; what they represent is
  unknown. Option A prevents the write, but it is not known whether a game that
  carries on would need a different value.

### Proposed names

None was applied to `config/G4AEE9/symbols.txt`; doing so requires recording
the evidence in `docs/FUNCTION_MAP.md`.

| Address | Proposal | Evidence in this document |
|---|---|---|
| `0x800522EC` | `lsrun_step` | Interpreter |
| `0x8005162C` | `lsrun_run` | How a script runs |
| `0x80051784` | `lsrun_start_main` | Assertion with `"lsrun.cpp"`, main entry |
| `0x80055068` | `lsrun_load_script` | Builds `/Script/….lsb` |
| `0x800FC2B4` | `EventManager_StartEvent` | How a script runs |
| `0x800FCBD0` | `EventManager_Setup` | Same |
| `0x800FC858` | `EventManager_PlaceReturnPoint` | Same |
| `0x800FCE04` | `EventManager_Update` | Same |
| `0x800FC528` | `EventManager_Finish` | The only teardown |
| `0x80166888` | `Field_CheckEvents` | Wedding chain |
| `0x80034868` | `GameState_Update` | Table of 43 states |
| `0x8003468C` | `GameState_Request` | Pushes a state on the queue |
| `0x800A77DC` / `0x800A7804` / `0x800A7824` | `BitSet_Test` / `_Clear` / `_Set` | Bit layout |
| `0x800A7940` / `0x800A7950` | `Counter_Get` / `Counter_Set` | Counter reads and writes |
| `0x800F350C` / `0x800F26F4` | `TitleTask_Construct` / `TitleTask_Update` | String `"TitleTask"` |
| `0x80163D98` | `EndingTask_Construct` | State 3, string `"EndingTask"` |
| `0x800FCD48` | `EventManager_ForceStart` | Forced start |
| built-in 65 / 66 / 67 / 69 / 70 / 91 / 105 | `RESUME_AND_FADE` / `PAUSE_AND_FADE` / `WAIT_FADE` / `END_EVENT` / `SET_RETURN_LOCATION` / `DESTROY_ACTOR` / `FADE` | Native functions table |
