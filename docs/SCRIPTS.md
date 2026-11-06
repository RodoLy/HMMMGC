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
fn_80166888  revisión del campo (estado 0x28), una vez por visita a ese estado, según el área
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
| `0x28` | Campo: en el camino normal (`0x80321D2C != 5`) llama a `fn_80166888` (es su único llamador); si devuelve 1 (arrancó un evento) empuja el estado 4 y si no el 6, así que dura un cuadro por visita. Se entra desde el estado `0x25` (día nuevo: 9 → `0x26` → `0x25` → `0x28`), desde `0x21` y `fn_800342A8`, y al terminar un evento (el estado 4 empuja `mgr+0x84`, que `fn_800FC858` deja en `0x28`) | `0x800363C4`, llamada en `0x80036498`, empuje en `0x80036628` (4) y `0x80036514` (6) |
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
  estado `0x28` (`0x80036498`), una vez cada vez que entra en él, y es el único
  código que prueba el flag 230. Un gancho corre antes que el original, así que
  basta con apagar el flag antes de que lo pruebe.
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
  dos lecturas de un byte por revisión.

Pseudocódigo (adaptar los accesos a memoria al API real de mods):

```
hook(0x80166888):                          # fn_80166888(field), once per visit to state 0x28
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
| Dónde | `0x80166888`, cada revisión del campo | `0x800FC528`, cada fin de evento | bytes de `Eve_rm01` y `Eve_rm02` |
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
sexos. El punto (1) se probó con condiciones escritas a mano (ver la sección
siguiente); los demás siguen pendientes.

### Prueba en ejecución del mod (2026-11-04)

`jamie_marriage` 0.1.0 se probó dentro del juego recompilado, pero **sin
jugar la historia**: las condiciones de la boda se escribieron a mano en la
memoria de un estado guardado. Demuestra qué hace el gancho en esa revisión
del campo; no demuestra una partida real de principio a fin (ver "Qué no está
verificado").

**Cómo se hizo** (el material de prueba es desechable y no está en el
repositorio):

- Un estado de emulación guardado durante el cambio de día, en el estado
  `0x25`, justo antes de la primera revisión del campo (estado `0x28`). Cada
  prueba lo carga con el juego en pausa, escribe las condiciones (flag 230 encendido; eventos
  `0x10C`-`0x10E` consumidos o no) y reanuda. Sin vídeo (`--graphics Null`);
  con dos instancias a la vez en dos núcleos el juego fue a unas 0,4 veces su
  velocidad.
- El cambio de día reescribe el índice de campo (`0x80321D40`) y la revisión
  veía `27` en vez de `26`: arrancaba el evento `0x37`, no la boda. Un mod
  auxiliar desechable escribe `26` solo en la **primera** revisión; todo valor
  posterior del índice lo pone el propio juego.
- Cada caso se corrió sin el mod (control) y con él. La evidencia es el
  registro del juego: `state_logger`, una traza desechable de `StartEvent` y
  la línea del propio mod.
- En la cadena entera, un guion pulsa A mientras corre un evento. Empieza con
  `Eve_rm00` (`0x10C`) ya consumido: con los contadores de ese estado guardado
  se repite, porque su cierre (`fn_800FC528`) lo deja otra vez sin consumir
  mientras `save + 0x4004 == save + 0x3FC8`.

**Resultados** (índice de campo 26, flag 230 encendido):

| Caso | Sin el mod | Con el mod |
|---|---|---|
| Boda sin empezar | Arranca `Eve_rm00` (`0x10C`) | Igual; el flag sigue encendido |
| Ceremonia ya empezada, primera revisión | Arranca `Eve_EP06` (`0x115`) | Apaga el flag (una línea en el registro) y arranca `Eve_music00` (`0x10B`), no el epílogo |
| Cadena entera desde la ceremonia | `0x10D` → `0x115` → `0x116` → `0x117`; el cierre de `0x117` pide el estado 2 (llamada en `0x800FC738`), luego el `0x2A` y arranca el título; el estado 3 no aparece | `0x10D` → `0x10B` → estado 6 → estado 0 (juego libre), y ahí se queda con el flag apagado |

En el control salen directamente `EP06`-`EP08`: las condiciones de
`EP00`-`EP05` (contadores de `save`) no se cumplían en ese estado guardado, ni
siquiera se llegó a pedirlos.

**Qué se aprendió:**

1. *El gancho actúa en la pasada justa.* La primera revisión tras la
   ceremonia (estado 4 → `0x28`) ve `0x10D` consumido y el flag encendido, lo
   apaga, y esa misma revisión se salta la rama de la boda. Los contadores
   que la cadena reescribe antes de `Eve_EP08` quedaron como estaban:
   `save + 0x2258` siguió en 6 hasta el final (la cadena lo deja en 12); los
   otros dos ya valían 0 en ese estado guardado, así que no distinguen.
2. *Sale una vez la escena de las notas.* Con el flag apagado, el manejador
   del índice 26 cae en su rama "sin flag". Si no toca ninguno de los dos
   eventos ligados al contador `save + 0x60` (`0x105` y `0x106`, con las
   banderas `0xBF`, `0xC0` y `0xC5`), su valor por defecto es
   `StartEvent(0x10B)` (`Eve_music00`, donde se dice que cinco notas se
   vuelven un instrumento). Esa escena sale una vez justo después de la
   ceremonia.
3. *No hay bucle en el juego.* `StartEvent` nunca deja consumido `0x10B` (ver
   arriba), así que, si el índice siguiera en 26, la escena se repetiría. No
   pasa: al terminar un evento el juego restaura el índice con la entrada de la
   tabla de colocación (`0x8026C000`), que para `0x10B` es el lugar `129`,
   posición `(10, 28)`, ángulo `270`. `fn_800A232C(129)` devuelve
   `fn_8002D148()`, un valor que el propio código deja entre 0 y 3
   (`0x8002D1AC`-`0x8002D1BC`) y por tanto nunca 26. El registro de una
   sonda desechable lo confirma: el índice valía 26 en la revisión que
   siguió a la ceremonia (lo restauró el juego, sin forzarlo) y 0 en la que
   siguió a la escena de las notas. Una versión previa de la prueba forzaba
   el 26 en **todas** las revisiones y la escena se repetía sin fin (10
   vueltas vistas): era un artefacto del ayudante, no del juego ni del mod.
4. *El gancho puede correr más de una vez por revisión* (en la primera, el
   registro de `state_logger` imprime "field check entered" dos veces) y el
   mod imprime una sola línea: la segunda pasada ya encuentra el flag
   apagado, como pide el diseño idempotente de
   [`mods/README.md`](../mods/README.md).
5. *Coste.* En 1 200 cuadros de juego libre en la casa (sin vídeo), el
   contador `fallback` del runtime fue 0 con y sin el mod, y los demás
   contadores por cuadro coinciden: `native` 554 545 sin el mod y 556 536
   con él, `hook_fb` 62 740 y 62 704. El bloque `[0x801644C0, 0x801684C0)`
   que el gancho manda al intérprete no se ejecutó en ese tramo. Solo se
   midió en la casa y en juego libre.

### Las conversaciones

Lo que dice un aldeano cuando le hablas sale de `talk.lsb`, el único script
que usa la función nativa 52 (`fn_800627F4`, 103 llamadas). Esa función saca
tres enteros (el personaje, donde 125 es «con quien habla el jugador» y
`fn_800A3428` lo traduce a su número; el número de mensaje; y otro valor) y
llama a `fn_8008534C(ventana, personaje, mensaje, ...)`, que:

1. toma la ruta del archivo de mensajes del personaje de la tabla de
   `0x801DFD40`, una palabra por número de personaje (la del 2, Jamie, es
   `/message_e/chara/Rion_e.bin`);
2. carga el archivo entero en un búfer nuevo con `fn_80029E74`;
3. se rinde si el número de mensaje es mayor que el más alto del archivo;
4. busca el texto por la tabla del archivo, lo pasa por `fn_80081E6C` (que
   pone los nombres) y por `fn_801A1C88` (un `vsprintf`: un `%` del texto
   sería un formato) y lo muestra con `fn_8008253C`;
5. libera el búfer con `fn_801B9D7C`.

Nadie más llama a `fn_8008534C`. Un archivo de mensajes empieza con el número
del personaje que habla y el número de mensaje más alto que tiene (16 bits
cada uno), cuatro bytes más, y dónde empiezan su tabla y su texto (32 bits
cada uno). La tabla tiene una palabra por mensaje, del 0 al más alto: dónde
empieza, contado desde el inicio del texto (`0xFF` es «sin mensaje»). En el
texto, `01 04 1D` es un renglón nuevo, `01 05 1D` una página nueva,
`01 08 1F 1D` el fin del mensaje, `01 0A` el nombre del jugador, `01 3E` cómo
lo llama su pareja, `01 40` el nombre del hijo y `01 3D` el de la pareja. Las
letras son las de la fuente de la consola (Windows-1252), salvo el signo de
interrogación, que es `0x9D`; `0x85` es `…`.

**Cada número es una situación**, el mismo para todos los personajes. En la
primera conversación del día (bit 0 del personaje apagado), `talk.lsb`
(subrutina `0x001A`) suma 5 a la amistad y prueba en este orden:

| Mensaje | Situación |
|---|---|
| 1 | La primera vez que hablas con el personaje (bit 2 apagado) |
| 70-85 | Días de festival (por estación y día) |
| 3, 4, 5 | Saludo según la hora: de 6 a 10, de 10 a 17 y el resto; después sigue otro mensaje |
| 8 | El día después de un tifón (clima de ayer 3) |
| 60 | Día de tormenta (clima 3) |
| 87 | Tu cumpleaños, con amistad de 300 o más |
| 9, lista, 100 | La primera conversación después de la boda (bit 3): pregunta cómo llamarte, muestra la lista de nombres y responde con el 100 |
| 10, 11 | La primera conversación tras saberse el embarazo (bit 4) y tras el nacimiento (bit 5) |
| 61-68 | En ciertos lugares (función nativa 114) |
| 12, 13 | Día de lluvia (clima 2), con amistad menor que 400 o desde 400 |
| 14-21 | Según la amistad: menos de 100, 200, ..., 800 |
| 26-30 | Casados (bandera 0), con amistad de 800 o más, la pareja y quien no es candidato: según los días de casados (menos de 20, 50, 80, 110, o más) |
| 31-35 | Lo mismo cuando el valor 43 vale 255: frases de hijos, según la edad del hijo |
| 90 | Casados, otro de los candidatos: «que seas feliz con» la pareja |
| 22-25 | Sin casarse, con amistad de 800 o más (22 y 23 hasta 899, 24 y 25 desde 900, según el sexo del jugador) |

En otra conversación del mismo día sale el 7 si el personaje es la pareja
(bit 8) y si no el 6. Los regalos (subrutina `0x0B2C`) usan del 36 al 51: el
37 es un regalo que le gusta en su cumpleaños (bit 6), del 41 al 45 van de
«le encanta» a «no le gusta», el 38 es la propuesta con la pluma azul y el 50
lo dice la pareja cuando te ve con el bebé en brazos.

Los valores que lee: la amistad es el contador de `save + 0x6628 + 12·n` (la
función nativa 122 lo lee y la 121 le suma). Los bits del personaje están en
`save + 0x5564 + 3·n` (nativas 123, 124 y 125: encender, apagar y probar).
Cada mañana el juego (`fn_80032EE8`, desde `0x800334F8`) apaga los bits 0 y 1,
enciende el 6 a quien cumple años ese día y el 8 a quien es la pareja
(`save + 0x22B8`). La fecha de cumpleaños sale de una tabla de los datos del
juego (`*0x80321F50 + 0x329E0`, 4 bytes por personaje: la estación más 2 en
los cuatro bits altos del primero y el día más 2 en el segundo;
`fn_800A7D3C` y `fn_800A7D14`). De `GET_GAME_VALUE`: el 1 es el jugador
(`save + 0x227C`: 0 el chico, que se casa con los personajes 3 a 12,
`fn_800A2B60`), el 3 y el 4 su cumpleaños, el 38 la pareja, el 124 la
estación (`save + 0x2234`), el 125 el día (`save + 0x224C`), el 126 la hora
(`save + 0x2258`), el 129 el clima de hoy (`save + 0x1BC8`), el 131 el de
ayer, el 154 los días de casados (`save + 0xCB54`), el 155 la edad del hijo
(`save + 0xCB6C`) y el 43 es `save + 0xCB60`.

Jamie (personaje 2) no tiene cumpleaños en esa tabla (estación -1, visto en
la memoria del juego corriendo), y en los números de la pareja su archivo
tiene frases de rival. El mod `jamie_marriage` 0.5.0 se las cambia cuando
Jamie es la pareja: engancha la entrada de `fn_8008534C` (elige la frase), el
regreso de `fn_80029E74` (la escribe en la copia del archivo recién cargada y
le apunta la tabla; para el 100, que el archivo no tiene, sube también el
número más alto) y el regreso de `fn_8008534C` (para que nada quede
pendiente). Elige el idioma por la carpeta de la ruta: el disco trae los
mismos archivos en `message_e`, `message_s`, `message_f`, `message_g` y
`message_i` (y en japonés en `message`), con los mismos números (el más alto
de Jamie es el 91 en todos), aunque el juego americano solo lee los de
inglés: las 170 rutas de mensajes de su programa son de `message_e`, y cada
una tiene su gemela en los otros cuatro idiomas europeos, salvo cinco que
tampoco existen en inglés. `tests/test_jamie_as_the_spouse.py` revisa que
cada frase quepa en la ventana y tenga solo letras de la fuente, y corre los
ganchos sobre una memoria falsa.

La tabla de cumpleaños sale de `system/Itemdata.bin` (desde `0x329E0`), que
el juego carga al arrancar. Desde la versión 0.6.0, mientras Jamie es la
pareja, el mod le da el 22 de invierno en esa copia de la memoria: ningún
otro personaje cumple ese día y ningún día 22 tiene frase de festival en
`talk.lsb` (los festivales del invierno son el 7, el 12 y el 19). Si Jamie no
es la pareja, le devuelve el «ninguno» del juego. Lo hace al entrar a cada
área (`fn_80042D10`), así que la fecha ya está cuando el juego revisa los
cumpleaños a la mañana siguiente.

### Qué no está verificado

- La prueba usó condiciones escritas a mano sobre un estado guardado, con el
  índice 26 forzado en la primera revisión. Falta una partida real: la
  propuesta de verdad (`talk.lsb` encendiendo el flag 230), guardar y cargar con
  la tarjeta de memoria, una segunda propuesta y los dos sexos del jugador.
  Tampoco se probó el mod compilado para Windows ARM64.
- Tras la ceremonia sale una vez la escena de las notas y, por su entrada de
  la tabla de colocación, el jugador queda en el lugar `129` (posición
  `(10, 28)`); eso no se observó. La opción A+ podría evitar la escena, pero
  no se probó.
- No se identificó el área 26. Para muchos lugares el índice de campo es el
  número de lugar (en el estado guardado de la prueba, lugar 27 e índice 27;
  la ceremonia guarda el lugar 26 con la función nativa 70), y otros (por
  ejemplo el `0x81`) pasan por `fn_8002D148` y dependen de contadores de la
  partida; qué mapa es el 26 no se sabe. Por la tabla de colocación (`-3`), la
  ceremonia devuelve al jugador a donde estaba al empezar el evento.
- No se sabe si el estado 9 avanza un día, ni qué significa exactamente
  `save + 0xD454` (ver la función nativa 70). Tampoco se probó dónde amanece
  el jugador el día siguiente a la boda: tras la ceremonia la variable de
  lugar (`0x80321D3C`) vale 26 (antes 27) y el cambio de día la copia a
  `save + 0xD454`.
- No se auditaron los demás built-ins que escriben estado (109-112, 133, 151,
  153, 156-160) ni qué cambia una boda normal (`Eve_marry*`) en la partida;
  copiarlo para Jamie sería la forma "completa" de estar casado.
- Qué activaría `EndingTask` (el "staff roll", estado 3): ningún código
  estático pide ese estado y en la prueba, tras `Eve_EP08`, no salieron
  créditos: el juego fue al estado 2 y al título. Puede ser código sin uso o
  llegar por un camino que el análisis estático no ve.
- La cadena reescribe `save + 0x2258`, `save + 0x2234` y `save + 0x1BC8` antes
  de `Eve_EP08`. Ya se sabe qué son: la hora, la estación y el clima del día
  (`talk.lsb` los lee para elegir el saludo de la hora, las frases de
  festival y las de lluvia; ver «Las conversaciones»). `fn_8002D148` lee la
  estación, el día y la hora, y devuelve un número de 0 a 3 que
  `fn_800A232C` usa como índice de campo de varios lugares, entre ellos el
  `0x81`. La opción A impide la escritura (en la prueba `0x2258` no cambió),
  pero no se sabe si una partida que continúa necesitaría otro valor.

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
fn_80166888  field check (state 0x28), once per visit to that state, depends on the area
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
| `0x28` | Field: on the normal path (`0x80321D2C != 5`) it calls `fn_80166888` (its only caller); if it returns 1 (an event started) it pushes state 4, otherwise state 6, so it lasts one frame per visit. It is entered from state `0x25` (new day: 9 → `0x26` → `0x25` → `0x28`), from `0x21` and `fn_800342A8`, and when an event ends (state 4 pushes `mgr+0x84`, which `fn_800FC858` leaves at `0x28`) | `0x800363C4`, call at `0x80036498`, push at `0x80036628` (4) and `0x80036514` (6) |
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
  `0x28` (`0x80036498`), once each time it enters that state, and is the only
  code that tests flag 230. A hook runs before the original, so clearing the
  flag before it tests it is enough.
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
  reads per check.

Pseudo-code (adapt the memory accesses to the real mod API):

```
hook(0x80166888):                          # fn_80166888(field), once per visit to state 0x28
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
| Where | `0x80166888`, every field check | `0x800FC528`, every event end | bytes of `Eve_rm01` and `Eve_rm02` |
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
repeat with a player of each gender. Point (1) was tried under hand-written
conditions (see the next section); the rest are still pending.

