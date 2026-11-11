// A stand-in for the translated game, which is made on each player's computer from their
// own disc (tools/prepare_game.py): the runtime kit (tools/make_runtime_kit.py) is built
// with it, so that the runtime links and its compile and link commands can be read. It is
// never shipped, and a program built with it stops at the start: it has no game.
//
// Un sustituto del juego traducido, que se hace en la computadora de cada jugador con su
// propio disco: el kit del runtime se compila con él para que el runtime enlace y se
// puedan leer sus comandos de compilación y de enlace. Nunca se entrega.

#include <cstdio>
#include <cstdlib>

extern "C" void InitializeDataSections() {
    std::fputs("this program was built with the stand-in of the game (runtime/kit/stub_game)\n", stderr);
    std::exit(EXIT_FAILURE);
}
