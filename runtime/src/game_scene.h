// What the game is showing, read from its memory once a frame: whether the player walks
// freely, and whether the place is outdoors.
//
// Lo que el juego está mostrando, leído de su memoria una vez por cuadro: si el jugador
// camina libre y si el lugar es al aire libre.
//
// The zoom is for walking around the farm and the town: the title screen (and the demo of
// the dog running behind it), the cutscenes and the rooms keep the game's own camera. The
// game runs its title screen and its cutscenes as events of its event manager, and builds
// each place from a map: the places outdoors are fields made of pieces, the rooms are one
// small piece.
#pragma once

namespace game_scene {

// Whether no event is running (no cutscene, no title screen) and the player is in a place
// outdoors.
bool player_walks_outdoors();

}  // namespace game_scene