### Run-time test of the mod (2026-11-04)

`jamie_marriage` 0.1.0 was tried inside the recompiled game, but **without
playing the story**: the conditions of the wedding were written by hand into
the memory of a saved state. It shows what the hook does in that field check;
it does not show a real game from start to end (see "What is not verified").

**How it was done** (the test material is throwaway and is not in the
repository):

- An emulation state saved during the day change, in state `0x25`, right
  before the first field check (state `0x28`). Each test loads it with the
  game paused, writes the conditions (flag 230 set; events `0x10C`-`0x10E`
  consumed or not) and resumes. No video (`--graphics Null`); with two
  instances running at once on two cores the game ran at about 0.4 times its
  speed.
- The day change rewrites the field index (`0x80321D40`) and the check saw
  `27` instead of `26`: it started event `0x37`, not the wedding. A throwaway
  helper mod writes `26` at the **first** check only; every later value of the
  index is the game's own.
- Each case was run without the mod (control) and with it. The evidence is the
  game's log: `state_logger`, a throwaway trace of `StartEvent`, and the mod's
  own line.
- For the whole chain, a script presses A while an event runs. It starts with
  `Eve_rm00` (`0x10C`) already consumed: with the counters of that saved state
  it repeats, because its teardown (`fn_800FC528`) leaves it unconsumed again
  while `save + 0x4004 == save + 0x3FC8`.

