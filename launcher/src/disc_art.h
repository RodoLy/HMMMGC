// What the launcher shows of the player's own disc image: the game's code, and the art of its main
// tab, the logo of the game's title screen (the texture graphic/Menu/english/title_e.tpl). Nothing
// of the game comes with the launcher; without a disc image it can read (an .iso or .gcm of the
// game), there is no art.
//
// Lo que el launcher muestra de la imagen del disco del propio jugador: el código del juego y el
// arte de su pestaña principal, el logotipo de la pantalla de título. Nada del juego viene con el
// launcher; sin una imagen que pueda leer (una .iso o .gcm del juego), no hay arte.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace disc_art {

struct Picture {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;  // red, green, blue and alpha, a byte each, row after row
};

// The logo in `disc_image`, or nothing when it is not a GameCube disc image of the game that can
// be read as it is (a compressed one is read once it is prepared, from the image made then).
std::optional<Picture> title_logo(const std::filesystem::path& disc_image);

// The code of the game on `disc_image` ("G4AEE9"), when it is a GameCube disc image read as it is.
std::optional<std::string> game_code(const std::filesystem::path& disc_image);

// `picture` made `width` x `height`, each new pixel the average of the ones it covers.
Picture resized(const Picture& picture, int width, int height);

}  // namespace disc_art
