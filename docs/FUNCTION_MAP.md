# Function map / Mapa de funciones

[Español](#español) · [English](#english)

Regenerate and check / Regenerar y comprobar:

```sh
python3 tools/analyze.py                      # reads orig/G4AEE9/sys/main.dol, writes config/G4AEE9/symbols.txt
python3 tools/sdk_signatures.py --database totaldb.dsy   # SDK names by signature (--apply writes them)
HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest
python3 tools/disasm.py OSInit
```

---

## Español

### Para qué sirve

Un recompilador estático traduce una función a la vez, así que primero
necesita saber dónde empieza y termina cada una, cuáles saltos son tablas de
`switch` y qué partes de las secciones de código son en realidad datos. Este
mapa es esa base. Vive en `config/G4AEE9/symbols.txt`, con la sintaxis de
decomp-toolkit, así que también sirve si algún día se descompila. Con el
backend de C, DolRecomp detecta las funciones por su cuenta; nuestro mapa
llega a los mods como nombres (`tools/export_mod_symbols.py`) y al backend
LLVM como puntos de entrada (`tools/export_dolrecomp_map.py`).

### Resultado

| | |
|---|---|
| Funciones | **4 486** |
| …con tamaño exacto de `extabindex` | 2 597 |
| …destino de algún `bl` | 3 637 |
| Tablas de `switch` | 274 (4 sin comprobación de límite, ver abajo) |
| Datos dentro de código | 3 (`_rom_copy_info`, `_bss_init_info`, cadena de TRK) |
| Funciones con nombre | 588, todas con evidencia (25 por lectura del código, 563 por firma del SDK) |

### Cómo se sabe que es correcto

Cada punto es una prueba en `tests/test_function_map.py`:

1. **Coincide con la verdad conocida.** `extabindex` (el índice de
   excepciones de C++ de CodeWarrior) da inicio y tamaño exactos de 2 597
   funciones. Se mide cada una con el algoritmo como si no se supiera su
   tamaño: **2 597 de 2 597 coinciden**.
2. **Ninguna función se solapa con otra.**
3. **Ningún salto sale de su función.** Todo salto condicional cae dentro de
   su función, y todo `b` que sale es una llamada de cola al inicio de otra.
4. **Todo el código está cubierto.** Lo que no pertenece a una función es
   relleno (`0x00000000` o `nop`) o datos identificados.
5. **Ninguna tabla de `switch` se rechaza:** cada caso cae dentro de su
   función y, cuando hay comprobación de límite, la cantidad coincide.
6. **Las tablas de arranque listan exactamente las secciones del DOL**, en
   las dos direcciones.

Además, `tests/test_function_finder.py` y `tests/test_sdk_names.py`
reproducen cada caso difícil con DOLs sintéticos, sin datos del juego.

### Cómo funciona

Ver el encabezado de `tools/function_finder.py`. En resumen:

1. **Semillas:** `extabindex`, destinos de `bl`, punteros a código en datos
   (vtables, callbacks), direcciones de código armadas con `lis`/`addi`,
   entry point, `.ctors` y `.dtors`.
2. **Tablas de `switch`:** patrón `lis`/`addi` → `lwzx` → `mtctr` → `bctr`.
   Sus casos apuntan *dentro* de una función, así que nunca cuentan como
   inicio de función. La base de la tabla a veces se carga una sola vez,
   antes de un bucle o en el prólogo, así que se recuerda hasta el inicio de
   la siguiente función (y a través de llamadas si está en un registro
   preservado, `r14`–`r31`).
3. **Límite de cada tabla:** el `cmplwi` que la protege, pero solo si
   compara el registro que termina siendo el índice (siguiendo el
   `slwi rX, rN, 2`). Se busca hasta 64 instrucciones atrás.
4. **Extensión:** desde cada inicio se avanza recordando el salto local más
   lejano. La función termina en el primer `blr`/`b`/`bctr`/`rfi` que esté
   más allá de todos.
5. **Huecos:** código sin dueño inicia una función nueva; se repite hasta
   que nada cambie.
6. **Datos dentro de código:** cadenas ASCII y las tablas de arranque se
   marcan como objetos, no como funciones.

### Las 4 tablas sin comprobación de límite

Se leen mientras sus casos caigan dentro de la función y hasta donde empieza
la siguiente tabla. No se descartan, porque entonces sus casos se tomarían
por inicios de función.

| `bctr` | Casos | Por qué no hay límite |
|---|---|---|
| `0x80088200` | 10 | La función no tiene ningún `cmplwi` del índice: viene de datos. |
| `0x800883C4` | 10 | Igual. |
| `0x800B5BE4` | 12 | Igual. |
| `0x8008C0C0` | 7 | El `cmplwi r0, 6` está en un bloque anterior al que se llega con `bne`; la búsqueda lineal se detiene en el `b` de en medio. Su valor (7) coincide con lo leído. |

### Errores corregidos por el camino

Para no repetirlos:

- Casos de `switch` tomados como funciones cuando la base de la tabla
  estaba en `r31` (cargado en el prólogo) o en un registro volátil cargado
  antes de un bucle (la función de GX en `0x80190FA8`).
- Tablas rechazadas porque sus propios casos, tomados como funciones,
  "cortaban" la función; por eso los límites de las tablas se calculan solo
  con inicios seguros.
- Un `cmplwi` de otro registro tomado como límite de la tabla.
- `lmw rD` escribe de `rD` a `r31`, no solo `rD`.
- `psq_l`/`psq_st` tienen 12 bits de desplazamiento, no 16.

### Secciones

`_rom_copy_info` y `_bss_init_info` dan los tamaños exactos; la cabecera del
DOL los redondea a 32 bytes.

| Sección | Dirección | Tamaño real | En el DOL |
|---|---|---|---|
| `.init` | `0x80003100` | `0x24E0` | `0x24E0` |
| `extab` | `0x800055E0` | `0xB500` | `0xB500` |
| `extabindex` | `0x80010AE0` | `0x79DC` | `0x79E0` |
| `.text` | `0x800184C0` | `0x1B650C` | `0x1B6520` |
| `.ctors` | `0x801CE9E0` | `0x60` | `0x60` |
| `.dtors` | `0x801CEA40` | `0xC` | `0x20` |
| `.rodata` | `0x801CEA60` | `0x20848` | `0x20860` |
| `.data` | `0x801EF2C0` | `0xBFA78` | `0xBFA80` |
| `.bss` | `0x802AED40` | `0x7232C` | — |
| `.sdata` | `0x80321080` | `0xC10` | `0xC20` |
| `.sbss` | `0x80321CA0` | `0xAC4` | — |
| `.sdata2` | `0x80322780` | `0x3C6C` | `0x3C80` |

Bases de small data, leídas de `__init_registers`: `r13 = 0x80329080`
(`.sdata` + 0x8000) y `r2 = 0x8032A780` (`.sdata2` + 0x8000). La pila
empieza en `r1 = 0x803363F0`.

### Nombres y su evidencia

**Arranque de CodeWarrior** (`__start.c`), comprobado leyendo el código:

| Nombre | Dirección | Evidencia |
|---|---|---|
| `__start` | `0x80003154` | Entry point del DOL. |
| `__init_registers` | `0x800032B0` | Primer `bl` de `__start`; carga `r1`, `r2` y `r13` con `lis`/`ori`. |
| `__init_hardware` | `0x80003400` | Activa el bit FP del MSR (`ori r0, r0, 0x2000`) y llama tres inits del SDK. |
| `__init_data` | `0x80003340` | Recorre `_rom_copy_info` y `_bss_init_info`. |
| `__check_pad3` | `0x80003100` | Lee `Pad3Button` (`0x800030E4`), compara con `0xEEF` y llama a `OSResetSystem(0, 0, FALSE)`. |
| `__set_debug_bba` | `0x80003140` | Guarda 1 en un flag; `__start` lo llama cuando el modo de depuración lo pide. |
| `__get_debug_bba` | `0x8000314C` | Devuelve ese flag; `__start` decide con él si inicia TRK por BBA. |
| `OSResetSystem` | `0x801736C0` | La función que llama `__check_pad3`. |
| `main` | `0x801B4A98` | `__start` la llama con `r14`/`r15` (argc/argv) justo antes de saltar a `exit`. |

**SDK de Dolphin**, por la regla de `tools/sdk_names.py`: cada biblioteca
tiene un global `__<LIB>Version` que apunta a su cadena
`<< Dolphin SDK - <LIB> release build ... >>`, y una sola función lo lee
para pasárselo a `OSRegisterVersion`. Las 13 bibliotecas llaman a la misma
función (`0x8016F840`), cuyo cuerpo es `OSReport("%s\n", id)`.

| Biblioteca | Init | Global |
|---|---|---|
| OS | `OSInit` `0x8016EF2C` (también lo llama `__start`) | `__OSVersion` |
| DVD | `DVDInit` `0x80176F74` | `__DVDVersion` |
| VI | `VIInit` `0x8017A354` | `__VIVersion` |
| PAD | `PADInit` `0x8017C890` | `__PADVersion` |
| AI | `AIInit` `0x8017DB38` | `__AIVersion` |
| AR | `ARInit` `0x8017E1E0` | `__ARVersion` |
| ARQ | `ARQInit` `0x8017FD10` | `__ARQVersion` |
| AX | `AXInit` `0x8017FEDC` → `AXInitEx` `0x8017FF00` | `__AXVersion` |
| DSP | `DSPInit` `0x80187828` (también lo llama `CARDInit`) | `__DSPVersion` |
| CARD | `CARDInit` `0x80189398` | `__CARDVersion` |
| GX | `GXInit` `0x8018F234` | `__GXVersion` |
| EXI | `EXIInit` `0x801A96D4` (lo llama `OSInit`) | `__EXIVersion` |
| SI | `SIInit` `0x801AAD14` (lo llama `OSInit`) | `__SIVersion` |

Más `OSRegisterVersion` (`0x8016F840`) y `OSReport` (`0x80171504`).

### Nombres del SDK por firma

Casi todas las funciones del SDK no tienen `Init` ni cadena de versión, así
que la regla anterior no las alcanza. Dolphin trae una base con las firmas de
10 549 funciones de SDKs de Nintendo, y `tools/sdk_signatures.py` busca en
ella cada función de `main.dol`:

```sh
python3 tools/sdk_signatures.py --database totaldb.dsy           # informe; no cambia nada
python3 tools/sdk_signatures.py --database totaldb.dsy --list    # y una línea por función
python3 tools/sdk_signatures.py --database totaldb.dsy --apply   # escribe los nombres en symbols.txt
```

**La firma.** Es la suma de verificación de Dolphin
(`HashSignatureDB::ComputeCodeChecksum`). De cada instrucción guarda el opcode
y, según su tipo, los registros o el opcode extendido, pero nunca los valores
inmediatos ni los destinos de los saltos; por eso la misma función da la misma
firma en cualquier dirección. Las pruebas usan valores calculados con una copia
literal del bucle de Dolphin, compilada aparte. Antes de usar el port se
comparó con esa copia sobre 20 768 secuencias de instrucciones (20 000 al azar
y 768 hechas para cubrir cada opcode con sus patrones de bits) y no hubo
ninguna diferencia.

**La base.** Es `Data/Sys/totaldb.dsy` de Dolphin, commit `10f7ac4f`
(2026-09-16): 1 434 668 bytes y sha256
`09fb4a167946dbdfee5890c0f027e40b3eacbb6cc5ee9184adeb9d1a81c2c0aa`. No está en
este repositorio, porque son datos de Dolphin con licencia GPL-2.0-or-later; la
herramienta la recibe con `--database` o con la variable `HM_MM_SIGNATURE_DB`.
Las cifras de abajo son las de esa versión.

**En qué se confía.** Una firma igual es una evidencia fuerte y no una prueba,
así que la herramienta no nombra todo lo que encuentra: clasifica cada
coincidencia. De las 4 486 funciones, 1 383 tienen una firma que la base conoce:

| Veredicto | Funciones | Qué quiere decir |
|---|---|---|
| Fuerte | 443 | Única en el programa, del mismo tamaño que en la base, con nombre de C normal y de al menos `0x60` bytes (24 instrucciones). Se nombra. |
| Corroborada | 133 | Igual, pero más corta. Su biblioteca (el prefijo `GX`, `OS`, `DVD`...) es la de la coincidencia fuerte que tiene justo antes o justo después. Se nombra. |
| Firma compartida | 698 | Varias funciones del programa tienen esa firma, así que no dice cuál es cuál. No se nombra ninguna. |
| Corta sin respaldo | 63 | Única y del mismo tamaño, pero corta y sin una coincidencia fuerte de su biblioteca al lado. No se nombra. |
| Nombre inutilizable | 32 | El nombre no es un identificador de C (un método de C++, por ejemplo). No se nombra. |
| Nombre repetido | 9 | Dos funciones distintas del programa recibirían el mismo nombre (funciones `static` de archivos distintos, como `OnReset`). No se nombra ninguna. |
| Otro tamaño | 5 | La base conoce otro tamaño para esa firma. No se nombra. |

**De dónde sale `0x60`.** Se midió con el código del propio juego, que no es
del SDK y no debería coincidir con nada. De sus 787 funciones de menos de
`0x60` bytes, 20 coincidieron por casualidad (la más larga, de `0x58`); de las
1 856 de `0x60` o más, ninguna. Por eso una coincidencia corta solo vale con el
respaldo de la biblioteca vecina, y en el código del juego
(`0x800184C0`–`0x8016EB94`) no quedó ninguna función con nombre.

**Contra qué se comprobó.** Las 13 funciones que ya tenían nombre por la regla
de las versiones o por lectura del código y que la base conoce (`OSReport`,
`OSRegisterVersion`, `VIInit`, `PADInit`, `AIInit`, `ARInit`, `ARQInit`,
`DSPInit`, `CARDInit`, `EXIInit`, `SIInit`, `__init_registers` y
`__init_data`) reciben el mismo nombre por firma, y ninguna recibe otro. Otras
dos coinciden con lo que se había deducido por separado: `GXCallDisplayList`
(`0x80195010`), la única función que manda a la tubería el comando 0x40 de GX,
y `__OSFPRInit` (`0x8016EDA0`), la que inicializa los registros de punto
flotante al arrancar. Y las 77 funciones de la librería GX que escriben en la
tubería (`tools/find_gx_pipe_writers.py`, halladas sin mirar los nombres) caen
todas dentro del rango GX; las otras ocho son las funciones de vértices del
juego, en `0x801CAB68`.

**Dónde está cada biblioteca.** El enlazador deja juntas las funciones de una
biblioteca, así que una cadena de nombres con el mismo prefijo marca su rango.
Cada rango va del inicio de su primera función con nombre al final de la
última. Entre dos rangos hay a veces una o dos funciones sin nombre, de una de
las dos vecinas.

| Biblioteca | Rango | Funciones | Con nombre |
|---|---|---|---|
| OS (primera parte) | `0x8016EDA0`–`0x80175778` | 146 | 93 |
| MTX | `0x8017580C`–`0x801759C8` | 4 | 4 |
| DVD | `0x801759C8`–`0x80179BB0` | 85 | 49 |
| VI | `0x80179DF0`–`0x8017BBB0` | 20 | 13 |
| PAD | `0x8017BF98`–`0x8017D73C` | 18 | 15 |
| AI | `0x8017D780`–`0x8017E008` | 17 | 11 |
| AR | `0x8017E04C`–`0x8017FB40` | 8 | 7 |
| ARQ | `0x8017FB40`–`0x8017FEDC` | 5 | 4 |
| AX y AXFX | `0x8017FEDC`–`0x801877FC` | 87 | 4 |
| DSP | `0x801877FC`–`0x80188300` | 12 | 9 |
| CARD | `0x80188338`–`0x8018ED98` | 87 | 60 |
| GX | `0x8018EDE0`–`0x80195F98` | 156 | 102 |
| TRK | `0x80195F98`–`0x8019B5F0` | 120 | 76 |
| Resto de TRK, biblioteca C y soporte del compilador | `0x8019B5F0`–`0x801A7A14` | 188 | 72 |
| DB | `0x801A7A14`–`0x801A848C` | 12 | 9 |
| EXI | `0x801A8580`–`0x801A9E68` | 22 | 21 |
| OS (segunda parte) | `0x801A9FF4`–`0x801AA1B0` | 1 | 1 |
| SI | `0x801AA420`–`0x801ABC18` | 24 | 23 |

La fila de AX y AXFX es la zona entre `AXInit` y DSP. Casi todas sus firmas
faltan en la base, pero las pocas que hay la identifican: `do_src1` y
`do_src2` (AX) tienen nombre, y `AXFXSetHooks` y `HandleReverb` (AXFX) se
reconocen pero se comparten entre varias funciones, así que no se nombran. La
fila «Resto de TRK, biblioteca C y soporte del compilador» tiene, con nombre,
`sprintf`, `qsort`, `memmove`, las funciones matemáticas (`__ieee754_pow`,
`sin`, `tan`...), `__div2u`, `__construct_array` y, de TRK,
`CircleBufferReadBytes`.

La región del SDK (`0x8016EB94`–`0x801ABC18`) tiene 1 057 funciones y 579 con
nombre, el 55 %. Fuera de ella el código del juego y su middleware queda sin
nombre: 2 643 funciones antes y 746 después de SI (solo `main`). Las 478
funciones del SDK que siguen sin nombre se reparten así: 212 comparten firma con
otras, 200 no tienen firma en la base, 51 son cortas sin respaldo, 9 repiten
nombre, 4 tienen otro tamaño y 2 tienen nombre de C++. AX es el caso más claro
de las que no están en la base: 72 de sus 87 funciones.

**Lo que no se usa todavía.** La base trae, en la mitad de sus entradas (5 133
de 10 549), el archivo y el objeto del enlazador (`card.a CARDBios.o`, por
ejemplo). Daría la biblioteca exacta de cada función, también en la biblioteca C
y en TRK, y no solo la que dice el prefijo del nombre.

### Archivos fuente del juego

Los asserts imprimen el nombre del archivo fuente. CodeWarrior enlaza los
objetos en orden, así que esto marca dónde vive cada archivo en `.text`:

```
Building.cpp      0x800386EC   lsrun.cpp         0x80051784   LiveStockDb.cpp   0x800D1820
Field.cpp         0x8003D030   lsrun_bltin.cpp   0x80057908   FurnitureArrange  0x800D3C80
CastControl.cpp   0x80046F7C   Cell.cpp          0x8006A7A0   UnitArrange.cpp   0x800F8378
LiveStock.cpp     0x8008E1C0   Sound.cpp         0x80098540   KusaKeiba.cpp     0x80142A5C
Parameter.cpp     0x800A218C   shipment.cpp      0x800A86B0   CDColiseum.cpp    0x80146570
WinRadar.cpp      0x800C1E20
SDK: dvdfs.c 0x8017687C · dvd.c 0x8017704C · vi.c 0x8017ACCC · gcaxSndMemory.c 0x801C3B8C · texPalette.c 0x801CC3B4
```

El código del juego (y su middleware) ocupa `0x800184C0`–`0x8016EB94`. Ahí
empieza el SDK con `PPCArch` de `base.a`: funciones de dos instrucciones que
leen o escriben un registro especial (`mfmsr r3; blr`, `mtspr HID0`, `L2CR`,
`DEC`, `MMCR0`…), seguidas de DB y OS (`OSInit` en `0x8016EF2C`).

### Bucle principal

`main` llama a `fn_801B482C(0x10)` una vez y luego repite para siempre
`fn_801B4560(1)` y pone en 1 un flag en `0x80322624` (`.sbss`).
`fn_801B4560` es el frame del juego.

### Límites conocidos

- Los manejadores de excepción de TRK (`0x800035AC`–`0x800053AC`, cada
  `0x100`) aparecen como funciones sueltas. Son código que TRK copia a
  memoria baja; el juego nunca los llama.
- `0x8016F77C` y `0x80174788` son etiquetas de fin de bloques que el SDK
  copia a memoria baja (un `nop` cada una).
- Un nombre por firma es una evidencia fuerte, no una prueba. El riesgo que
  queda es una función del SDK de este juego con la misma forma que otra que
  el juego no incluye: recibiría el nombre de la otra. El mismo tamaño y que la
  biblioteca encaje con las vecinas lo hacen poco probable, no imposible.
  Antes de reemplazar una función hay que mirarla (`tools/disasm.py`,
  `tools/find_callers.py`).
- 478 de las 1 057 funciones de la región del SDK siguen sin nombre (ver
  arriba); AX y AXFX son el hueco más grande.
- Las funciones de `PPCArch` (desde `0x8016EB94`) son de dos instrucciones que
  leen o escriben un registro especial. La firma no guarda el número del
  registro, así que no las distingue y quedan sin nombre. Haría falta una
  regla aparte que lea ese número.
- Los nombres ya escritos están en `symbols.txt` y `tools/analyze.py` los
  conserva al regenerar; la base de Dolphin solo hace falta para repetir o
  ampliar la búsqueda.

---

## English

### What it is for

A static recompiler translates one function at a time, so it first needs to
know where each function starts and ends, which jumps are `switch` tables,
and which parts of the code sections are really data. This map is that
foundation. It lives in `config/G4AEE9/symbols.txt`, in decomp-toolkit
syntax, so it is also usable if the game is ever decompiled. With the C
backend DolRecomp finds functions on its own; our map reaches mods as names
(`tools/export_mod_symbols.py`) and the LLVM backend as entry points
(`tools/export_dolrecomp_map.py`).

### Result

| | |
|---|---|
| Functions | **4,486** |
| …with exact size from `extabindex` | 2,597 |
| …target of some `bl` | 3,637 |
| `switch` tables | 274 (4 without a bounds check, see below) |
| Data inside code | 3 (`_rom_copy_info`, `_bss_init_info`, TRK string) |
| Named functions | 588, all with evidence (25 from reading the code, 563 from SDK signatures) |

### How we know it is right

Each point is a test in `tests/test_function_map.py`:

1. **It matches known truth.** `extabindex` (CodeWarrior's C++ exception
   index) gives the exact start and size of 2,597 functions. Each one is
   measured with the algorithm as if its size were unknown: **2,597 of 2,597
   agree**.
2. **No function overlaps another.**
3. **No branch leaves its function.** Every conditional branch stays inside,
   and every `b` that leaves is a tail call to another function's start.
4. **All code is covered.** Whatever belongs to no function is padding
   (`0x00000000` or `nop`) or identified data.
5. **No `switch` table is rejected:** every case lands inside its function
   and, where there is a bounds check, the count matches it.
6. **The startup tables list exactly the DOL's sections**, both ways.

`tests/test_function_finder.py` and `tests/test_sdk_names.py` also
reproduce every hard case with synthetic DOLs, no game data needed.

### How it works

See the header of `tools/function_finder.py`. In short:

1. **Seeds:** `extabindex`, `bl` targets, code pointers in data (vtables,
   callbacks), code addresses built with `lis`/`addi`, the entry point,
   `.ctors` and `.dtors`.
2. **`switch` tables:** the `lis`/`addi` → `lwzx` → `mtctr` → `bctr`
   pattern. Their cases point *inside* a function, so they never count as
   function starts. The table base is sometimes loaded once, before a loop or
   in the prologue, so it is remembered until the next function start (and
   across calls when it sits in a callee-saved register, `r14`–`r31`).
3. **Table bounds:** the guarding `cmplwi`, but only when it compares the
   register that becomes the index (following `slwi rX, rN, 2`). The search
   goes up to 64 instructions back.
4. **Extent:** walk forward from each start remembering the furthest local
   branch target. The function ends at the first `blr`/`b`/`bctr`/`rfi` past
   all of them.
5. **Gaps:** unowned code starts a new function; repeat until nothing
   changes.
6. **Data inside code:** ASCII strings and the startup tables become
   objects, not functions.

### The 4 tables without a bounds check

They are read while their cases stay inside the function and up to where the
next table starts. They are not dropped, because their cases would then be
taken for function starts.

| `bctr` | Cases | Why there is no bounds check |
|---|---|---|
| `0x80088200` | 10 | The function never compares the index: it comes from data. |
| `0x800883C4` | 10 | Same. |
| `0x800B5BE4` | 12 | Same. |
| `0x8008C0C0` | 7 | The `cmplwi r0, 6` sits in an earlier block reached through `bne`; the linear search stops at the `b` in between. Its value (7) matches what was read. |

### Mistakes fixed along the way

So they are not repeated:

- `switch` cases taken as functions when the table base sat in `r31`
  (loaded in the prologue) or in a volatile register loaded before a loop
  (the GX function at `0x80190FA8`).
- Tables rejected because their own cases, taken as functions, "cut" the
  function short; table bounds therefore use certain starts only.
- A `cmplwi` on another register taken as the table's bounds check.
- `lmw rD` writes `rD` through `r31`, not just `rD`.
- `psq_l`/`psq_st` have a 12-bit displacement, not 16.

### Sections

`_rom_copy_info` and `_bss_init_info` give the exact sizes; the DOL header
rounds them up to 32 bytes.

| Section | Address | Real size | In the DOL |
|---|---|---|---|
| `.init` | `0x80003100` | `0x24E0` | `0x24E0` |
| `extab` | `0x800055E0` | `0xB500` | `0xB500` |
| `extabindex` | `0x80010AE0` | `0x79DC` | `0x79E0` |
| `.text` | `0x800184C0` | `0x1B650C` | `0x1B6520` |
| `.ctors` | `0x801CE9E0` | `0x60` | `0x60` |
| `.dtors` | `0x801CEA40` | `0xC` | `0x20` |
| `.rodata` | `0x801CEA60` | `0x20848` | `0x20860` |
| `.data` | `0x801EF2C0` | `0xBFA78` | `0xBFA80` |
| `.bss` | `0x802AED40` | `0x7232C` | — |
| `.sdata` | `0x80321080` | `0xC10` | `0xC20` |
| `.sbss` | `0x80321CA0` | `0xAC4` | — |
| `.sdata2` | `0x80322780` | `0x3C6C` | `0x3C80` |

Small-data bases read from `__init_registers`: `r13 = 0x80329080`
(`.sdata` + 0x8000) and `r2 = 0x8032A780` (`.sdata2` + 0x8000). The stack
starts at `r1 = 0x803363F0`.

### Names and their evidence

**CodeWarrior startup** (`__start.c`), checked by reading the code:

| Name | Address | Evidence |
|---|---|---|
| `__start` | `0x80003154` | The DOL entry point. |
| `__init_registers` | `0x800032B0` | First `bl` in `__start`; loads `r1`, `r2` and `r13` with `lis`/`ori`. |
| `__init_hardware` | `0x80003400` | Sets the MSR FP bit (`ori r0, r0, 0x2000`) and calls three SDK inits. |
| `__init_data` | `0x80003340` | Walks `_rom_copy_info` and `_bss_init_info`. |
| `__check_pad3` | `0x80003100` | Reads `Pad3Button` (`0x800030E4`), compares with `0xEEF` and calls `OSResetSystem(0, 0, FALSE)`. |
| `__set_debug_bba` | `0x80003140` | Stores 1 to a flag; `__start` calls it when the debug mode asks for it. |
| `__get_debug_bba` | `0x8000314C` | Returns that flag; `__start` uses it to decide whether to start TRK over BBA. |
| `OSResetSystem` | `0x801736C0` | The function `__check_pad3` calls. |
| `main` | `0x801B4A98` | `__start` calls it with `r14`/`r15` (argc/argv) right before jumping to `exit`. |

**Dolphin SDK**, by the rule in `tools/sdk_names.py`: each library has a
`__<LIB>Version` global pointing at its
`<< Dolphin SDK - <LIB> release build ... >>` string, and exactly one
function reads it to pass it to `OSRegisterVersion`. All 13 libraries call
the same function (`0x8016F840`), whose body is `OSReport("%s\n", id)`.

| Library | Init | Global |
|---|---|---|
| OS | `OSInit` `0x8016EF2C` (`__start` calls it too) | `__OSVersion` |
| DVD | `DVDInit` `0x80176F74` | `__DVDVersion` |
| VI | `VIInit` `0x8017A354` | `__VIVersion` |
| PAD | `PADInit` `0x8017C890` | `__PADVersion` |
| AI | `AIInit` `0x8017DB38` | `__AIVersion` |
| AR | `ARInit` `0x8017E1E0` | `__ARVersion` |
| ARQ | `ARQInit` `0x8017FD10` | `__ARQVersion` |
| AX | `AXInit` `0x8017FEDC` → `AXInitEx` `0x8017FF00` | `__AXVersion` |
| DSP | `DSPInit` `0x80187828` (`CARDInit` calls it too) | `__DSPVersion` |
| CARD | `CARDInit` `0x80189398` | `__CARDVersion` |
| GX | `GXInit` `0x8018F234` | `__GXVersion` |
| EXI | `EXIInit` `0x801A96D4` (called by `OSInit`) | `__EXIVersion` |
| SI | `SIInit` `0x801AAD14` (called by `OSInit`) | `__SIVersion` |

Plus `OSRegisterVersion` (`0x8016F840`) and `OSReport` (`0x80171504`).

### SDK names by signature

Most SDK functions have no `Init` and no version string, so the rule above
does not reach them. Dolphin ships a database with the signatures of 10,549
functions of Nintendo SDKs, and `tools/sdk_signatures.py` looks up every
function of `main.dol` in it:

```sh
python3 tools/sdk_signatures.py --database totaldb.dsy           # report; changes nothing
python3 tools/sdk_signatures.py --database totaldb.dsy --list    # and one line per function
python3 tools/sdk_signatures.py --database totaldb.dsy --apply   # writes the names to symbols.txt
```

**The signature.** It is Dolphin's checksum
(`HashSignatureDB::ComputeCodeChecksum`). From each instruction it keeps the
opcode and, depending on its kind, the registers or the extended opcode, but
never immediate values or branch targets; that is why the same function has
the same signature at any address. The tests use values computed with a
verbatim copy of Dolphin's loop, compiled separately. Before the port was
used it was compared with that copy on 20,768 sequences of instructions
(20,000 random ones and 768 built to cover every opcode with its bit
patterns) and there was no difference.

**The database.** It is Dolphin's `Data/Sys/totaldb.dsy`, commit `10f7ac4f`
(2026-09-16): 1,434,668 bytes and sha256
`09fb4a167946dbdfee5890c0f027e40b3eacbb6cc5ee9184adeb9d1a81c2c0aa`. It is not
in this repository, because it is Dolphin's data under GPL-2.0-or-later; the
tool takes it with `--database` or the `HM_MM_SIGNATURE_DB` variable. The
figures below are those of that version.

**What is trusted.** An equal signature is strong evidence and not a proof, so
the tool does not name everything it finds: it sorts each match. Of the 4,486
functions, 1,383 have a signature the database knows:

| Verdict | Functions | What it means |
|---|---|---|
| Strong | 443 | Unique in the program, the same size as in the database, with a plain C name and at least `0x60` bytes (24 instructions). Named. |
| Corroborated | 133 | The same, but shorter. Its library (the `GX`, `OS`, `DVD`... prefix) is that of the strong match right before or right after it. Named. |
| Shared signature | 698 | Several functions of the program have this signature, so it does not say which is which. None is named. |
| Short, no backing | 63 | Unique and the same size, but short and with no strong match of its library next to it. Not named. |
| Unusable name | 32 | The name is not a C identifier (a C++ method, for example). Not named. |
| Repeated name | 9 | Two different functions of the program would get the same name (`static` functions of different files, such as `OnReset`). Neither is named. |
| Other size | 5 | The database knows another size for this signature. Not named. |

**Where `0x60` comes from.** It was measured on the game's own code, which is
not SDK and should match nothing. Of its 787 functions shorter than `0x60`
bytes, 20 matched by accident (the longest `0x58` bytes); of the 1,856 of
`0x60` or more, none. That is why a short match counts only with the backing
of the neighbouring library, and why no function of the game's code
(`0x800184C0`–`0x8016EB94`) was named.

**What it was checked against.** The 13 functions that already had a name from
the version rule or from reading the code and that the database knows
(`OSReport`, `OSRegisterVersion`, `VIInit`, `PADInit`, `AIInit`, `ARInit`,
`ARQInit`, `DSPInit`, `CARDInit`, `EXIInit`, `SIInit`, `__init_registers` and
`__init_data`) get the same name by signature, and none gets another. Two more
agree with what had been deduced separately: `GXCallDisplayList`
(`0x80195010`), the only function that sends GX command 0x40 to the pipe, and
`__OSFPRInit` (`0x8016EDA0`), the one that initialises the floating-point
registers at start-up. And the 77 functions of the GX library that write to
the pipe (`tools/find_gx_pipe_writers.py`, found without looking at names) all
fall inside the GX range; the other eight are the game's vertex functions, at
`0x801CAB68`.

**Where each library is.** The linker keeps the functions of a library
together, so a run of names with the same prefix marks its range. Each range
goes from the start of its first named function to the end of its last one.
Between two ranges there are sometimes one or two unnamed functions, which
belong to one of the two neighbours.

| Library | Range | Functions | Named |
|---|---|---|---|
| OS (first part) | `0x8016EDA0`–`0x80175778` | 146 | 93 |
| MTX | `0x8017580C`–`0x801759C8` | 4 | 4 |
| DVD | `0x801759C8`–`0x80179BB0` | 85 | 49 |
| VI | `0x80179DF0`–`0x8017BBB0` | 20 | 13 |
| PAD | `0x8017BF98`–`0x8017D73C` | 18 | 15 |
| AI | `0x8017D780`–`0x8017E008` | 17 | 11 |
| AR | `0x8017E04C`–`0x8017FB40` | 8 | 7 |
| ARQ | `0x8017FB40`–`0x8017FEDC` | 5 | 4 |
| AX and AXFX | `0x8017FEDC`–`0x801877FC` | 87 | 4 |
| DSP | `0x801877FC`–`0x80188300` | 12 | 9 |
| CARD | `0x80188338`–`0x8018ED98` | 87 | 60 |
| GX | `0x8018EDE0`–`0x80195F98` | 156 | 102 |
| TRK | `0x80195F98`–`0x8019B5F0` | 120 | 76 |
| Rest of TRK, C library and compiler support | `0x8019B5F0`–`0x801A7A14` | 188 | 72 |
| DB | `0x801A7A14`–`0x801A848C` | 12 | 9 |
| EXI | `0x801A8580`–`0x801A9E68` | 22 | 21 |
| OS (second part) | `0x801A9FF4`–`0x801AA1B0` | 1 | 1 |
| SI | `0x801AA420`–`0x801ABC18` | 24 | 23 |

The AX and AXFX row is the zone between `AXInit` and DSP. Most of its
signatures are missing from the database, but the few there are identify it:
`do_src1` and `do_src2` (AX) are named, and `AXFXSetHooks` and `HandleReverb`
(AXFX) are recognised but shared among several functions, so they are not
named. The row "Rest of TRK, C library and compiler support" has, named,
`sprintf`, `qsort`, `memmove`, the math functions (`__ieee754_pow`, `sin`,
`tan`...), `__div2u`, `__construct_array` and, from TRK,
`CircleBufferReadBytes`.

The SDK region (`0x8016EB94`–`0x801ABC18`) has 1,057 functions and 579 named,
55 %. Outside it the game's code and its middleware have no names: 2,643
functions before it and 746 after SI (only `main`). The 478 SDK functions that
are still unnamed split like this: 212 share a signature with others, 200
have no signature in the database, 51 are short with no backing, 9 repeat a
name, 4 have another size and 2 have a C++ name. AX is the clearest case of
those missing from the database: 72 of its 87 functions.

**What is not used yet.** In half of its entries (5,133 of 10,549) the
database carries the linker's archive and object (`card.a CARDBios.o`, for
example). It would give the exact library of each function, in the C library
and in TRK too, and not only the one the name prefix says.