**Results** (field index 26, flag 230 set):

| Case | Without the mod | With the mod |
|---|---|---|
| Wedding not started | `Eve_rm00` (`0x10C`) starts | Same; the flag stays set |
| Ceremony already started, first check | `Eve_EP06` (`0x115`) starts | Clears the flag (one line in the log) and `Eve_music00` (`0x10B`) starts, not the epilogue |
| Whole chain from the ceremony | `0x10D` → `0x115` → `0x116` → `0x117`; the teardown of `0x117` requests state 2 (call at `0x800FC738`), then `0x2A`, and the title starts; state 3 never appears | `0x10D` → `0x10B` → state 6 → state 0 (free play), where it stays with the flag clear |

In the control, `EP06`-`EP08` come straight away: the conditions of
`EP00`-`EP05` (counters of the save data) were not met in that saved state,
and they were not even requested.

**What was learned:**

1. *The hook acts on exactly the right pass.* The first check after the
   ceremony (state 4 → `0x28`) sees `0x10D` consumed and the flag set, clears
   it, and that same check skips the wedding branch. The counters the chain
   rewrites before `Eve_EP08` were left as they were: `save + 0x2258` stayed
   at 6 to the end (the chain leaves it at 12); the other two were already 0
   in that saved state, so they do not tell.
