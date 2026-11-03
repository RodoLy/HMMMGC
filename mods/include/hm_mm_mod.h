/*
 * Shared definitions for the Harvest Moon: Magical Melody mods.
 * Definiciones comunes de los mods de Harvest Moon: Magical Melody.
 *
 * A mod is a shared library that ModernGekko loads at start-up. In a
 * ModernGekkoModDesc it declares which game functions it patches (replaces)
 * or hooks (observes), and it exports that descriptor as moderngekko_get_mod().
 *
 * Un mod es una biblioteca compartida que ModernGekko carga al arrancar. En
 * un ModernGekkoModDesc declara qué funciones del juego parcha (reemplaza) o
 * engancha (observa), y exporta ese descriptor como moderngekko_get_mod().
 */

#ifndef HM_MM_MOD_H
#define HM_MM_MOD_H

#include <stdint.h>
#include <stdio.h>

/* ModernGekko's mod ABI; its include/ folder is on the include path. */
#include "moderngekko/mod_abi.h"

/*
 * Guest address of every function of the game, as DOLRECOMP_SYMBOL_<name>
 * (and its size as DOLRECOMP_SYMBOL_SIZE_<name>). Written by
 * tools/export_mod_symbols.py from config/G4AEE9/symbols.txt.
 */
#include "G4AEE9_symbols.h"

/* Disc ID of the game these mods are written for (USA, revision 0). */
#define HM_MM_GAME_ID "G4AEE9"

/* Number of elements of a C array, for the num_* fields of a mod descriptor. */
#define HM_MM_ARRAY_COUNT(array) ((uint32_t)(sizeof(array) / sizeof((array)[0])))

/*
 * Prints one line, tagged with the mod's name, on the runner's standard
 * error. ModernGekko's mod ABI has no logging call, so this is how a mod
 * shows that it ran. It needs at least a format string: HM_MM_LOG(id, "text").
 */
#define HM_MM_LOG(mod_name, ...)            \
    do {                                    \
        fprintf(stderr, "[" mod_name "] "); \
        fprintf(stderr, __VA_ARGS__);       \
        fputc('\n', stderr);                \
    } while (0)

#endif /* HM_MM_MOD_H */
