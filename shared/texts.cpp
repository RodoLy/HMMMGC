// See texts.h.

#include "texts.h"

#include <SDL3/SDL_locale.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

#include <array>
#include <cstdio>

namespace texts {
namespace {

struct Translation {
    Text which;
    const char* spanish;
    const char* english;
};

// One line per text; the order does not matter (checked when the program starts).
constexpr Translation TRANSLATIONS[] = {
    {Text::Subtitle, "versión nativa para PC", "native PC version"},
    {Text::Play, "Jugar", "Play"},
    {Text::Options, "Opciones", "Options"},
    {Text::Mods, "Mods", "Mods"},
    {Text::Controls, "Controles", "Controls"},
    {Text::Exit, "Salir", "Exit"},
    {Text::Back, "Volver", "Back"},
    {Text::Version, "Versión %s", "Version %s"},
    {Text::DiscLabel, "Disco: %s", "Disc: %s"},
    {Text::ChangeDisc, "Cambiar el disco", "Change the disc"},
    {Text::OpenLog, "Abrir el registro", "Open the log"},

    {Text::WelcomeTitle, "¡Bienvenido!", "Welcome!"},
    {Text::WelcomeBody,
     "Para jugar necesitas tu propia copia del juego: la imagen de tu disco de Harvest Moon: Magical Melody "
     "de Estados Unidos. La primera vez, el juego se prepara para tu computadora con esa imagen: tarda unos "
     "minutos y después ya no.",
     "To play you need your own copy of the game: the image of your disc of Harvest Moon: Magical Melody for "
     "the USA. The first time, the game is prepared for your computer from that image: it takes a few minutes, "
     "and then never again."},
    {Text::WelcomeFormats, "Sirven .iso, .gcm y .rvz (las de Dolphin).",
     "It can be .iso, .gcm or .rvz (Dolphin's)."},
    {Text::ChooseDisc, "Elegir la imagen del disco", "Choose the disc image"},
    {Text::DiscFilter, "Imágenes de disco de GameCube", "GameCube disc images"},
    {Text::UpdateTitle, "Hay una versión nueva", "There is a new version"},
    {Text::UpdateBody,
     "El port se actualizó, así que el juego se tiene que preparar otra vez con tu disco. Tarda unos minutos.",
     "The port was updated, so the game has to be prepared again from your disc. It takes a few minutes."},
    {Text::PrepareAgainTitle, "Falta preparar el juego", "The game is not prepared"},
    {Text::PrepareAgainBody, "El juego todavía no está listo en esta computadora. Se prepara con tu disco: %s",
     "The game is not ready on this computer yet. It is prepared from your disc: %s"},
    {Text::Prepare, "Preparar el juego", "Prepare the game"},
    {Text::PackageIncomplete,
     "Al paquete le falta una parte (%s). Vuelve a descargarlo y descomprímelo completo.",
     "A part of the package is missing (%s). Download it again and unzip all of it."},

    {Text::PreparingTitle, "Preparando el juego", "Preparing the game"},
    {Text::PreparingBody, "Solo esta vez: el juego se hace con tu disco, en tu computadora.",
     "Only this once: the game is made from your disc, on your computer."},
    {Text::StageDisc, "Leer el disco", "Reading the disc"},
    {Text::StageTranslate, "Traducir el código del juego", "Translating the game's code"},
    {Text::StageAdapt, "Ajustar la granja y los mods", "Adjusting the farm and the mods"},
    {Text::StageCompile, "Compilar", "Compiling"},
    {Text::StageLink, "Armar el programa", "Putting the program together"},
    {Text::StageFinish, "Terminar", "Finishing"},
    {Text::StageCount, "%d de %d", "%d of %d"},
    {Text::Elapsed, "Tiempo: %d:%02d", "Time: %d:%02d"},
    {Text::ShowDetails, "Ver detalles", "Show details"},
    {Text::HideDetails, "Ocultar detalles", "Hide details"},
    {Text::Cancel, "Cancelar", "Cancel"},
    {Text::Cancelled, "Se canceló la preparación.", "The preparation was cancelled."},
    {Text::Prepared, "¡Listo! El juego ya está preparado.", "Done! The game is ready."},
    {Text::TryAgain, "Intentar otra vez", "Try again"},
    {Text::ChooseOther, "Elegir otra imagen", "Choose another image"},

    {Text::ErrorNotADisc, "Ese archivo no es una imagen de disco de GameCube.",
     "That file is not a GameCube disc image."},
    {Text::ErrorCompressed,
     "Esa imagen está comprimida y no se pudo convertir. Conviértela a ISO con Dolphin (clic derecho en el "
     "juego > Convertir archivo) y elige la ISO.",
     "That image is compressed and could not be converted. Convert it to ISO with Dolphin (right click on the "
     "game > Convert File) and choose the ISO."},
    {Text::ErrorWrongGame, "Ese disco es de otro juego.", "That disc is another game."},
    {Text::ErrorWrongVersion,
     "Ese disco es de otra versión del juego. El port es para la de Estados Unidos (G4AEE9).",
     "That disc is another version of the game. The port is for the USA one (G4AEE9)."},
    {Text::ErrorModifiedGame, "Ese disco tiene el juego modificado: su programa no es el original.",
     "That disc holds a modified game: its program is not the original one."},
    {Text::ErrorToolFailed, "Algo falló al preparar el juego. Los detalles están en el registro.",
     "Something failed while preparing the game. The details are in the log."},
    {Text::ErrorSetup, "Al paquete le falta algo para preparar el juego. Vuelve a descargarlo.",
     "The package lacks something it needs to prepare the game. Download it again."},
    {Text::ErrorNotStarted, "No se pudo empezar la preparación.", "The preparation could not start."},
    {Text::GameNotStarted, "No se pudo abrir el juego.", "The game could not be opened."},
    {Text::GameFailed, "El juego se cerró por un error. Los detalles están en el registro.",
     "The game closed because of an error. The details are in the log."},

    {Text::PictureSection, "Imagen", "Picture"},
    {Text::GameSection, "Juego", "Game"},
    {Text::SoundSection, "Sonido", "Sound"},
    {Text::LanguageSection, "Idioma", "Language"},
    {Text::Resolution, "Resolución interna", "Internal resolution"},
    {Text::ResolutionWindow, "Como la ventana", "As the window"},
    {Text::ResolutionHint, "Más resolución se ve más nítido y pide más a la tarjeta gráfica.",
     "More resolution looks sharper and asks more of the graphics card."},
    {Text::Fullscreen, "Pantalla completa", "Full screen"},
    {Text::FullscreenHint, "En el juego, Alt+Enter cambia entre ventana y pantalla completa.",
     "In the game, Alt+Enter switches between a window and full screen."},
    {Text::Widescreen, "Pantalla ancha", "Widescreen"},
    {Text::WidescreenHint, "El 3D muestra más a los lados; los menús y los textos se estiran.",
     "The 3D shows more at the sides; the menus and the texts are stretched."},
    {Text::FrameRate, "Cuadros por segundo", "Frames per second"},
    {Text::FrameRateGame, "30 (como el juego)", "30 (as the game)"},
    {Text::FrameRateMade, "%d (con cuadros intermedios)", "%d (frames made in between)"},
    {Text::Zoom, "Zoom de la cámara al aire libre", "Camera zoom outdoors"},
    {Text::ZoomHint, "100% es el del juego; con menos se ve más alrededor del personaje.",
     "100% is the game's; less shows more around the character."},
    {Text::GameSpeed, "Velocidad de todo el juego", "Speed of the whole game"},
    {Text::GameSpeedHint, "100% es la normal. En el juego, mantén Tab para el turbo.",
     "100% is the normal one. In the game, hold Tab for turbo."},
    {Text::Volume, "Volumen", "Volume"},
    {Text::VolumeHint, "La música y los sonidos del juego; 0% lo silencia.",
     "The game's music and sounds; 0% mutes it."},
    {Text::LanguageAutomatic, "Como el sistema", "As the system"},

    {Text::ModsIntro, "Activa o desactiva los mods y elige sus ajustes. Se aplican al jugar.",
     "Turn the mods on or off and choose their settings. They apply when you play."},
    {Text::ModOn, "Activado", "On"},
    {Text::ModOff, "Desactivado", "Off"},
    {Text::ModVersion, "Versión %s", "Version %s"},
    {Text::ModNoSettings, "Este mod no tiene ajustes.", "This mod has no settings."},
    {Text::ModsIncluded, "Los mods vienen con el port.", "The mods come with the port."},

    {Text::ControlsIntro,
     "Juega con el primer control que conectes, con la asignación automática de abajo. Sin control, se "
     "juega con el teclado.",
     "The first controller you connect plays, with the automatic mapping below. Without a controller, the "
     "keyboard plays."},
    {Text::Keyboard, "Teclado", "Keyboard"},
    {Text::NoGamepads, "No hay controles conectados.", "No controllers are connected."},
    {Text::AutomaticMapping, "Asignación automática", "Automatic mapping"},
    {Text::TryButtons, "Presiona botones para probarlo.", "Press buttons to try it."},
    {Text::GameCubeButton, "GameCube", "GameCube"},
    {Text::YourButton, "Tu control", "Your controller"},
    {Text::ControlStick, "Palanca", "Control Stick"},
    {Text::CStick, "Palanca C", "C-Stick"},
    {Text::DPad, "Cruceta", "D-Pad"},
    {Text::LeftStick, "Palanca izquierda", "Left stick"},
    {Text::RightStick, "Palanca derecha", "Right stick"},
    {Text::ArrowKeys, "Flechas", "Arrow keys"},
};

constexpr size_t TEXT_COUNT = static_cast<size_t>(Text::Count);

struct Table {
    std::array<const char*, TEXT_COUNT> spanish{};
    std::array<const char*, TEXT_COUNT> english{};
};

const Table& table() {
    static const Table built = [] {
        Table result;
        for (const Translation& translation : TRANSLATIONS) {
            result.spanish[static_cast<size_t>(translation.which)] = translation.spanish;
            result.english[static_cast<size_t>(translation.which)] = translation.english;
        }
        for (size_t i = 0; i < TEXT_COUNT; ++i) {
            if (result.spanish[i] == nullptr || result.english[i] == nullptr) {
                SDL_Log("launcher: text %zu has no translation", i);
                result.spanish[i] = result.spanish[i] ? result.spanish[i] : "?";
                result.english[i] = result.english[i] ? result.english[i] : "?";
            }
        }
        return result;
    }();
    return built;
}

Language g_language = Language::English;

}  // namespace

Language system_language() {
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    Language found = Language::English;
    if (locales != nullptr && count > 0 && locales[0]->language != nullptr &&
        SDL_strcmp(locales[0]->language, "es") == 0) {
        found = Language::Spanish;
    }
    SDL_free(locales);
    return found;
}

void set_language(Language language) { g_language = language; }

Language language() { return g_language; }

const char* language_code() { return g_language == Language::Spanish ? "es" : "en"; }

const char* text(Text which) {
    const size_t index = static_cast<size_t>(which);
    return g_language == Language::Spanish ? table().spanish[index] : table().english[index];
}

std::string format(Text which, const char* value) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), text(which), value);
    return buffer;
}

std::string format(Text which, int value) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), text(which), value);
    return buffer;
}

std::string format(Text which, int first, int second) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), text(which), first, second);
    return buffer;
}

}  // namespace texts