2. *The notes scene plays once.* With the flag clear, the handler of index 26
   falls into its "no flag" branch. If neither of the two events tied to the
   counter `save + 0x60` is due (`0x105` and `0x106`, with flags `0xBF`,
   `0xC0` and `0xC5`), its default is `StartEvent(0x10B)` (`Eve_music00`,
   where the player is told that five notes turn into an instrument). That
   scene plays once right after the ceremony.
3. *There is no loop in the game.* `StartEvent` never leaves `0x10B`
   consumed (see above), so if the index stayed at 26 the scene would repeat.
   It does not: when an event ends the game restores the index from the
   placement table (`0x8026C000`) entry, which for `0x10B` is place `129`,
   position `(10, 28)`, angle `270`. `fn_800A232C(129)` returns
   `fn_8002D148()`, a value the code itself keeps between 0 and 3
   (`0x8002D1AC`-`0x8002D1BC`), so never 26. The log of a throwaway probe
   confirms it: the index was 26 at the check that followed the ceremony (the
   game restored it, unforced) and 0 at the one that followed the notes scene.
   An earlier version of the test forced 26 at **every** check and the scene
   repeated forever (10 turns seen): that was an artefact of the helper, not
   of the game or the mod.
4. *The hook can run more than once per check* (at the first one, the
   `state_logger` log prints "field check entered" twice) and the mod prints
   a single line: the second pass already finds the flag clear, as the
   idempotent design in [`mods/README.md`](../mods/README.md) asks.