### Game source files

Asserts print the source file name. CodeWarrior links objects in order, so
this marks where each file lives in `.text`:

```
Building.cpp      0x800386EC   lsrun.cpp         0x80051784   LiveStockDb.cpp   0x800D1820
Field.cpp         0x8003D030   lsrun_bltin.cpp   0x80057908   FurnitureArrange  0x800D3C80
CastControl.cpp   0x80046F7C   Cell.cpp          0x8006A7A0   UnitArrange.cpp   0x800F8378
LiveStock.cpp     0x8008E1C0   Sound.cpp         0x80098540   KusaKeiba.cpp     0x80142A5C
Parameter.cpp     0x800A218C   shipment.cpp      0x800A86B0   CDColiseum.cpp    0x80146570
WinRadar.cpp      0x800C1E20
SDK: dvdfs.c 0x8017687C · dvd.c 0x8017704C · vi.c 0x8017ACCC · gcaxSndMemory.c 0x801C3B8C · texPalette.c 0x801CC3B4
```

Game code (and its middleware) spans `0x800184C0`–`0x8016EB94`. The SDK
starts there with `base.a`'s `PPCArch`: two-instruction special-register
accessors (`mfmsr r3; blr`, `mtspr HID0`, `L2CR`, `DEC`, `MMCR0`, ...),
followed by DB and OS (`OSInit` at `0x8016EF2C`).

