# Contributing and code style / Cómo contribuir y estilo de código

[Español](#español) · [English](#english)

---

## Español

La meta: que cualquier desarrollador que abra un archivo de este repositorio
entienda qué hace y por qué, sin tener que preguntarle a nadie.

### Principios

1. **Nombres que se explican solos.** `function_start`, `jump_table_address`,
   `exception_index_sizes`; nunca `fs`, `jt`, `tmp2`. Las únicas abreviaturas
   permitidas son términos estándar que cualquiera puede buscar:
   - campos del ISA tal como los nombra el manual IBM PowerPC 750CL
     (`rd`, `ra`, `rb`, `BO`, `LK`, `SIMM`);
   - siglas conocidas (`DOL`, `SDK`, `GX`, `BSS`, `CPU`, `SPR`).
2. **Comentarios que explican el porqué.** El código ya dice *qué* hace; el
   comentario explica la razón, la fuente o la trampa. Ejemplo bueno: "Los
   casos de un switch parecen punteros a funciones, por eso no se usan para
   delimitar tablas". Ejemplo malo: "suma 4 a la dirección".
3. **Sin números mágicos.** Toda constante lleva nombre y, si viene del
   hardware, del SDK o del juego, de dónde salió (`SMALL_DATA_BASE = 0x80329080
   # set by __init_registers`).
4. **Cada afirmación sobre el juego trae su evidencia.** Ponerle nombre a una
   función (`OSResetSystem`) exige documentar por qué en
   `docs/FUNCTION_MAP.md`. Lo que no está probado conserva su nombre genérico
   (`fn_801736C0`).
5. **Todo resultado es reproducible.** Cualquier archivo generado
   (`config/G4AEE9/symbols.txt`) se puede regenerar con una herramienta del
   repo, y la documentación explica el comando.

### Idiomas

- Identificadores, comentarios dentro del código y mensajes de commit en
  **inglés**: es el idioma común de la escena de decomp/recomp y permite que
  cualquiera colabore.
- Encabezados de módulo y toda la documentación (`README.md`, `docs/`,
  este archivo) en **español e inglés**.
- Nombres de archivo en inglés.

### Python (herramientas en `tools/`)

- Python 3.11 o más reciente, con type hints, `pathlib` y `argparse`.
- `ruff` para lint y formato; la configuración está en `pyproject.toml`.
  Antes de hacer commit:
  ```sh
  python3 -m ruff format tools tests
  python3 -m ruff check tools tests
  python3 -m pytest
  ```
- Las pruebas no deben necesitar datos del juego. Las que sí los necesitan
  se saltan solas salvo que `HM_MM_DOL` apunte a tu `main.dol`.
- Un script = una tarea, con un docstring que diga para qué sirve y cómo se
  usa.
- Las rutas por defecto salen de `tools/project.py` (relativas a la raíz del
  repo), así que las herramientas funcionan desde cualquier carpeta.
- Lo que se pueda probar sin el juego se prueba con DOLs sintéticos
  (`tests/synthetic.py`, cuyas codificaciones se comparan con capstone).

### C++ (runtime nativo, `runtime/`)

- C++20, compilado con Clang (el código traducido está escrito para Clang).
- Formato con `clang-format` 18 (`runtime/.clang-format`) y nombres revisados
  con `clang-tidy` 18 (`runtime/.clang-tidy`). Antes de hacer commit, con
  `build/native/runtime` configurado con `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`:
  ```sh
  clang-format -i runtime/src/*.cpp runtime/src/*.h runtime/src/*/*.cpp runtime/src/*/*.h
  run-clang-tidy -p build/native/runtime 'runtime/src/.*\.cpp$'
  ```
- `snake_case` para funciones, variables y namespaces; `PascalCase` para
  tipos; `UPPER_SNAKE_CASE` para constantes, valores de enum y macros. Los
  miembros privados de una clase terminan en `_`.
- Los nombres que vienen de fuera se escriben como allá: los de la ABI de
  WiiCompiled, que el código traducido llama por su nombre (`PPC_ReadSpr`,
  `Memory::GetPointer`), y los reemplazos nativos, que se llaman como la
  función del juego que reemplazan más `Native` (`OSLoadContextNative`), para
  que una búsqueda encuentre las dos.
- El código generado **no se edita a mano**: si algo está mal, se corrige el
  traductor, los símbolos o un parche.

### Estructura del repositorio

Sigue las convenciones de [dtk-template](https://github.com/encounter/dtk-template)
(decomps de GameCube/Wii) y de N64Recomp (recompilaciones):

| Carpeta | Contenido |
|---|---|
| `orig/G4AEE9/` | Tu copia extraída del disco. Nunca se sube. |
| `config/G4AEE9/` | `symbols.txt` en la sintaxis de decomp-toolkit: la única fuente de verdad para nombres y tamaños. |
| `tools/` | Herramientas de análisis. |
| `tests/` | Pruebas. |
| `docs/` | Documentación bilingüe. |
| `runtime/` | El runtime nativo (fase 2): el código del juego, traducido, corre sobre él sin Dolphin. |
| `patches/` | Parches opcionales para componentes de terceros (ModernGekko). |

### Datos del juego

Nunca se suben al repositorio: ni el DOL, ni assets extraídos, ni el código
que generen el recompilador o el traductor (se deriva del código del juego).
Como en N64Recomp, todo eso se genera en tu máquina a partir de tu propia
copia.

### Commits

- Un cambio lógico por commit.
- Asunto en inglés, en imperativo, de 72 caracteres o menos
  (`Add jump table detection to the function finder`).
- El cuerpo explica el porqué cuando no es obvio.

---

## English

The goal: any developer who opens a file in this repository understands what
it does and why, without having to ask anyone.

### Principles

1. **Self-explaining names.** `function_start`, `jump_table_address`,
   `exception_index_sizes`; never `fs`, `jt`, `tmp2`. The only abbreviations
   allowed are standard terms anyone can look up:
   - ISA fields as the IBM PowerPC 750CL manual names them
     (`rd`, `ra`, `rb`, `BO`, `LK`, `SIMM`);
   - well-known acronyms (`DOL`, `SDK`, `GX`, `BSS`, `CPU`, `SPR`).
2. **Comments explain why.** The code already says *what* it does; a comment
   gives the reason, the source or the pitfall. Good: "Switch case labels look
   like function pointers, so they are not used to bound tables". Bad: "add 4
   to the address".
3. **No magic numbers.** Every constant has a name and, if it comes from the
   hardware, the SDK or the game, where it came from
   (`SMALL_DATA_BASE = 0x80329080  # set by __init_registers`).
4. **Every claim about the game comes with its evidence.** Naming a function
   (`OSResetSystem`) requires writing down why in `docs/FUNCTION_MAP.md`.
   Anything unproven keeps its generic name (`fn_801736C0`).
5. **Every result is reproducible.** Any generated file
   (`config/G4AEE9/symbols.txt`) can be regenerated with a tool in the repo,
   and the docs give the command.

### Languages

- Identifiers, in-code comments and commit messages in **English**: it is
  the common language of the decomp/recomp scene and lets anyone contribute.
- Module headers and all documentation (`README.md`, `docs/`, this file) in
  **Spanish and English**.
- File names in English.

### Python (tools in `tools/`)

- Python 3.11 or newer, with type hints, `pathlib` and `argparse`.
- `ruff` for linting and formatting; configured in `pyproject.toml`. Before
  committing:
  ```sh
  python3 -m ruff format tools tests
  python3 -m ruff check tools tests
  python3 -m pytest
  ```
- Tests must not need game data. Those that do skip themselves unless
  `HM_MM_DOL` points to your `main.dol`.
- One script = one task, with a docstring saying what it is for and how to
  use it.
- Default paths come from `tools/project.py` (relative to the repository
  root), so the tools work from any folder.
- Whatever can be tested without the game is tested on synthetic DOLs
  (`tests/synthetic.py`, whose encodings are checked against capstone).

### C++ (native runtime, `runtime/`)

- C++20, compiled with Clang (the translated code is written for Clang).
- Formatting with `clang-format` 18 (`runtime/.clang-format`) and names
  checked with `clang-tidy` 18 (`runtime/.clang-tidy`). Before committing,
  with `build/native/runtime` configured with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`:
  ```sh
  clang-format -i runtime/src/*.cpp runtime/src/*.h runtime/src/*/*.cpp runtime/src/*/*.h
  run-clang-tidy -p build/native/runtime 'runtime/src/.*\.cpp$'
  ```
- `snake_case` for functions, variables and namespaces; `PascalCase` for
  types; `UPPER_SNAKE_CASE` for constants, enum values and macros. Private
  members of a class end in `_`.
- Names that come from elsewhere are spelled as they are there: those of
  WiiCompiled's ABI, which the translated code calls by name (`PPC_ReadSpr`,
  `Memory::GetPointer`), and the native replacements, named after the game
  function they replace plus `Native` (`OSLoadContextNative`), so that a search
  finds both.
- Generated code is **never edited by hand**: if something is wrong, fix the
  translator, the symbols or a patch.

### Repository layout

Follows the conventions of [dtk-template](https://github.com/encounter/dtk-template)
(GameCube/Wii decomps) and N64Recomp (recompilations):

| Folder | Contents |
|---|---|
| `orig/G4AEE9/` | Your extracted copy of the disc. Never committed. |
| `config/G4AEE9/` | `symbols.txt` in decomp-toolkit syntax: the single source of truth for names and sizes. |
| `tools/` | Analysis tools. |
| `tests/` | Tests. |
| `docs/` | Bilingual documentation. |
| `runtime/` | The native runtime (phase 2): the game's code, translated, runs on it without Dolphin. |
| `patches/` | Optional patches for third-party components (ModernGekko). |

### Game data

Never committed: not the DOL, not extracted assets, not the code the
recompiler or the translator generates (it is derived from the game's code).
As with N64Recomp, all of it is generated on your machine from your own copy.

### Commits

- One logical change per commit.
- English, imperative subject of 72 characters or fewer
  (`Add jump table detection to the function finder`).
- The body explains why when it is not obvious.
