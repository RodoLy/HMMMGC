# Recompiler evaluation / Evaluación de recompiladores

Reviewed on 2026-11-03 / Revisado el 2026-11-03.

[Español](#español) · [English](#english)

---

## Español

### Qué se decide

Qué herramienta traduce el código PowerPC del juego a C (el recompilador de
CPU) y qué la ejecuta en PC (el runtime: video, audio, controles, disco).

### Candidatos

| | DolRecomp + ModernGekko | NWiiRecomp | Recompilador propio |
|---|---|---|---|
| Revisado | DolRecomp `02e27ac` (10 oct 2026), ModernGekko-Template `c2553a9` (16 sep 2026) | `595f176` (7 ago 2026) | — |
| Licencia | GPL-3.0 (ModernGekko: GPL-3.0-or-later, deriva de Dolphin) | Propia: solo uso no comercial, derivados con términos igual de restrictivos, crédito obligatorio | La que elijas |
| CPU | C11, backends C y LLVM, ~44 000 líneas | C++, ~22 000 líneas (con runtime) | Por escribir |
| Pruebas | 30+ suites (opcodes, FPSCR, paired singles, tablas de saltos, comparación con Dolphin), CI en Linux/Windows/macOS | Su CMake no define pruebas | Por escribir |
| Runtime | ModernGekko: núcleo derivado de Dolphin para video, audio y HLE | Propio: rasterizador, HLE e interfaz de hardware; GX todavía en "seguimiento de estructuras" | Por escribir (GX es lo más largo) |
| Encaja con nuestro mapa | Sí: `--map` acepta `dirección tamaño nombre` y usa cada dirección como entrada y partición | Tiene su propio analizador | Sí, por diseño |
| Mods y parches | Parches por función en C con verificación FNV-64 del código original; reemplazo de funciones por código nativo | Hooks HLE por juego en TOML | Por diseñar |
| Estado público | El README de ModernGekko lista recomps de Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii y 007 Agent Under Fire (no verifiqué qué tan completos están) | Juegos que llegan a intros (Mario Party 7) | — |

### Recomendación: DolRecomp + ModernGekko

1. **Es el que mejor resuelve la parte difícil.** El runtime basado en el
   núcleo de Dolphin trae video (con resolución interna, o sea 4K), audio y
   HLE ya resueltos. Eso elimina la parte de meses del plan: escribir un
   renderizador de GX desde cero.
2. **Nuestro trabajo encaja directo.** `tools/export_dolrecomp_map.py`
   convierte `symbols.txt` al formato de `--map`. Con eso DolRecomp usa
   nuestras 4 486 funciones validadas y los nombres del SDK.
3. **Las metas del juego son parches, que es lo que mejor soporta.** El mod
   de Jamie, la velocidad y la fricción del personaje, los cheats y el
   anclaje del HUD en widescreen son reemplazos de funciones en C. DolRecomp
   verifica con un hash que el código original sea el esperado antes de
   aplicar cada parche.
4. **La licencia es la estándar de la escena.** GPL-3.0 es la de
   Zelda64Recomp; no impide nada a un proyecto de fans. La de NWiiRecomp
   obligaría a tu proyecto a heredar sus restricciones.
5. **Está bien hecho.** Pruebas, CI en tres sistemas, guía de contribución.
   Coincide con el estándar que pedimos para nuestro código.

Riesgos:

- Todas las opciones son de 2026. ModernGekko ya tiene una lista de juegos
  recompilados con él, pero **falta probar que Magical Melody arranque con
  este stack.**
- El proyecto quedaría bajo GPL-3.0.
- ModernGekko descarga varios cientos de MB de dependencias de Dolphin.
- Los controles se configuran con un `.ini` de Dolphin a mano (no tiene UI).

### Lo que falta

En este entorno de trabajo no se puede compilar ni ejecutar código
descargado, así que la prueba se hace en tu máquina:

La extracción de DolRecomp lee ISO y WBFS, no RVZ, así que primero extrae
con la herramienta de este repo dentro de la carpeta que usa la plantilla:

```sh
git clone --recurse-submodules https://github.com/ExpansionPak/ModernGekko-Template.git
python3 hm-mm-recomp/tools/extract_disc.py Harvest_Moon_-_Magical_Melody__USA_.rvz \
    ModernGekko-Template/extracted/Harvest-Moon-Magical-Melody
cd ModernGekko-Template
make check
make run GAME=Harvest-Moon-Magical-Melody
```

(La otra opción es convertir el RVZ a ISO en Dolphin, con clic derecho →
*Convert File…* → ISO, y usar `make run ISO=archivo.iso`.)

**En Windows, usar WSL2 con Ubuntu.** Los README se contradicen: el de
ModernGekko-Template pide Visual Studio (MSVC), pero el de DolRecomp, más
reciente, dice que ModernGekko no compila con MSVC por ahora. La ruta de
Linux es la mejor documentada (lista exacta de paquetes `apt`) y WSL2 la da
también en Windows ARM64, con ventanas gráficas vía WSLg.

Resultado esperado de la prueba, de menos a más: compila el C generado →
arranca y llega a `main` → muestra el logo de Natsume → título → jugable.
Cualquier punto de ese camino sirve: dice dónde empezar.

---

## English

### What is being decided

Which tool translates the game's PowerPC code to C (the CPU recompiler) and
which runs it on PC (the runtime: video, audio, input, disc).

### Candidates

| | DolRecomp + ModernGekko | NWiiRecomp | Our own recompiler |
|---|---|---|---|
| Reviewed | DolRecomp `02e27ac` (2026-10-10), ModernGekko-Template `c2553a9` (2026-09-16) | `595f176` (2026-08-07) | — |
| License | GPL-3.0 (ModernGekko: GPL-3.0-or-later, Dolphin-derived) | Custom: non-commercial only, derivatives must carry equally restrictive terms, mandatory credit | Whichever we choose |
| CPU | C11, C and LLVM backends, ~44,000 lines | C++, ~22,000 lines (runtime included) | To be written |
| Tests | 30+ suites (opcodes, FPSCR, paired singles, jump tables, cross-checks against Dolphin), CI on Linux/Windows/macOS | Its CMake defines no tests | To be written |
| Runtime | ModernGekko: Dolphin-derived core for video, audio and HLE | Its own rasterizer, HLE and hardware interface; GX still at "structure tracking" | To be written (GX is the longest part) |
| Fits our map | Yes: `--map` takes "address size name" lines and uses every address as an entry and partition point | Has its own analyzer | Yes, by design |
| Mods and patches | Per-function C patches verified with an FNV-64 hash of the original code; native function replacements | Per-game HLE hooks in TOML | To be designed |
| Public status | ModernGekko's README lists recomps of Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii and 007 Agent Under Fire (how complete they are was not checked) | Games that reach their intros (Mario Party 7) | — |

### Recommendation: DolRecomp + ModernGekko

1. The Dolphin-derived runtime solves video (internal resolution, i.e. 4K),
   audio and HLE, which removes the months-long part of the plan: writing a
   GX renderer from scratch.
2. Our work plugs straight in: `tools/export_dolrecomp_map.py` turns
   `symbols.txt` into `--map` input, so DolRecomp uses our 4,486 validated
   functions and the SDK names.
3. The game's goals (Jamie mod, player speed and friction, cheats,
   widescreen HUD anchoring) are function replacements in C, which DolRecomp
   checks against a hash of the original code before applying.