5. *Cost.* In 1,200 frames of free play at the house (no video), the
   runtime's `fallback` counter was 0 with and without the mod, and the other
   per-frame counters match: `native` 554,545 without the mod and 556,536
   with it, `hook_fb` 62,740 and 62,704. The block `[0x801644C0, 0x801684C0)`
   that the hook sends to the interpreter did not run in that stretch. It was
   only measured at the house and in free play.

### The conversations

What a villager says when you talk to them comes from `talk.lsb`, the only
script that uses native function 52 (`fn_800627F4`, 103 calls). That function
pops three integers (the character, where 125 is "whoever the player is
talking to" and `fn_800A3428` turns it into their number; the message number;
and one more value) and calls `fn_8008534C(window, character, message, ...)`,
which:

1. takes the path of the character's message file from the table at
   `0x801DFD40`, a word for each character number (number 2, Jamie, is
   `/message_e/chara/Rion_e.bin`);
2. loads the whole file into a new buffer with `fn_80029E74`;
3. gives up if the message number is above the highest one of the file;
4. finds the text through the file's table, passes it through `fn_80081E6C`
   (which puts the names in) and `fn_801A1C88` (a `vsprintf`: a `%` in the
   text would be a format) and shows it with `fn_8008253C`;
5. frees the buffer with `fn_801B9D7C`.

