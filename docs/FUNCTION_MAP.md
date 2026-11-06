# Function map / Mapa de funciones

[Español](#español) · [English](#english)

Regenerate and check / Regenerar y comprobar:

```sh
python3 tools/analyze.py                      # reads orig/G4AEE9/sys/main.dol, writes config/G4AEE9/symbols.txt
python3 tools/sdk_signatures.py --database totaldb.dsy   # SDK names by signature (--apply writes them)
python3 tools/find_library_entry_points.py GX            # functions of an SDK library that the game reaches
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
| Funciones con nombre | 682, todas con evidencia (119 por lectura del código, 563 por firma del SDK) |

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
reproducen cada caso difícil con DOLs sintéticos, sin datos del juego. Los
nombres que salieron de leer el código tienen su propia prueba contra el
`main.dol` real: `tests/test_gx_function_names.py` los de GX (ver «Nombres de
GX por lectura del código») y `tests/test_sdk_function_names.py` los del resto
del SDK (ver «Otros nombres del SDK por lectura del código»). Las dos leen los
hechos del código con `tools/code_facts.py`, que se prueba con un programa
hecho a mano en `tests/test_code_facts.py`.

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
última. Entre dos rangos hay a veces algunas funciones sin nombre (cinco, la
mayor cantidad, entre AX y AXFX), de una de las dos vecinas.

| Biblioteca | Rango | Funciones | Con nombre |
|---|---|---|---|
| OS (primera parte) | `0x8016EDA0`–`0x80175778` | 146 | 109 |
| MTX | `0x8017580C`–`0x801759C8` | 4 | 4 |
| DVD | `0x801759C8`–`0x80179BB0` | 85 | 53 |
| VI | `0x80179DF0`–`0x8017BBB0` | 20 | 17 |
| PAD | `0x8017BF98`–`0x8017D73C` | 18 | 16 |
| AI | `0x8017D780`–`0x8017E008` | 17 | 13 |
| AR | `0x8017E04C`–`0x8017FB40` | 8 | 8 |
| ARQ | `0x8017FB40`–`0x8017FEDC` | 5 | 4 |
| AX | `0x8017FEDC`–`0x80182E44` | 56 | 16 |
| AXFX | `0x80183BD4`–`0x80185740` | 19 | 9 |
| MIX | `0x801857FC`–`0x801877DC` | 4 | 4 |
| DSP | `0x801877DC`–`0x80188300` | 14 | 13 |
| CARD | `0x80188300`–`0x8018ED98` | 89 | 71 |
| GX | `0x8018EDE0`–`0x80195F98` | 156 | 128 |
| TRK | `0x80195F98`–`0x8019B5F0` | 120 | 76 |
| Resto de TRK, biblioteca C y soporte del compilador | `0x8019B5F0`–`0x801A7A14` | 188 | 72 |
| DB | `0x801A7A14`–`0x801A848C` | 12 | 9 |
| EXI | `0x801A8580`–`0x801A9E68` | 22 | 21 |
| OS (segunda parte) | `0x801A9FF4`–`0x801AA1B0` | 1 | 1 |
| SI | `0x801AA420`–`0x801ABC18` | 24 | 23 |

Las filas de AX, AXFX y MIX son la zona del audio, entre `AXInit` y DSP. Casi
todas sus firmas faltan en la base. Antes de leer su código solo tenían nombre
`AXInit`, `AXInitEx` y los dos convertidores del chorus de AXFX (`do_src1` y
`do_src2`); la base reconoce `HandleReverb`, pero da ese nombre a dos funciones
(la de cada reverberación), así que no se nombra. Las demás salieron de leer el
código (ver «La zona del audio»). Entre AX y AXFX quedan cinco funciones sin
nombre, y entre AXFX y MIX una. La fila «Resto de TRK, biblioteca C y soporte del compilador» tiene, con nombre,
`sprintf`, `qsort`, `memmove`, las funciones matemáticas (`__ieee754_pow`,
`sin`, `tan`...), `__div2u`, `__construct_array` y, de TRK,
`CircleBufferReadBytes`.

La región del SDK (`0x8016EB94`–`0x801ABC18`) tiene 1 057 funciones y 673 con
nombre, el 64 %. Fuera de ella el código del juego y su middleware queda sin
nombre: 2 643 funciones antes y 746 después de SI (solo `main`). Las 384
funciones del SDK que siguen sin nombre se reparten así: 170 comparten firma con
otras, 152 no tienen firma en la base, 49 son cortas sin respaldo, 7 repiten
nombre, 4 tienen otro tamaño y 2 tienen nombre de C++. La zona del audio es el
caso más claro de las que no están en la base: 72 de sus 87 funciones.

**Lo que no se usa todavía.** La base trae, en la mitad de sus entradas (5 133
de 10 549), el archivo y el objeto del enlazador (`card.a CARDBios.o`, por
ejemplo). Daría la biblioteca exacta de cada función, también en la biblioteca C
y en TRK, y no solo la que dice el prefijo del nombre.

### Nombres de GX por lectura del código

Quedaban sin nombre 26 funciones de GX que el juego llama desde fuera de la
biblioteca. Se encontraron con `tools/find_library_entry_points.py`, que lista
los puntos de entrada de una biblioteca del SDK: las funciones suyas a las que
llega el resto del programa con un `bl`, con una llamada final (un `b` que
sale de la llamadora) o con un puntero guardado en los datos.

```sh
python3 tools/find_library_entry_points.py GX
python3 tools/find_library_entry_points.py AX --range 0x8017FEDC-0x801877FC   # un tramo dado a mano
```

GX tiene 156 funciones en un solo tramo (`0x8018EDE0`–`0x80195F98`) y el juego
alcanza 75 de ellas. 49 ya tenían nombre, por firma o por las reglas de antes;
las otras 26 son las de esta sección. Con ellas las 75 tienen nombre. Dos
límites de la herramienta, que explica su encabezado: las llamadas por un
registro (`bctrl`, `blrl`) no se ven leyendo el código, y el tramo de la
biblioteca es tan ancho como los nombres que lo dicen.

**Por qué la firma no bastó.** 14 de las 26 comparten firma con una hermana que
solo se distingue por un valor inmediato, como el número de un registro o un
desplazamiento (`GXSetTevColorIn` y `GXSetTevAlphaIn`, `GXSetColorUpdate` y
`GXSetAlphaUpdate`...). La suma de verificación ignora los inmediatos y la base
guarda un solo nombre por suma, así que su candidato es el de una hermana o el
de una función de otra biblioteca. En 8 de esas 14 el nombre del candidato era
el de otra función: `0x80191A3C` aparecía como `AIRegisterDMACallback` (7
funciones del programa comparten esa firma), `0x80191E58` como `GXSetLineWidth`
(es `GXSetPointSize`) y `0x80192C40` como `nlVector3::Set` (10 la comparten).
Eso es lo que evita la tabla de veredictos al no nombrar las firmas
compartidas. Las otras 12 no están en la base.

**Cómo se identificó cada una.** Con datos de su código que solo tiene una
función de GX:

- *El registro de hardware que escribe*, visto como una constante. Un registro
  XF se escribe con el comando `0x10` (`0x1009` es el número de canales,
  `0x103F` el de generadores de coordenadas de textura) y uno BP con el comando
  `0x61` (`0xEE`–`0xF2` es la niebla, `0xE0` y `0xE1` los colores de TEV). Los
  números son los de Dolphin (`BPMemory.h`, `XFMemory.h`).
- *La posición que lee o escribe dentro del bloque de estado de la biblioteca*
  (`GXData`), vista como el desplazamiento de un `lwz` o un `stw`. El código
  llega al bloque por un puntero guardado en los datos (`lwz r6, -0x4CB0(r2)`:
  la palabra `0x80325AD0`, que apunta a `0x80315CE0`). Por ejemplo, `+0x130` y
  `+0x170` son los ajustes de las etapas de TEV de color y de alfa, `+0x1D0` el
  modo del motor de píxeles, `+0x204` el modo general y `+0x5A8` «se está
  grabando una lista de visualización».
- *El campo de bits que inserta* con `rlwimi` (giro, primer bit, último bit).
- *Las funciones del SDK que llama.*

| Dirección | Nombre | Hechos de su código |
|---|---|---|
| `0x801912B0` | `GXSetArray` | Cambia el atributo `0x19` (normal, binormal y tangente) por `0x0A` (normal) y manda la dirección y el paso del arreglo a los registros `0xA0+n` y `0xB0+n` del procesador de comandos. |
| `0x80191584` | `GXSetNumTexGens` | Escribe el registro XF `0x103F` y un campo de 4 bits en `+0x204`. |
| `0x801915C0` | `GXSetMisc` | Un `switch` sobre el tipo de ajuste. Un caso guarda un valor y marca el estado como sucio (`+0x5AC`); los otros fijan «las listas guardan el estado» (`+0x5A9`) y «abortar la espera de la copia» (`+0x5AA`). |
| `0x801918D0` | `GXPixModeSync` | Manda por el comando `0x61` la palabra que hay en `+0x1DC`. |
| `0x801918F4` | `GXTexModeSync` | Manda por el comando `0x61` la constante `0x63000000`. |
| `0x80191A3C` | `GXSetDrawSyncCallback` | Con las interrupciones apagadas (`OSDisableInterrupts`, `OSRestoreInterrupts`) cambia la variable `-0x6BE8(r13)` y devuelve el valor anterior. Esa variable la lee `GXTokenInterruptHandler`. |
| `0x80191E58` | `GXSetPointSize` | Inserta el ancho en los bits 8–15 y el desfase en los bits 19–21 del registro BP de tamaño de línea y de punto (`+0x7C`). `GXSetLineWidth`, que tiene la misma forma, escribe los bits 0–7 y 16–18. |
| `0x801920A4` | `GXSetDispCopySrc` | Registros BP `0x49` y `0x4A` (esquina y tamaño del origen de la copia); guarda el origen en `+0x1E0` y `+0x1E4`, que lee `GXCopyDisp`. |
| `0x80192120` | `GXSetTexCopySrc` | La misma forma, pero guarda en `+0x1F0` y `+0x1F4`, que lee `GXCopyTex`. |
| `0x80192C40` | `GXInitLightPos` | Guarda tres flotantes en `+0x28`, `+0x2C` y `+0x30` del objeto de luz. |
| `0x80192CD8` | `GXSetChanAmbColor` | Registro XF `0x100A` (color ambiente 0); guarda el color en `+0xA8` y `+0xAC`. |
| `0x80192DC0` | `GXSetChanMatColor` | Registro XF `0x100C` (color del material 0); guarda el color en `+0xB0` y `+0xB4`. |
| `0x80192EA8` | `GXSetNumChans` | Registro XF `0x1009` y un campo de 3 bits en `+0x204`. |
| `0x80192EE4` | `GXSetChanCtrl` | Registros XF `0x100E`, `0x1010` y `0x1011` (control de los canales de color y de alfa). |
| `0x80192F94` | `GXGetTexBufferSize` | Tamaño de mosaico por formato: `0x40` bytes para el formato 6 (RGBA8) y para el `0x16` (Z24X8), `0x20` para el resto; un `switch` limitado a los formatos hasta `0x3C`. No escribe en el hardware. |
| `0x8019427C` | `GXSetTevColorIn` | Cuatro campos de 4 bits en el arreglo de ajustes de color de las etapas de TEV (`+0x130`). |
| `0x801942C0` | `GXSetTevAlphaIn` | Cuatro campos de 3 bits en el arreglo de ajustes de alfa (`+0x170`). |
| `0x80194304` | `GXSetTevColorOp` | Tres campos (dos de 1 bit y uno de 2) en el arreglo de color (`+0x130`). |
| `0x8019436C` | `GXSetTevAlphaOp` | Los mismos tres campos, en el arreglo de alfa (`+0x170`). |
| `0x801943D4` | `GXSetTevColor` | Registros BP `0xE0` y `0xE1` (los registros de color de TEV) por el comando `0x61`. |
| `0x80194848` | `GXSetFog` | Los cinco registros BP de la niebla, `0xEE` a `0xF2`. |
| `0x80194BD4` | `GXSetColorUpdate` | Un bit (el 3) de `+0x1D0`, el modo del motor de píxeles. |
| `0x80194C00` | `GXSetAlphaUpdate` | Un bit (el 4) de `+0x1D0`. |
| `0x80194E80` | `GXBeginDisplayList` | Arma un FIFO con el búfer de la lista y lo activa (`GXSetCPUFifo`); marca «grabando» (`+0x5A8`) y, si la lista guarda el estado (`+0x5A9`), copia antes `0x5B0` bytes del bloque. |
| `0x80194F4C` | `GXEndDisplayList` | Vacía (`GXFlush`), vuelve al FIFO anterior (`GXSetCPUFifo`), restaura el estado si la lista lo guardó, quita la marca `+0x5A8` y devuelve el tamaño grabado. |
| `0x8019553C` | `GXSetViewportJitter` | Guarda los seis valores del viewport en `+0x4F4` a `+0x508` y deja que `__GXSetViewport` los mande. |

**Qué comprueba la prueba.** `tests/test_gx_function_names.py` guarda esta tabla
y comprueba, contra el `main.dol` real: que cada dirección tiene el nombre y el
tamaño de la tabla y que su código tiene los hechos; que ninguna otra de las
156 funciones de GX los tiene (una fila que sirviera para dos funciones no
identificaría a ninguna); y dos cruces de datos: `GXSetDrawSyncCallback`
escribe la variable que lee `GXTokenInterruptHandler`, y `GXSetDispCopySrc` y
`GXSetTexCopySrc` escriben el estado que leen `GXCopyDisp` y `GXCopyTex`, cada
una el suyo.

### Otros nombres del SDK por lectura del código

Después de GX quedaban sin nombre 39 puntos de entrada de otras bibliotecas
del SDK: 16 de OS, 9 de CARD, 4 de DVD, 4 de VI, 2 de AI, 2 de DSP, 1 de AR y
1 de PAD (`tools/find_library_entry_points.py` con cada biblioteca). Son
funciones que un runtime nativo tiene que dar (ver
[NATIVE_RUNTIME.md](NATIVE_RUNTIME.md)). Se suman dos funciones internas de
CARD en las que se apoyan: `__CARDSyncCallback`, la que cada llamada síncrona
de CARD le pasa a su gemela asíncrona, y `__CARDDefaultApiCallback`, la que se
usa cuando no se da ninguna. Con ellas, `tools/find_library_entry_points.py`
ya no encuentra puntos de entrada sin nombre en las bibliotecas que dicen sus
nombres. La excepción era la zona del audio, que se nombró después (ver «La
zona del audio», abajo).

**Por qué la firma no bastó.** La mayoría tiene pocas instrucciones (un getter
o un setter de una variable, o una función vacía). 22 tienen «firma
compartida»: `OSCreateAlarm` y `OSInitThreadQueue`, por ejemplo, comparten la
de un constructor de C++ de otro juego (`nlListContainer<P8SaveData>::__ct`).
15 no están en la base. Y las cuatro funciones static que el SDK llama
`OnReset`, en cuatro archivos distintos, salieron como «nombre repetido» (dos)
o «corta sin respaldo» (dos).

**Cómo se identificó cada una.** Con los hechos de su código que lee
`tools/code_facts.py`, como las de GX, y dos clases de hechos más: los textos
cuya dirección arma la función (los mensajes que imprime) y las funciones que
tiene justo antes y justo después. El enlazador deja juntas las funciones de
un archivo fuente, en el orden en que están escritas, así que las vecinas dicen
de qué archivo viene una función: es lo que distingue `OSCreateAlarm` de
`OSInitThreadQueue`, que tienen el mismo código. El código y las variables
coinciden con las fuentes del SDK que reconstruyó la descompilación de
Twilight Princess ([zeldaret/tp](https://github.com/zeldaret/tp),
`libs/dolphin/src`). 20 de los nombres (10 de OS y todos los de DVD, VI y
DSP) también están en el mapa de símbolos de Mario Kart Wii que trae WiiCompiled
(`projects/mkwii/MAP.txt`), cuyo SDK comparte esos archivos (el mapa no tiene
funciones de CARD).

**Funciones static con el mismo nombre.** `symbols.txt` necesita un nombre por
función (el código generado los usa como identificadores), así que una función
static cuyo nombre el SDK repite en varios archivos lleva detrás el nombre del
archivo: `OnReset_OSAlarm` es la `OnReset` de `OSAlarm.c`.

| Dirección | Nombre | Hechos de su código |
|---|---|---|
| `0x8016F8C4` | `OSCreateAlarm` | Guarda cero en el manejador y la etiqueta de la alarma (`+0`, `+4`). Está en `OSAlarm.c`, entre `OSInitAlarm` y `InsertAlarm`. |
| `0x8016FF28` | `OnReset_OSAlarm` | Recorre la cola de alarmas (`-0x6FD0(r13)`) y cancela con `OSCancelAlarm` las que `__DVDTestAlarm` no reconoce. Es la función del registro de reinicio `0x802A3AB0` (prioridad `0xFFFFFFFF`), que registra `OSInitAlarm`. |
| `0x801701EC` | `OSSetCurrentHeap` | Cambia el montón actual (`-0x7698(r13)`, que empieza en -1) y devuelve el anterior. |
| `0x801702D8` | `OSCheckHeap` | Imprime «OSCheckHeap: Failed HeapArray in %d» con `OSReport`. |
| `0x80170638` | `OSGetArenaHi` | Lee el límite alto de la arena (`-0x6FB8(r13)`, en `.sbss`). |
| `0x80170640` | `OSGetArenaLo` | Lee el límite bajo (`-0x7690(r13)`, que empieza en -1). |
| `0x80170648` | `OSSetArenaHi` | Escribe el límite alto. |
| `0x80170650` | `OSSetArenaLo` | Escribe el límite bajo. |
| `0x80172684` | `OSGetFontEncode` | Lee el modo de TV (`0x800000CC`) y el estado de la salida de video digital (`0xCC00206E`), y guarda la codificación en una variable de 16 bits que empieza en `0xFFFF`. |
| `0x801730A8` | `OnReset_OSMemory` | En el reinicio final escribe `0xFF` en `0xCC004010` y enmascara las interrupciones de la memoria (`__OSMaskInterrupts`). Registro `0x802A5150`, prioridad 127, de `__OSInitMemoryProtection`. |
| `0x8017432C` | `__OSUnlockSram` | `UnlockSram(commit, 0)`. |
| `0x80174350` | `__OSUnlockSramEx` | `UnlockSram(commit, 0x14)`: el tamaño de la SRAM. |
| `0x801747F0` | `DefaultSwitchThreadCallback` | Vacía. Es el valor inicial de la variable `0x80321A10`, que leen `__OSThreadInit` y `SelectThread`. |
| `0x8017494C` | `OSInitThreadQueue` | Guarda cero en las dos palabras de la cola. Está en `OSThread.c`, justo antes de `OSGetCurrentThread`. |
| `0x8017495C` | `OSGetCurrentThread` | Lee `0x800000E4`, el hilo actual. |
| `0x801749A8` | `OSEnableScheduler` | Resta 1 al contador `-0x6F40(r13)` con las interrupciones apagadas (la función de antes, `OSDisableScheduler`, suma 1). |
| `0x80176B70` | `DVDOpen` | Imprime «Warning: DVDOpen(): file '%s' was not found under %s.» cuando `DVDConvertPathToEntrynum` no encuentra el archivo. |
| `0x80176F70` | `defaultOptionalCommandChecker` | Vacía, al inicio de `dvd.c` (antes de `DVDInit`). Es el valor inicial de la variable `0x80321A30`. |
| `0x801790F8` | `DVDSetAutoInvalidation` | Cambia `-0x7654(r13)`, que empieza en 1, y devuelve el valor anterior. |
| `0x801794A4` | `DVDGetCurrentDiskID` | Devuelve `0x80000000`, donde se copia el ID del disco al arrancar. |
| `0x8017A064` | `VISetPostRetraceCallback` | Cambia la función que `__VIRetraceHandler` llama al final del retrazo (`-0x6E48(r13)`). El juego no tiene `VISetPreRetraceCallback`. |
| `0x8017ACCC` | `VIConfigure` | Imprime «VIConfigure(): Tried to change mode from (%d) to (%d), which is forbidden» y llama a `setFbbRegs` y `setVerticalRegs`. |
| `0x8017B6EC` | `VIGetRetraceCount` | Lee el contador de retrazos (`-0x6E5C(r13)`), que suma `__VIRetraceHandler`. |
| `0x8017B798` | `VIGetNextField` | Calcula el campo actual con las interrupciones apagadas y lo combina con `HorVer.AdjustedDispPosY` (`0x802FAA98`, `+0xA`). Está entre `getCurrentFieldEvenOdd` y `VIGetCurrentLine`. |
| `0x8017D550` | `OnReset_Pad` | Quita la función de muestreo (`PADSetSamplingCallback`), espera a los controles (`SIBusy`) y recalibra los cuatro (`PADRecalibrate`). Registro `0x802A5958`, prioridad 127, de `PADInit`. |
| `0x8017DAE0` | `AISetStreamVolLeft` | Escribe el byte bajo de `0xCC006C04`. |
| `0x8017DB0C` | `AISetStreamVolRight` | Escribe el byte de arriba (bits 8–15) del mismo registro. |
| `0x8017E33C` | `__ARGetInterruptStatus` | Lee el registro de control del DSP (`0xCC00500A`) y deja el bit `0x20`, la interrupción del DMA de ARAM. |
| `0x801878EC` | `DSPCheckInit` | Lee la marca que pone `DSPInit` (`-0x6C58(r13)`). |
| `0x80187964` | `DSPAssertTask` | Con las interrupciones apagadas, pone una tarea como la siguiente y, si hay otra corriendo, interrumpe al DSP por `0xCC00500A`. |
| `0x80188300` | `__CARDDefaultApiCallback` | Vacía, al inicio de `CARDBios.c`. Seis funciones asíncronas de CARD la usan cuando no se les da una. |
| `0x80188304` | `__CARDSyncCallback` | Despierta (`OSWakeupThread`) la cola de hilos del bloque de control de la ranura (`0x80315A20`, `+0x8C`). |
| `0x801896F0` | `__CARDSync` | Duerme (`OSSleepThread`) en esa misma cola mientras el resultado sea «ocupada» (-1). |
| `0x80189788` | `OnReset_CARDBios` | Antes del reinicio final desmonta las dos ranuras (`CARDUnmount`); una tarjeta ocupada lo retrasa. Registro `0x802AAA98`, prioridad 127, de `CARDInit`. |
| `0x8018C260` | `CARDCheck` | `CARDCheckExAsync` con `__CARDSyncCallback` y luego `__CARDSync`. La cuenta de bytes va a una variable local. |
| `0x8018CC28` | `CARDMount` | `CARDMountAsync` con `__CARDSyncCallback` y luego `__CARDSync`. |
| `0x8018D554` | `CARDFormat` | `__CARDFormatRegionAsync` con la codificación de `__CARDGetFontEncode`, y luego `__CARDSync`. |
| `0x8018DEE4` | `CARDCreate` | `CARDCreateAsync` y luego `__CARDSync`. |
| `0x8018E358` | `CARDRead` | `CARDReadAsync` y luego `__CARDSync`. |
| `0x8018E6D4` | `CARDWrite` | `CARDWriteAsync` y luego `__CARDSync`. |
| `0x8018E8D0` | `CARDDelete` | `CARDDeleteAsync` y luego `__CARDSync`. |

#### La zona del audio

Entre `AXInit` y la biblioteca DSP (`0x8017FEDC`–`0x801877FC`) solo tenían
nombre cuatro funciones, así que la herramienta veía un tramo de AX de dos
funciones y casi ninguno de sus puntos de entrada. Medida a mano
(`--range 0x8017FEDC-0x801877FC`) tiene 28, y 27 no tenían nombre: 21 no están
en la base y 6 comparten firma. Leyendo el código, la zona resultó ser cuatro
bibliotecas seguidas: AX (las voces y el envío de cada cuadro de audio al DSP),
AXFX (los efectos: dos reverberaciones, chorus y delay), MIX (el mezclador que
va sobre AX) y el inicio de DSP.

Una voz de AX es un `AXVPB`. Su bloque de parámetros para el DSP (`AXPB`)
empieza en `+0x138`, y cada función que cambia una parte del bloque la marca en
`sync` (`+0x1C`) para que el siguiente cuadro la copie al DSP. La parte que
escribe distingue a cada función.

| Dirección | Nombre | Hechos de su código |
|---|---|---|
| `0x8018016C` | `AXAcquireVoice` | Saca una voz de las pilas de prioridades (`0x802FAB40`) con las interrupciones apagadas y le guarda la prioridad, la función de aviso y el contexto (`+0xC`, `+0x10`, `+0x14`). |
| `0x80180758` | `AXRegisterAuxACallback` | Guarda la función y el contexto del envío auxiliar A (`-0x6D38(r13)` y `-0x6D30(r13)`). |
| `0x80180764` | `AXRegisterAuxBCallback` | Lo mismo para el envío B (`-0x6D34` y `-0x6D2C`). |
| `0x80180E88` | `AXSetMode` | Cambia el modo de la lista de comandos (`-0x6CF0(r13)`). |
| `0x8018160C` | `AXRegisterCallback` | Cambia, con las interrupciones apagadas, la función que se llama en cada cuadro de audio (`-0x6CD0(r13)`) y devuelve la anterior. |
| `0x80182734` | `AXSetVoiceSrcType` | Escribe `srcSelect` y `coefSelect` del bloque (`+0x138`, y ahí `+0x8` y `+0xA`). |
| `0x801827F8` | `AXSetVoiceState` | Escribe `state` (`+0x146`) y marca `0x4`; al parar una voz pide además quitar el chasquido (`depop`, `+0x20`). |
| `0x80182854` | `AXSetVoiceType` | Escribe `type` (`+0x148`) y marca `0x8`. |
| `0x801828A0` | `AXSetVoiceMix` | Copia la mezcla (`+0x14A`), ajusta `mixerCtrl` (`+0x144`) y marca `0x10`. |
| `0x80182AF4` | `AXSetVoiceAddr` | Copia las direcciones de la muestra (`+0x1A6`) y marca `0x1000`. |
| `0x80182C04` | `AXSetVoiceAdpcm` | Copia los coeficientes ADPCM (`+0x1B6`). |
| `0x80182CA8` | `AXSetVoiceSrc` | Copia el convertidor de frecuencia (`+0x1DE`). |
| `0x80182D40` | `AXSetVoiceSrcRatio` | Convierte una razón de punto flotante en las dos mitades de la razón del convertidor (`+0x1DE` y `+0x1E0`). |
| `0x80182DD8` | `AXSetVoiceAdpcmLoop` | Copia el estado ADPCM del bucle (`+0x1EC`). |
| `0x80183BD4` | `AXFXReverbHiInit` | Con las interrupciones apagadas, limpia «desactivada por ahora» (`+0x1C4`) y arma la reverberación alta. |
| `0x8018469C` | `AXFXReverbStdInit` | Lo mismo para la reverberación estándar (`+0x13C`). |
| `0x80184ABC` | `AXFXChorusInit` | Pide sus líneas de retardo al gancho de memoria (`-0x75D8(r13)`). Va justo después de `do_src1` y `do_src2`, los dos convertidores de `chorus.c`. |
| `0x801855FC` | `AXFXDelayInit` | Olvida los tres búferes del delay (`+0x30`, `+0x34` y `+0x38`) y aplica los ajustes. |
| `0x801856E4` | `__AXFXAllocFunction` | Pide memoria al montón actual (`OSAllocFromHeap`). Es el valor inicial del gancho `-0x75D8(r13)`. |
| `0x8018570C` | `__AXFXFreeFunction` | La devuelve (`OSFreeToHeap`). Es el valor inicial del gancho `-0x75D4(r13)`. |
| `0x80185734` | `AXFXSetHooks` | Cambia los dos ganchos. |
| `0x801857FC` | `MIXInit` | Reinicia los 64 canales (`0x80314220`: modo `0x50000000`, envíos a -96 dB), el atenuador del streaming del disco y el modo de sonido. |
| `0x801858C4` | `MIXInitChannel` | Prepara el canal de una voz según el modo de sonido, con las interrupciones apagadas. |
| `0x80186740` | `MIXReleaseChannel` | `__MIXChannel[voz->index].axvpb = NULL`. |
| `0x80186760` | `MIXUpdateSettings` | Lleva el atenuador del streaming un paso hacia lo pedido, con el volumen de streaming de AI (`AISetStreamVolLeft` y `AISetStreamVolRight`). |
| `0x801877DC` | `DSPCheckMailToDSP` | Lee el bit 15 de `0xCC005000`: el DSP todavía no leyó el último correo. |
| `0x801877EC` | `DSPCheckMailFromDSP` | Lee el bit 15 de `0xCC005004`: hay un correo del DSP. Va justo antes de `DSPReadMailFromDSP`. |

**Qué comprueba la prueba.** `tests/test_sdk_function_names.py` guarda estas
dos tablas y comprueba, contra el `main.dol` real: que cada dirección tiene el
nombre y el tamaño de la tabla y que su código tiene los hechos; que ninguna
otra función de su biblioteca los tiene; y los datos detrás de los nombres:
cada `OnReset` es la función del registro de reinicio que registra su
biblioteca, con su prioridad; las funciones vacías de OS y DVD y los dos
ganchos de AXFX son el valor inicial de la variable que los guarda; las seis
funciones asíncronas de CARD usan `__CARDDefaultApiCallback`; lo que guardan
`AXRegisterAuxACallback`, `AXRegisterAuxBCallback`, `AXRegisterCallback`,
`AXSetMode`, `AXFXSetHooks` y `VISetPostRetraceCallback` lo leen otras
funciones de su biblioteca; las variables empiezan con los valores de las
fuentes del SDK; `__VIRetraceHandler` cuenta los retrazos que devuelve
`VIGetRetraceCount` y llama a la función de antes del retrazo antes que a la de
después; y `DSPCheckInit` lee la marca que pone `DSPInit`.

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
- Un nombre por lectura del código es una deducción. Las pruebas demuestran
  que los hechos de la tabla (registro, desplazamiento, campo de bits)
  pertenecen a una sola función de GX; que ese comportamiento sea el de,
  por ejemplo, `GXSetPointSize` sale de conocer el SDK y de los registros que
  describe Dolphin, no de una base de firmas ni del código fuente del SDK.
- 452 de las 1 057 funciones de la región del SDK siguen sin nombre (ver
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
| Named functions | 682, all with evidence (119 from reading the code, 563 from SDK signatures) |

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
reproduce every hard case with synthetic DOLs, no game data needed. The
names that came from reading the code have their own test against the real
`main.dol`: `tests/test_gx_function_names.py` for GX (see "GX names by reading
the code") and `tests/test_sdk_function_names.py` for the rest of the SDK (see
"Other SDK names by reading the code"). Both read the facts of the code with
`tools/code_facts.py`, which is tested on a hand-built program in
`tests/test_code_facts.py`.

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
Between two ranges there are sometimes a few unnamed functions (five at most,
between AX and AXFX), which belong to one of the two neighbours.

| Library | Range | Functions | Named |
|---|---|---|---|
| OS (first part) | `0x8016EDA0`–`0x80175778` | 146 | 109 |
| MTX | `0x8017580C`–`0x801759C8` | 4 | 4 |
| DVD | `0x801759C8`–`0x80179BB0` | 85 | 53 |
| VI | `0x80179DF0`–`0x8017BBB0` | 20 | 17 |
| PAD | `0x8017BF98`–`0x8017D73C` | 18 | 16 |
| AI | `0x8017D780`–`0x8017E008` | 17 | 13 |
| AR | `0x8017E04C`–`0x8017FB40` | 8 | 8 |
| ARQ | `0x8017FB40`–`0x8017FEDC` | 5 | 4 |
| AX | `0x8017FEDC`–`0x80182E44` | 56 | 16 |
| AXFX | `0x80183BD4`–`0x80185740` | 19 | 9 |
| MIX | `0x801857FC`–`0x801877DC` | 4 | 4 |
| DSP | `0x801877DC`–`0x80188300` | 14 | 13 |
| CARD | `0x80188300`–`0x8018ED98` | 89 | 71 |
| GX | `0x8018EDE0`–`0x80195F98` | 156 | 128 |
| TRK | `0x80195F98`–`0x8019B5F0` | 120 | 76 |
| Rest of TRK, C library and compiler support | `0x8019B5F0`–`0x801A7A14` | 188 | 72 |
| DB | `0x801A7A14`–`0x801A848C` | 12 | 9 |
| EXI | `0x801A8580`–`0x801A9E68` | 22 | 21 |
| OS (second part) | `0x801A9FF4`–`0x801AA1B0` | 1 | 1 |
| SI | `0x801AA420`–`0x801ABC18` | 24 | 23 |

The AX, AXFX and MIX rows are the audio zone, between `AXInit` and DSP. Most
of its signatures are missing from the database. Before its code was read,
only `AXInit`, `AXInitEx` and the two converters of the AXFX chorus (`do_src1`
and `do_src2`) had a name; the database recognises `HandleReverb`, but gives
that name to two functions (the one of each reverb), so it is not used. The
others came from reading the code (see "The audio zone"). Five unnamed
functions are left between AX and AXFX, and one between AXFX and MIX. The row "Rest of TRK, C library and compiler support" has, named,
`sprintf`, `qsort`, `memmove`, the math functions (`__ieee754_pow`, `sin`,
`tan`...), `__div2u`, `__construct_array` and, from TRK,
`CircleBufferReadBytes`.

The SDK region (`0x8016EB94`–`0x801ABC18`) has 1,057 functions and 673 named,
64 %. Outside it the game's code and its middleware have no names: 2,643
functions before it and 746 after SI (only `main`). The 384 SDK functions that
are still unnamed split like this: 170 share a signature with others, 152
have no signature in the database, 49 are short with no backing, 7 repeat a
name, 4 have another size and 2 have a C++ name. The audio zone is the
clearest case of those missing from the database: 72 of its 87 functions.

**What is not used yet.** In half of its entries (5,133 of 10,549) the
database carries the linker's archive and object (`card.a CARDBios.o`, for
example). It would give the exact library of each function, in the C library
and in TRK too, and not only the one the name prefix says.

### GX names by reading the code

26 functions of GX that the game calls from outside the library were still
unnamed. They were found with `tools/find_library_entry_points.py`, which
lists the entry points of an SDK library: its functions that the rest of the
program reaches with a `bl`, with a tail call (a `b` that leaves the caller)
or with a pointer kept in the data.

```sh
python3 tools/find_library_entry_points.py GX
python3 tools/find_library_entry_points.py AX --range 0x8017FEDC-0x801877FC   # a stretch given by hand
```

GX has 156 functions in a single stretch (`0x8018EDE0`–`0x80195F98`) and the
game reaches 75 of them. 49 already had a name, by signature or by the earlier
rules; the other 26 are the ones in this section. With them all 75 have a
name. Two limits of the tool, which its header explains: calls through a
register (`bctrl`, `blrl`) cannot be seen by reading the code, and the
library's stretch is only as wide as the names that say it.

**Why the signature was not enough.** 14 of the 26 share a signature with a
sibling that differs only in an immediate value, such as a register number or
an offset (`GXSetTevColorIn` and `GXSetTevAlphaIn`, `GXSetColorUpdate` and
`GXSetAlphaUpdate`...). The checksum ignores immediates and the database keeps
a single name per checksum, so its candidate is a sibling's, or the name of a
function of another library. In 8 of those 14 the candidate's name belonged to
another function: `0x80191A3C` showed up as `AIRegisterDMACallback` (7
functions of the program share that signature), `0x80191E58` as
`GXSetLineWidth` (it is `GXSetPointSize`) and `0x80192C40` as `nlVector3::Set`
(10 share it). That is what the table of verdicts avoids by not naming shared
signatures. The other 12 are not in the database.

**How each one was identified.** With facts in its code that only one function
of GX has:

- *The hardware register it writes*, seen as a constant. An XF register is
  written with command `0x10` (`0x1009` is the number of channels, `0x103F`
  the number of texture coordinate generators) and a BP register with command
  `0x61` (`0xEE`–`0xF2` is fog, `0xE0` and `0xE1` the TEV colors). The numbers
  are Dolphin's (`BPMemory.h`, `XFMemory.h`).
- *The position it reads or writes inside the library's state block*
  (`GXData`), seen as the displacement of a `lwz` or a `stw`. The code reaches
  the block through a pointer kept in the data (`lwz r6, -0x4CB0(r2)`: the word
  `0x80325AD0`, which points to `0x80315CE0`). For example, `+0x130` and
  `+0x170` are the settings of the TEV color and alpha stages, `+0x1D0` the
  pixel engine mode, `+0x204` the general mode and `+0x5A8` "a display list is
  being recorded".
- *The bit field it inserts* with `rlwimi` (rotation, first bit, last bit).
- *The SDK functions it calls.*

| Address | Name | Facts in its code |
|---|---|---|
| `0x801912B0` | `GXSetArray` | Turns attribute `0x19` (normal, binormal and tangent) into `0x0A` (normal) and sends the array's address and stride to the command processor registers `0xA0+n` and `0xB0+n`. |
| `0x80191584` | `GXSetNumTexGens` | Writes XF register `0x103F` and a 4-bit field at `+0x204`. |
| `0x801915C0` | `GXSetMisc` | A `switch` on the kind of setting. One case stores a value and marks the state dirty (`+0x5AC`); the others set "display lists save the state" (`+0x5A9`) and "abort waiting for the copy" (`+0x5AA`). |
| `0x801918D0` | `GXPixModeSync` | Sends the word at `+0x1DC` with command `0x61`. |
| `0x801918F4` | `GXTexModeSync` | Sends the constant `0x63000000` with command `0x61`. |
| `0x80191A3C` | `GXSetDrawSyncCallback` | With interrupts off (`OSDisableInterrupts`, `OSRestoreInterrupts`) it swaps the variable `-0x6BE8(r13)` and returns the old value. `GXTokenInterruptHandler` reads that variable. |
| `0x80191E58` | `GXSetPointSize` | Inserts the width in bits 8–15 and the offset in bits 19–21 of the BP line and point size register (`+0x7C`). `GXSetLineWidth`, which has the same shape, writes bits 0–7 and 16–18. |
| `0x801920A4` | `GXSetDispCopySrc` | BP registers `0x49` and `0x4A` (corner and size of the copy source); it keeps the source at `+0x1E0` and `+0x1E4`, which `GXCopyDisp` reads. |
| `0x80192120` | `GXSetTexCopySrc` | The same shape, but it keeps it at `+0x1F0` and `+0x1F4`, which `GXCopyTex` reads. |
| `0x80192C40` | `GXInitLightPos` | Stores three floats at `+0x28`, `+0x2C` and `+0x30` of the light object. |
| `0x80192CD8` | `GXSetChanAmbColor` | XF register `0x100A` (ambient color 0); keeps the color at `+0xA8` and `+0xAC`. |
| `0x80192DC0` | `GXSetChanMatColor` | XF register `0x100C` (material color 0); keeps the color at `+0xB0` and `+0xB4`. |
| `0x80192EA8` | `GXSetNumChans` | XF register `0x1009` and a 3-bit field at `+0x204`. |
| `0x80192EE4` | `GXSetChanCtrl` | XF registers `0x100E`, `0x1010` and `0x1011` (control of the color and alpha channels). |
| `0x80192F94` | `GXGetTexBufferSize` | Tile size by format: `0x40` bytes for format 6 (RGBA8) and for `0x16` (Z24X8), `0x20` for the rest; a `switch` limited to formats up to `0x3C`. It writes nothing to the hardware. |
| `0x8019427C` | `GXSetTevColorIn` | Four 4-bit fields in the array of color settings of the TEV stages (`+0x130`). |
| `0x801942C0` | `GXSetTevAlphaIn` | Four 3-bit fields in the array of alpha settings (`+0x170`). |
| `0x80194304` | `GXSetTevColorOp` | Three fields (two of 1 bit and one of 2) in the color array (`+0x130`). |
| `0x8019436C` | `GXSetTevAlphaOp` | The same three fields, in the alpha array (`+0x170`). |
| `0x801943D4` | `GXSetTevColor` | BP registers `0xE0` and `0xE1` (the TEV color registers) with command `0x61`. |
| `0x80194848` | `GXSetFog` | The five BP fog registers, `0xEE` to `0xF2`. |
| `0x80194BD4` | `GXSetColorUpdate` | One bit (bit 3) of `+0x1D0`, the pixel engine mode. |
| `0x80194C00` | `GXSetAlphaUpdate` | One bit (bit 4) of `+0x1D0`. |
| `0x80194E80` | `GXBeginDisplayList` | Builds a FIFO over the list's buffer and activates it (`GXSetCPUFifo`); marks "recording" (`+0x5A8`) and, if the list saves the state (`+0x5A9`), first copies `0x5B0` bytes of the block. |
| `0x80194F4C` | `GXEndDisplayList` | Flushes (`GXFlush`), goes back to the previous FIFO (`GXSetCPUFifo`), restores the state if the list saved it, clears the `+0x5A8` mark and returns the recorded size. |
| `0x8019553C` | `GXSetViewportJitter` | Keeps the six viewport values at `+0x4F4` to `+0x508` and lets `__GXSetViewport` send them. |

**What the test checks.** `tests/test_gx_function_names.py` holds this table
and checks, against the real `main.dol`: that each address has the table's
name and size and that its code has the facts; that no other of the 156
functions of GX has them (a row that fitted two functions would not identify
either); and two data cross-checks: `GXSetDrawSyncCallback` writes the variable
that `GXTokenInterruptHandler` reads, and `GXSetDispCopySrc` and
`GXSetTexCopySrc` write the state that `GXCopyDisp` and `GXCopyTex` read, each
one its own.

### Other SDK names by reading the code

After GX, 39 entry points of other SDK libraries were still unnamed: 16 of OS,
9 of CARD, 4 of DVD, 4 of VI, 2 of AI, 2 of DSP, 1 of AR and 1 of PAD
(`tools/find_library_entry_points.py` with each library). They are functions
that a native runtime has to provide (see
[NATIVE_RUNTIME.md](NATIVE_RUNTIME.md)). Two internal CARD functions that they
rest on join them: `__CARDSyncCallback`, the one each synchronous CARD call
passes to its asynchronous twin, and `__CARDDefaultApiCallback`, the one used
when none is given. With them, `tools/find_library_entry_points.py` finds no
unnamed entry point left in the libraries that their names mark. The exception
was the audio zone, named afterwards (see "The audio zone", below).

**Why the signature was not enough.** Most are a few instructions long (a
getter or a setter of one variable, or an empty function). 22 have a "shared
checksum": `OSCreateAlarm` and `OSInitThreadQueue`, for example, share the one
of a C++ constructor from another game (`nlListContainer<P8SaveData>::__ct`).
15 are not in the database. And the four static functions that the SDK calls
`OnReset`, in four different files, came out as "shared name" (two) or "short
and uncorroborated" (two).

**How each one was identified.** With the facts of its code that
`tools/code_facts.py` reads, like the GX ones, and two more kinds of facts: the
texts whose address the function puts together (the messages it prints) and
the functions right before and right after it. The linker keeps the functions
of one source file together, in the order they are written, so the neighbors
say which file a function comes from: that is what tells `OSCreateAlarm` from
`OSInitThreadQueue`, which have the same code. The code and the variables match
the SDK sources that the Twilight Princess decompilation reconstructed
([zeldaret/tp](https://github.com/zeldaret/tp), `libs/dolphin/src`). 20 of the
names (10 of OS and all those of DVD, VI and DSP) are also in the Mario Kart Wii symbol map
that WiiCompiled carries (`projects/mkwii/MAP.txt`), whose SDK shares those
files (the map has no CARD functions).

**Static functions with the same name.** `symbols.txt` needs one name per
function (the generated code uses the names as identifiers), so a static
function whose name the SDK repeats in several files gets the file's name after
its own: `OnReset_OSAlarm` is the `OnReset` of `OSAlarm.c`.

| Address | Name | Facts of its code |
|---|---|---|
| `0x8016F8C4` | `OSCreateAlarm` | Stores zero into the handler and the tag of the alarm (`+0`, `+4`). It sits in `OSAlarm.c`, between `OSInitAlarm` and `InsertAlarm`. |
| `0x8016FF28` | `OnReset_OSAlarm` | Walks the alarm queue (`-0x6FD0(r13)`) and cancels with `OSCancelAlarm` the alarms that `__DVDTestAlarm` does not recognise. It is the function of the reset record `0x802A3AB0` (priority `0xFFFFFFFF`), which `OSInitAlarm` registers. |
| `0x801701EC` | `OSSetCurrentHeap` | Swaps the current heap (`-0x7698(r13)`, which starts at -1) and returns the one before. |
| `0x801702D8` | `OSCheckHeap` | Prints "OSCheckHeap: Failed HeapArray in %d" with `OSReport`. |
| `0x80170638` | `OSGetArenaHi` | Reads the high bound of the arena (`-0x6FB8(r13)`, in `.sbss`). |
| `0x80170640` | `OSGetArenaLo` | Reads the low bound (`-0x7690(r13)`, which starts at -1). |
| `0x80170648` | `OSSetArenaHi` | Writes the high bound. |
| `0x80170650` | `OSSetArenaLo` | Writes the low bound. |
| `0x80172684` | `OSGetFontEncode` | Reads the TV mode (`0x800000CC`) and the status of the digital video output (`0xCC00206E`), and keeps the encoding in a 16-bit variable that starts at `0xFFFF`. |
| `0x801730A8` | `OnReset_OSMemory` | On the final reset, writes `0xFF` to `0xCC004010` and masks the memory interrupts (`__OSMaskInterrupts`). Record `0x802A5150`, priority 127, from `__OSInitMemoryProtection`. |
| `0x8017432C` | `__OSUnlockSram` | `UnlockSram(commit, 0)`. |
| `0x80174350` | `__OSUnlockSramEx` | `UnlockSram(commit, 0x14)`: the size of the SRAM. |
| `0x801747F0` | `DefaultSwitchThreadCallback` | Empty. It is the first value of the variable `0x80321A10`, which `__OSThreadInit` and `SelectThread` read. |
| `0x8017494C` | `OSInitThreadQueue` | Stores zero into the two words of the queue. It sits in `OSThread.c`, right before `OSGetCurrentThread`. |
| `0x8017495C` | `OSGetCurrentThread` | Reads `0x800000E4`, the current thread. |
| `0x801749A8` | `OSEnableScheduler` | Takes 1 from the counter `-0x6F40(r13)` with the interrupts off (the function before it, `OSDisableScheduler`, adds 1). |
| `0x80176B70` | `DVDOpen` | Prints "Warning: DVDOpen(): file '%s' was not found under %s." when `DVDConvertPathToEntrynum` does not find the file. |
| `0x80176F70` | `defaultOptionalCommandChecker` | Empty, at the start of `dvd.c` (before `DVDInit`). It is the first value of the variable `0x80321A30`. |
| `0x801790F8` | `DVDSetAutoInvalidation` | Swaps `-0x7654(r13)`, which starts at 1, and returns the value before. |
| `0x801794A4` | `DVDGetCurrentDiskID` | Returns `0x80000000`, where the disc ID is copied at boot. |
| `0x8017A064` | `VISetPostRetraceCallback` | Swaps the function that `__VIRetraceHandler` calls at the end of the retrace (`-0x6E48(r13)`). The game has no `VISetPreRetraceCallback`. |
| `0x8017ACCC` | `VIConfigure` | Prints "VIConfigure(): Tried to change mode from (%d) to (%d), which is forbidden" and calls `setFbbRegs` and `setVerticalRegs`. |
| `0x8017B6EC` | `VIGetRetraceCount` | Reads the retrace counter (`-0x6E5C(r13)`), which `__VIRetraceHandler` adds to. |
| `0x8017B798` | `VIGetNextField` | Works out the current field with the interrupts off and combines it with `HorVer.AdjustedDispPosY` (`0x802FAA98`, `+0xA`). It sits between `getCurrentFieldEvenOdd` and `VIGetCurrentLine`. |
| `0x8017D550` | `OnReset_Pad` | Drops the sampling function (`PADSetSamplingCallback`), waits for the controllers (`SIBusy`) and recalibrates all four (`PADRecalibrate`). Record `0x802A5958`, priority 127, from `PADInit`. |
| `0x8017DAE0` | `AISetStreamVolLeft` | Writes the low byte of `0xCC006C04`. |
| `0x8017DB0C` | `AISetStreamVolRight` | Writes the byte above it (bits 8–15) of the same register. |
| `0x8017E33C` | `__ARGetInterruptStatus` | Reads the DSP control register (`0xCC00500A`) and keeps bit `0x20`, the ARAM DMA interrupt. |
| `0x801878EC` | `DSPCheckInit` | Reads the flag that `DSPInit` sets (`-0x6C58(r13)`). |
| `0x80187964` | `DSPAssertTask` | With the interrupts off, makes a task the next one and, if another is running, interrupts the DSP through `0xCC00500A`. |
| `0x80188300` | `__CARDDefaultApiCallback` | Empty, at the start of `CARDBios.c`. Six asynchronous CARD functions use it when they are not given one. |
| `0x80188304` | `__CARDSyncCallback` | Wakes (`OSWakeupThread`) the thread queue of the slot's control block (`0x80315A20`, `+0x8C`). |
| `0x801896F0` | `__CARDSync` | Sleeps (`OSSleepThread`) on that same queue while the result is "busy" (-1). |
| `0x80189788` | `OnReset_CARDBios` | Before the final reset, unmounts both slots (`CARDUnmount`); a busy card delays it. Record `0x802AAA98`, priority 127, from `CARDInit`. |
| `0x8018C260` | `CARDCheck` | `CARDCheckExAsync` with `__CARDSyncCallback`, then `__CARDSync`. The byte count goes to a local variable. |
| `0x8018CC28` | `CARDMount` | `CARDMountAsync` with `__CARDSyncCallback`, then `__CARDSync`. |
| `0x8018D554` | `CARDFormat` | `__CARDFormatRegionAsync` with the encoding from `__CARDGetFontEncode`, then `__CARDSync`. |
| `0x8018DEE4` | `CARDCreate` | `CARDCreateAsync`, then `__CARDSync`. |
| `0x8018E358` | `CARDRead` | `CARDReadAsync`, then `__CARDSync`. |
| `0x8018E6D4` | `CARDWrite` | `CARDWriteAsync`, then `__CARDSync`. |
| `0x8018E8D0` | `CARDDelete` | `CARDDeleteAsync`, then `__CARDSync`. |

#### The audio zone

Between `AXInit` and the DSP library (`0x8017FEDC`–`0x801877FC`) only four
functions had a name, so the tool saw an AX stretch of two functions and
almost none of its entry points. Measured by hand
(`--range 0x8017FEDC-0x801877FC`) it has 28, and 27 had no name: 21 are not in
the database and 6 share a checksum. Reading the code, the zone turned out to
be four libraries in a row: AX (the voices, and sending each audio frame to
the DSP), AXFX (the effects: two reverbs, chorus and delay), MIX (the mixer
that sits on AX) and the start of DSP.

An AX voice is an `AXVPB`. Its parameter block for the DSP (`AXPB`) starts at
`+0x138`, and every function that changes a part of the block marks it in
`sync` (`+0x1C`) so that the next frame copies it to the DSP. The part it
writes tells each function apart.

| Address | Name | Facts of its code |
|---|---|---|
| `0x8018016C` | `AXAcquireVoice` | Takes a voice from the stacks of the priorities (`0x802FAB40`) with the interrupts off and keeps the priority, the callback and the context in it (`+0xC`, `+0x10`, `+0x14`). |
| `0x80180758` | `AXRegisterAuxACallback` | Keeps the function and the context of aux send A (`-0x6D38(r13)` and `-0x6D30(r13)`). |
| `0x80180764` | `AXRegisterAuxBCallback` | The same for send B (`-0x6D34` and `-0x6D2C`). |
| `0x80180E88` | `AXSetMode` | Swaps the mode of the command list (`-0x6CF0(r13)`). |
| `0x8018160C` | `AXRegisterCallback` | Swaps, with the interrupts off, the function called on every audio frame (`-0x6CD0(r13)`) and returns the one before. |
| `0x80182734` | `AXSetVoiceSrcType` | Writes `srcSelect` and `coefSelect` of the block (`+0x138`, and there `+0x8` and `+0xA`). |
| `0x801827F8` | `AXSetVoiceState` | Writes `state` (`+0x146`) and marks `0x4`; stopping a voice also asks for a depop (`+0x20`). |
| `0x80182854` | `AXSetVoiceType` | Writes `type` (`+0x148`) and marks `0x8`. |
| `0x801828A0` | `AXSetVoiceMix` | Copies the mix (`+0x14A`), adjusts `mixerCtrl` (`+0x144`) and marks `0x10`. |
| `0x80182AF4` | `AXSetVoiceAddr` | Copies the addresses of the sample (`+0x1A6`) and marks `0x1000`. |
| `0x80182C04` | `AXSetVoiceAdpcm` | Copies the ADPCM coefficients (`+0x1B6`). |
| `0x80182CA8` | `AXSetVoiceSrc` | Copies the sample-rate converter (`+0x1DE`). |
| `0x80182D40` | `AXSetVoiceSrcRatio` | Turns a floating-point ratio into the two halves of the converter's ratio (`+0x1DE` and `+0x1E0`). |
| `0x80182DD8` | `AXSetVoiceAdpcmLoop` | Copies the ADPCM state of the loop (`+0x1EC`). |
| `0x80183BD4` | `AXFXReverbHiInit` | With the interrupts off, clears "temporarily disabled" (`+0x1C4`) and builds the high reverb. |
| `0x8018469C` | `AXFXReverbStdInit` | The same for the standard reverb (`+0x13C`). |
| `0x80184ABC` | `AXFXChorusInit` | Asks the memory hook (`-0x75D8(r13)`) for its delay lines. It comes right after `do_src1` and `do_src2`, the two converters of `chorus.c`. |
| `0x801855FC` | `AXFXDelayInit` | Forgets the three buffers of the delay (`+0x30`, `+0x34` and `+0x38`) and applies the settings. |
| `0x801856E4` | `__AXFXAllocFunction` | Takes memory from the current heap (`OSAllocFromHeap`). It is the first value of the hook `-0x75D8(r13)`. |
| `0x8018570C` | `__AXFXFreeFunction` | Gives it back (`OSFreeToHeap`). It is the first value of the hook `-0x75D4(r13)`. |
| `0x80185734` | `AXFXSetHooks` | Swaps both hooks. |
| `0x801857FC` | `MIXInit` | Resets the 64 channels (`0x80314220`: mode `0x50000000`, sends at -96 dB), the fader of the disc stream and the sound mode. |
| `0x801858C4` | `MIXInitChannel` | Sets up a voice's channel for the sound mode, with the interrupts off. |
| `0x80186740` | `MIXReleaseChannel` | `__MIXChannel[voice->index].axvpb = NULL`. |
| `0x80186760` | `MIXUpdateSettings` | Brings the fader of the disc stream one step closer to what was asked, through the AI stream volume (`AISetStreamVolLeft` and `AISetStreamVolRight`). |
| `0x801877DC` | `DSPCheckMailToDSP` | Reads bit 15 of `0xCC005000`: the DSP has not read the last mail yet. |
| `0x801877EC` | `DSPCheckMailFromDSP` | Reads bit 15 of `0xCC005004`: there is a mail from the DSP. It comes right before `DSPReadMailFromDSP`. |

**What the test checks.** `tests/test_sdk_function_names.py` holds these two
tables and checks, against the real `main.dol`: that each address has the
table's name and size and that its code has the facts; that no other function
of its library has them; and the data behind the names: each `OnReset` is the
function of the reset record that its library registers, with its priority;
the empty functions of OS and DVD and the two AXFX hooks are the first value
of the variable that holds them; the six asynchronous CARD functions use
`__CARDDefaultApiCallback`; what `AXRegisterAuxACallback`,
`AXRegisterAuxBCallback`, `AXRegisterCallback`, `AXSetMode`, `AXFXSetHooks`
and `VISetPostRetraceCallback` keep is read by other functions of their
library; the variables start with the values of the SDK sources; `__VIRetraceHandler` counts the retraces that `VIGetRetraceCount`
returns and calls the function for before the retrace ahead of the one for
after; and `DSPCheckInit` reads the flag that `DSPInit` sets.

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
- A name from reading the code is a deduction. The tests show that the facts
  in the table (register, offset, bit field) belong to one function of GX
  only; that this behaviour is that of, say, `GXSetPointSize` comes from
  knowing the SDK and the registers Dolphin describes, not from a signature
  database or from the SDK's source code.
- 452 of the 1,057 functions of the SDK region are still unnamed (see
  above); AX and AXFX are the biggest gap.
- The `PPCArch` functions (from `0x8016EB94`) are two instructions that read
  or write a special register. The signature does not keep the register
  number, so it cannot tell them apart and they stay unnamed. A separate rule
  that reads that number would be needed.
- The names already written are in `symbols.txt` and `tools/analyze.py` keeps
  them when it regenerates; Dolphin's database is only needed to repeat or
  widen the search.
