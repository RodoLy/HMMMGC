# Patches / Parches

[Español](#español) · [English](#english)

---

## Español

Parches opcionales para componentes de terceros que usa este proyecto.
`scripts/build_and_run_linux.sh` usa ModernGekko tal cual; nada de aquí se
aplica solo.

### `moderngekko/lockstep-verifier.patch`

Mejora el verificador lockstep que ya trae ModernGekko: mientras se juega,
compara cada bloque de código recompilado con el intérprete de CPU de Dolphin
(cómo funciona y qué resultados da: [docs/VALIDATION.md](../docs/VALIDATION.md)).
Agrega estas variables de entorno, que se usan junto a `STATICRECOMP_LOCKSTEP=1`:

| Variable | Qué hace |
|---|---|
| `STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` | El intérprete sigue más allá de la primera vez que llega a la dirección donde terminó el bloque recompilado, y el bloque pasa si el estado recompilado es igual al del intérprete en *cualquier* llegada. Así desaparecen las diferencias que solo vienen de detenerse en iteraciones distintas de un mismo bucle. Cada bloque que pasó en una llegada posterior sale como `[lockstep] LATE-MATCH` y se cuenta en `late_matches`. |
| `STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1` | No compara los bits FPRF, FR y FI del FPSCR (máscara `0x0007F000`), que cada instrucción de punto flotante reescribe y el juego no lee. |
| `STATICRECOMP_LOCKSTEP_COVERAGE_FILE=ruta` | Al cerrar el juego escribe la dirección de cada instrucción que se comparó y salió igual; `tools/lockstep_coverage.py` lo convierte en porcentajes. |
| `STATICRECOMP_LOCKSTEP_INJECT_FAULT=0xDIRECCIÓN` | Control negativo del propio verificador: cambia un bit de `r3` en el estado final recompilado del bloque que empieza en esa dirección. Si el verificador no lo reporta, no sirve. |

**Versiones a las que se aplica:** ModernGekko `edd460a`, `vendor/dolphin`
(RecompCore) `10f7ac4`, las que fija `scripts/build_and_run_linux.sh` con
ModernGekko-Template `c2553a9`.

**Licencia:** el parche modifica archivos de Dolphin, que son
GPL-2.0-or-later, y se distribuye con esa misma licencia.

**Aplicar** (un minuto de compilación; el resto de ModernGekko no se vuelve a
compilar):

```bash
TEMPLATE=~/ModernGekko-Template   # o tu MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/lockstep-verifier.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Quitar:** el mismo `git apply` con `-R` y otra vez `ninja`.

---

## English

Optional patches for third-party components this project uses.
`scripts/build_and_run_linux.sh` uses ModernGekko as it is; nothing here
applies itself.

### `moderngekko/lockstep-verifier.patch`

Improves the lockstep verifier that ModernGekko already ships: while the game
plays, it compares every block of recompiled code with Dolphin's CPU
interpreter (how it works and what it found:
[docs/VALIDATION.md](../docs/VALIDATION.md)). It adds these environment
variables, used together with `STATICRECOMP_LOCKSTEP=1`:

| Variable | What it does |
|---|---|
| `STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1` | The interpreter runs on past the first time it reaches the address where the recompiled block ended, and the block passes if the recompiled state equals the interpreter's state at *any* arrival. That removes the differences that only come from stopping on different iterations of the same loop. Each block that passed at a later arrival is printed as `[lockstep] LATE-MATCH` and counted in `late_matches`. |
| `STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS=1` | Does not compare the FPSCR bits FPRF, FR and FI (mask `0x0007F000`), which every floating-point instruction rewrites and the game does not read. |
| `STATICRECOMP_LOCKSTEP_COVERAGE_FILE=path` | When the game closes, writes the address of every instruction that was compared and came out equal; `tools/lockstep_coverage.py` turns it into percentages. |
| `STATICRECOMP_LOCKSTEP_INJECT_FAULT=0xADDRESS` | A negative control for the verifier itself: flips a bit of `r3` in the recompiled end state of the block that starts at that address. If the verifier does not report it, it is worthless. |

**Applies to:** ModernGekko `edd460a`, `vendor/dolphin` (RecompCore)
`10f7ac4`, the versions `scripts/build_and_run_linux.sh` pins through
ModernGekko-Template `c2553a9`.

**License:** the patch modifies Dolphin files, which are GPL-2.0-or-later,
and is distributed under the same license.

**Apply** (a minute of compiling; the rest of ModernGekko is not rebuilt):

```bash
TEMPLATE=~/ModernGekko-Template   # or your MODERNGEKKO_TEMPLATE
git -C "$TEMPLATE/lib/ModernGekko/vendor/dolphin" apply "$PWD/patches/moderngekko/lockstep-verifier.patch"
ninja -C "$TEMPLATE/lib/ModernGekko/build" moderngekko-run
```

**Remove:** the same `git apply` with `-R`, then `ninja` again.