Nothing else calls `fn_8008534C`. A message file starts with the number of
the character who speaks and the highest message number it holds (16 bits
each), four more bytes, and where its table and its text start (32 bits
each). The table has a word for each message from 0 to the highest one: where
it starts, counted from the start of the text (`0xFF` is "no message"). In
the text, `01 04 1D` is a new row, `01 05 1D` a new page, `01 08 1F 1D` the
end of the message, `01 0A` the player's name, `01 3E` what the spouse calls
the player, `01 40` the child's name and `01 3D` the spouse's. The letters
are those of the console's font (Windows-1252), except the question mark,
which is `0x9D`; `0x85` is `…`.

**Each number is a situation**, the same for every character. On the first
conversation of the day (bit 0 of the character clear), `talk.lsb`
(subroutine `0x001A`) adds 5 to the friendship and tries, in this order:

| Message | Situation |
|---|---|
| 1 | The first time you talk to the character (bit 2 clear) |
| 70-85 | Festival days (by season and day) |
| 3, 4, 5 | Greeting by the hour: from 6 to 10, from 10 to 17, and the rest; another message follows |
| 8 | The day after a typhoon (yesterday's weather 3) |
| 60 | A stormy day (weather 3) |
| 87 | Your birthday, with friendship 300 or more |
| 9, list, 100 | The first conversation after the wedding (bit 3): asks what to call you, shows the list of names and answers with 100 |
| 10, 11 | The first conversation once the pregnancy is known (bit 4) and after the birth (bit 5) |
| 61-68 | At certain places (native function 114) |
| 12, 13 | A rainy day (weather 2), with friendship below 400 or from 400 on |
| 14-21 | By the friendship: below 100, 200, ..., 800 |
| 26-30 | Married (flag 0), friendship 800 or more, the spouse and anyone who is not a partner candidate: by the days of marriage (below 20, 50, 80, 110, or more) |
| 31-35 | The same when value 43 is 255: lines about children, by the child's age |
| 90 | Married, another partner candidate: "be happy with" the spouse |
| 22-25 | Not married, friendship 800 or more (22 and 23 up to 899, 24 and 25 from 900, by the player's gender) |

On another conversation of the same day it shows 7 if the character is the
spouse (bit 8) and 6 otherwise. Gifts (subroutine `0x0B2C`) use 36 to 51: 37
is a gift the character likes on their birthday (bit 6), 41 to 45 go from
"loves it" to "dislikes it", 38 is the proposal with the blue feather, and the
spouse says 50 on seeing you with the baby in your arms.

The values it reads: the friendship is the counter at `save + 0x6628 + 12·n`
(native function 122 reads it and 121 adds to it). The character bits are at
`save + 0x5564 + 3·n` (natives 123, 124 and 125: set, clear and test). Every
morning the game (`fn_80032EE8`, from `0x800334F8`) clears bits 0 and 1, sets
bit 6 for whoever has a birthday that day and bit 8 for the spouse
(`save + 0x22B8`). The birthday comes from a table of the game's data
(`*0x80321F50 + 0x329E0`, 4 bytes for each character: the season plus 2 in
the four high bits of the first byte and the day plus 2 in the second;
`fn_800A7D3C` and `fn_800A7D14`). From `GET_GAME_VALUE`: 1 is the player
(`save + 0x227C`: 0 for the boy, who marries characters 3 to 12,
`fn_800A2B60`), 3 and 4 the player's birthday, 38 the spouse, 124 the season
(`save + 0x2234`), 125 the day (`save + 0x224C`), 126 the hour
(`save + 0x2258`), 129 today's weather (`save + 0x1BC8`), 131 yesterday's,
154 the days of marriage (`save + 0xCB54`), 155 the child's age
(`save + 0xCB6C`), and 43 is `save + 0xCB60`.

Jamie (character 2) has no birthday in that table (season -1, seen in the
memory of the running game), and at the spouse's numbers Jamie's file holds a
rival's lines. The `jamie_marriage` mod 0.5.0 changes them when Jamie is the
spouse: it hooks the entry of `fn_8008534C` (chooses the line), the return of
`fn_80029E74` (writes it into the copy of the file that was just loaded and
points the table at it; for 100, which the file lacks, it also raises the
highest number) and the return of `fn_8008534C` (so that nothing stays
waiting). It chooses the language by the folder of the path: the disc has
the same files in `message_e`, `message_s`, `message_f`, `message_g` and
`message_i` (and in Japanese in `message`), with the same numbers (Jamie's
highest is 91 in all of them), although the American game only reads the
English ones: the 170 message paths of its program are in `message_e`, and
each has its twin in the other four European languages, except five that do
not exist in English either. `tests/test_jamie_as_the_spouse.py` checks
that every line fits the window and has only letters of the font, and runs
the hooks on a fake memory.

The birthday table comes from `system/Itemdata.bin` (from `0x329E0`), which
the game loads when it starts. Since version 0.6.0, while Jamie is the
spouse, the mod gives Jamie the 22nd of winter in that copy in memory: no
other character has a birthday on that day and no 22nd has a festival line in
`talk.lsb` (the winter festivals are on the 7th, the 12th and the 19th). When
Jamie is not the spouse, it gives back the game's "none". It does so on
entering every area (`fn_80042D10`), so the date is there when the game
checks the birthdays the next morning.

### What is not verified

- The test used hand-written conditions on a saved state, with index 26
  forced at the first check. A real game is still missing: the actual
  proposal (`talk.lsb` setting flag 230), saving and loading with the memory
  card, a second proposal and both genders of the player. The mod built for
  Windows ARM64 was not tried either.
- After the ceremony the notes scene plays once and, by its entry in the
  placement table, the player is left at place `129` (position `(10, 28)`);
  that was not observed. Option A+ could avoid the scene, but it was not
  tried.
- Area 26 was not identified. For many places the field index is the place
  number (in the saved state of the test, place 27 and index 27; the
  ceremony stores place 26 with native function 70), and others (for
  example `0x81`) go through `fn_8002D148` and depend on counters of the
  save data; which map 26 is, is not known. From the placement table (`-3`),
  the ceremony returns the player to where they were when the event started.
- It is not known whether state 9 advances a day, nor what exactly
  `save + 0xD454` is (see native function 70). Nor was it tried where the
  player wakes up the day after the wedding: after the ceremony the place
  variable (`0x80321D3C`) holds 26 (it held 27) and the day change copies it
  to `save + 0xD454`.
- The other state-writing built-ins (109-112, 133, 151, 153, 156-160) were not
  audited, nor what an ordinary wedding (`Eve_marry*`) changes in the save;
  copying that for Jamie would be the "complete" way of being married.
- What would start `EndingTask` (the staff roll, state 3): no static code
  requests that state, and in the test no credits appeared after `Eve_EP08`:
  the game went to state 2 and then to the title. It may be unused code or be
  reached by a path that static analysis does not see.
- The chain rewrites `save + 0x2258`, `save + 0x2234` and `save + 0x1BC8`
  before `Eve_EP08`. What they are is now known: the hour, the season and the
  weather of the day (`talk.lsb` reads them to choose the greeting of the
  hour, the festival lines and the rain lines; see "The conversations").
  `fn_8002D148` reads the season, the day and the hour, and returns a number
  from 0 to 3 that `fn_800A232C` uses as the field index of several places,
  `0x81` among them. Option A prevents the write (in the test `0x2258` did
  not change), but it is not known whether a game that carries on would need
  a different value.

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
