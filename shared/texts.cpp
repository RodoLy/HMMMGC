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
    {Text::ChangeDisc, "Cambiar el disco", "Change the disc"},
    {Text::OpenLog, "Abrir el registro", "Open the log"},

    {Text::GameTab, "Juego", "Game"},
    {Text::PanelPicture, "Imagen y sonido", "Picture and sound"},
    {Text::PanelModsOn, "%d de %d activados", "%d of %d on"},
    {Text::PanelDevices, "Dispositivos", "Devices"},
    {Text::DiscBox, "Disco", "Disc"},
    {Text::StatusReady, "Listo", "Ready"},
    {Text::StatusNotPrepared, "Sin preparar", "Not prepared"},
    {Text::StatusPreparing, "Preparando", "Preparing"},
    {Text::StatusFailed, "Falló", "Failed"},
    {Text::VersionBox, "versión", "version"},
    {Text::PlayHint, "Abre el juego con tu disco. En el juego, F10 o Select abren las opciones.",
     "Opens the game from your disc. In the game, F10 or Select open the options."},
    {Text::PrepareHint,
     "El juego todavía no está listo en esta computadora. Se prepara con tu disco y tarda unos minutos.",
     "The game is not ready on this computer yet. It is prepared from your disc and takes a few minutes."},
    {Text::ChooseDiscHint,
     "Elige la imagen de tu disco (el de Estados Unidos): .iso, .gcm o .rvz. También puedes soltarla aquí.",
     "Choose the image of your disc (the USA one): .iso, .gcm or .rvz. You can also drop it here."},
    {Text::PreparingRow, "Preparando el juego…", "Preparing the game…"},
    {Text::PreparingStage, "Preparando el juego… %d de %d: ", "Preparing the game… %d of %d: "},
    {Text::ChangeDiscHint, "Elige otra imagen del disco. Sirven .iso, .gcm y .rvz (las de Dolphin).",
     "Choose another disc image. It can be .iso, .gcm or .rvz (Dolphin's)."},
    {Text::OpenLogHint, "Abre el registro con los detalles de la preparación y de los errores.",
     "Opens the log with the details of the preparation and of any errors."},
    {Text::NoLog, "Todavía no hay registro.", "There is no log yet."},
    {Text::ExitHint, "Cierra el launcher.", "Closes the launcher."},
    {Text::Opening, "Abriendo el juego…", "Opening the game…"},

    {Text::DiscFilter, "Imágenes de disco de GameCube", "GameCube disc images"},
    {Text::UpdateBody,
     "El port se actualizó, así que el juego se tiene que preparar otra vez con tu disco. Tarda unos minutos.",
     "The port was updated, so the game has to be prepared again from your disc. It takes a few minutes."},
    {Text::Prepare, "Preparar el juego", "Prepare the game"},
    {Text::PackageIncomplete,
     "Al paquete le falta una parte (%s). Vuelve a descargarlo y descomprímelo completo.",
     "A part of the package is missing (%s). Download it again and unzip all of it."},

    {Text::StageDisc, "Leer el disco", "Reading the disc"},
    {Text::StageTranslate, "Traducir el código del juego", "Translating the game's code"},
    {Text::StageAdapt, "Ajustar la granja y los mods", "Adjusting the farm and the mods"},
    {Text::StageCompile, "Compilar", "Compiling"},
    {Text::StageLink, "Armar el programa", "Putting the program together"},
    {Text::StageFinish, "Terminar", "Finishing"},
    {Text::StageCount, "%d de %d", "%d of %d"},
    {Text::Cancel, "Cancelar", "Cancel"},
    {Text::Cancelled, "Se canceló la preparación.", "The preparation was cancelled."},
    {Text::Prepared, "¡Listo! El juego ya está preparado.", "Done! The game is ready."},
    {Text::TryAgain, "Intentar otra vez", "Try again"},

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
    {Text::FrameRateHint, "30 es como el juego; con más, se hacen cuadros intermedios.",
     "30 is as the game; higher makes frames in between."},
    {Text::ShowFrameRate, "Mostrar los cuadros por segundo", "Show the frames per second"},
    {Text::ShowFrameRateHint, "Arriba a la derecha, sobre el juego.", "At the top right, over the game."},
    {Text::FrameRateCounter, "%d FPS", "%d FPS"},
    {Text::Zoom, "Alejar la cámara al aire libre", "Pull the camera back outdoors"},
    {Text::ZoomHint, "0% es la cámara del juego; con más se ve más alrededor del personaje.",
     "0% is the game's camera; more shows more around the character."},
    {Text::Volume, "Volumen", "Volume"},
    {Text::VolumeHint, "La música y los sonidos del juego; 0% lo silencia.",
     "The game's music and sounds; 0% mutes it."},
    {Text::ZoomBeyondStart,
     "Con este zoom, los cultivos y objetos más lejanos aparecen cuando te acercas a ellos, hasta que vuelvas a "
     "abrir el juego.",
     "With this zoom, the farthest crops and items appear as you walk up to them, until you open the game again."},

    {Text::BackToGame, "Volver al juego", "Back to the game"},
    {Text::QuitGame, "Salir del juego", "Quit the game"},
    {Text::QuitQuestion, "¿Salir del juego? Lo que no hayas guardado en el juego se pierde.",
     "Quit the game? Whatever you have not saved in the game is lost."},
    {Text::QuitYes, "Sí, salir", "Yes, quit"},
    {Text::QuitNo, "No, seguir jugando", "No, keep playing"},
    {Text::MenuOpenNote, "Menú abierto: el juego no recibe los controles. F10 o Select para volver.",
     "Menu open: the game gets no input. F10 or Select to go back."},
    {Text::GameShortcuts,
     "En el juego: F10 o el botón Select abren las opciones, Tab acelera (turbo) y Alt+Enter cambia a pantalla "
     "completa.",
     "In the game: F10 or the Select button open the options, Tab speeds up (turbo) and Alt+Enter switches to full "
     "screen."},

    {Text::ModOn, "Activado", "On"},
    {Text::ModOff, "Desactivado", "Off"},
    {Text::ModNoSettings, "Este mod no tiene ajustes.", "This mod has no settings."},
    {Text::NoManifest, "sin manifiesto", "no manifest"},
    {Text::NoManifestHint, "Mod sin manifiesto: aparece solo con su id.",
     "A mod without a manifest: it is listed by its id alone."},
    {Text::NoDescription, "Este mod no trae descripción.", "This mod comes without a description."},

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
    {Text::InUse, "En uso", "In use"},
    {Text::NotInUse, "Sin usar", "Not in use"},
    {Text::Controller, "Control", "Controller"},
    {Text::NotConnected, "No conectado", "Not connected"},
    {Text::KeyboardHint, "Sin control, se juega con el teclado. Las flechas mueven al personaje.",
     "Without a controller, the keyboard plays. The arrow keys move the character."},
    {Text::ControllerNoneHint, "No hay controles conectados. Juega el primero que conectes.",
     "No controllers are connected. The first one you connect plays."},
    {Text::ControllerHint, "Juega el primer control que conectaste. Presiona sus botones para probarlo.",
     "The first controller you connected plays. Press its buttons to try it."},
    {Text::PressedPart, "%s de GameCube: %s en tu control.", "GameCube's %s: %s on your controller."},
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
                SDL_Log("texts: text %zu has no translation", i);
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

std::string format(Text which, const char* first, const char* second) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), text(which), first, second);
    return buffer;
}

}  // namespace texts