4. GPL-3.0 is the scene's standard (Zelda64Recomp uses it); NWiiRecomp's
   license would force its restrictions onto this project.
5. It is well engineered (tests, CI, contribution guide), matching the
   standard this repository holds itself to.

Risks: every option dates from 2026. ModernGekko's README lists recomps of
Super Mario Sunshine, Smash Bros. Brawl, Melee, Luigi's Mansion, Kirby Wii
and 007 Agent Under Fire (how complete they are was not checked), but
**Magical Melody still has to be shown to boot on this stack**; the project
would be GPL-3.0; ModernGekko pulls several hundred MB of Dolphin
dependencies; input is configured through a hand-written Dolphin `.ini`.

### What is left

Downloaded code cannot be built or run in the environment this analysis was
done in, so the test happens on your machine with the commands in the
Spanish section. DolRecomp's extractor reads ISO and WBFS but not RVZ, so
the disc is first extracted with this repository's `tools/extract_disc.py`
into the template's `extracted/<name>/` folder (or converted to ISO in
Dolphin). On Windows, use WSL2 with Ubuntu: the template's README asks for
MSVC, but DolRecomp's newer README says ModernGekko does not build under
MSVC yet, and the Linux path is the best documented one (it also works on
Windows ARM64, with GUI windows through WSLg). Expected milestones, from
least to most: the generated C
compiles → boots and reaches `main` → Natsume logo → title screen →
playable. Wherever it stops tells us where to start.
