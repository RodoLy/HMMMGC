// The language of the game's text. The American disc has the text of the five European languages
// (the folders message_e, message_s, message_f, message_g and message_i hold the same files, with
// the same messages), but its program only knows the paths of the English ones. For another
// language, those paths are changed in the game's memory before it starts, so that the game opens
// the files of that language by itself.
//
// El idioma de los textos del juego. El disco americano trae los textos de los cinco idiomas
// europeos, pero su programa solo conoce las rutas de los de inglés. Para otro idioma, esas rutas
// se cambian en la memoria del juego antes de que arranque.
//
// What the game draws from its own program (the names of the items, the menus) and its pictures
// with words stay in English: the disc has them in no other language.
#pragma once

#include "game_disc.h"

#include <string>

namespace game_language {

// `language`: "en", "es", "fr", "de" or "it" (settings::GAME_LANGUAGES). Call it with the game's
// data in memory and before the game runs. A file the disc lacks in that language stays in English.
void apply(const GameDisc& disc, const std::string& language);

}  // namespace game_language