### Main loop

`main` calls `fn_801B482C(0x10)` once, then loops forever over
`fn_801B4560(1)` and sets a flag at `0x80322624` (`.sbss`). `fn_801B4560` is
the per-frame function.

### Known limits

- TRK's exception handlers (`0x800035AC`–`0x800053AC`, every `0x100`) show
  up as loose functions. TRK copies them to low memory; the game never
  calls them.
- `0x8016F77C` and `0x80174788` are end labels of blocks the SDK copies to
  low memory (one `nop` each).
- A name by signature is strong evidence, not a proof. The risk that remains
  is an SDK function of this game with the same shape as another one the game
  does not include: it would get the other one's name. The same size and a
  library that fits its neighbours make that unlikely, not impossible. Before
  replacing a function, look at it (`tools/disasm.py`,
  `tools/find_callers.py`).
- 478 of the 1,057 functions of the SDK region are still unnamed (see
  above); AX and AXFX are the biggest gap.
- The `PPCArch` functions (from `0x8016EB94`) are two instructions that read
  or write a special register. The signature does not keep the register
  number, so it cannot tell them apart and they stay unnamed. A separate rule
  that reads that number would be needed.
- The names already written are in `symbols.txt` and `tools/analyze.py` keeps
  them when it regenerates; Dolphin's database is only needed to repeat or
  widen the search.
