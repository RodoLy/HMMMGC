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
    {Text::SubtitleAndroid, "versión nativa para Android", "native Android version"},
    {Text::Play, "Jugar", "Play"},
    {Text::Options, "Opciones", "Options"},
    {Text::Mods, "Mods", "Mods"},
    {Text::Cheats, "Trucos", "Cheats"},
    {Text::Controls, "Controles", "Controls"},
    {Text::Exit, "Salir", "Exit"},
    {Text::ChangeDisc, "Cambiar el disco", "Change the disc"},
    {Text::OpenLog, "Ver el registro", "See the log"},

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
    {Text::PlayHint, "Abre el juego con tu disco. Dentro del juego: F10 abre las opciones y los mods, y Tab lo acelera.",
     "Opens the game with your disc. Inside the game: F10 opens the options and the mods, and Tab speeds it up."},
    {Text::PrepareHint,
     "El juego todavía no está listo en esta computadora. Se prepara con tu disco y tarda unos minutos.",
     "The game is not ready on this computer yet. It is prepared from your disc and takes a few minutes."},
    {Text::ChooseDiscHint,
     "Elige la imagen de tu disco (el de Estados Unidos): .iso, .gcm o .rvz. También puedes soltarla aquí, o "
     "dejarla en Descargas, Documentos o el Escritorio y el launcher la encuentra solo.",
     "Choose the image of your disc (the USA one): .iso, .gcm or .rvz. You can also drop it here, or leave it "
     "in Downloads, Documents or on the Desktop and the launcher finds it by itself."},
    {Text::PreparingRow, "Preparando el juego…", "Preparing the game…"},
    {Text::PreparingStage, "Preparando el juego… %d de %d: ", "Preparing the game… %d of %d: "},
    {Text::ChangeDiscHint, "Elige otra imagen del disco. Sirven .iso, .gcm y .rvz (las de Dolphin).",
     "Choose another disc image. It can be .iso, .gcm or .rvz (Dolphin's)."},
    {Text::FoundDisc, "Usar %s", "Use %s"},
    {Text::FoundDiscHint, "Encontrada en %s. Prepara el juego con esta imagen del disco.",
     "Found in %s. Prepares the game with this disc image."},
    {Text::OpenLogHint, "Abre el registro con los detalles de la preparación y de los errores.",
     "Opens the log with the details of the preparation and of any errors."},
    {Text::NoLog, "Todavía no hay registro.", "There is no log yet."},
    {Text::ExitHint, "Cierra el launcher.", "Closes the launcher."},
    {Text::Opening, "Abriendo el juego…", "Opening the game…"},

    {Text::DiscFilter, "Imágenes de disco de GameCube", "GameCube disc images"},
    {Text::UpdateBody,
     "El port se actualizó, así que el juego se tiene que preparar otra vez con tu disco. Tarda unos minutos.",
     "The port was updated, so the game has to be prepared again from your disc. It takes a few minutes."},
    {Text::LauncherUpdate, "Actualizar a %s", "Update to %s"},
    {Text::LauncherUpdateHint,
     "Baja la versión %s de GitHub (%s MB) y la pone sobre esta; el launcher se vuelve a abrir solo. Tus partidas, "
     "opciones y controles se quedan; después el juego se prepara otra vez con tu disco.",
     "Downloads version %s from GitHub (%s MB) and puts it over this one; the launcher opens again by itself. Your "
     "saves, options and controls stay; then the game is prepared again from your disc."},
    {Text::LauncherUpdating, "Actualizando a %s…", "Updating to %s…"},
    {Text::LauncherUpdateFailed, "No se pudo actualizar (%s). Revisa tu conexión y vuelve a intentarlo.",
     "The update did not work (%s). Check your connection and try again."},
    {Text::LauncherUpdateBusy, "Espera a que termine lo que está haciendo el launcher, o cierra el juego.",
     "Wait until the launcher finishes what it is doing, or close the game."},
    {Text::LauncherUpdateRestart, "La versión nueva ya está; ábrela otra vez.",
     "The new version is in place; open it again."},
    {Text::LauncherUpdatingHint, "Bajando la versión nueva; no cierres el launcher. Se vuelve a abrir solo al terminar.",
     "Downloading the new version; do not close the launcher. It opens again by itself when it is done."},
    {Text::UpdateNotesTitle, "Novedades de la %s", "What's new in %s"},
    {Text::UpdateNotesSize, "Tienes la %s · descarga de %s MB", "You have %s · a %s MB download"},
    {Text::UpdateNotesNone, "Esta versión no trae notas.", "This version has no notes."},
    {Text::UpdateNow, "Actualizar", "Update"},
    {Text::NotNow, "Ahora no", "Not now"},
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
    {Text::ErrorNoCompiler,
     "Tu Mac necesita las herramientas de línea de comandos de Apple para preparar el juego. Se abrió su "
     "instalador: instálalas y vuelve a preparar el juego.",
     "Your Mac needs Apple's Command Line Tools to prepare the game. Their installer has opened: install "
     "them and prepare the game again."},
    {Text::TouchOptions, "Opciones", "Options"},
    {Text::AndroidApp, "Hacer la app de Android", "Make the Android app"},
    {Text::AndroidAppHint,
     "Hace HarvestMoonMM.apk con tu disco, para instalarla en un celular o tablet con Android. Tarda unos "
     "minutos, como preparar el juego.",
     "Makes HarvestMoonMM.apk from your disc, to install it on an Android phone or tablet. It takes a few "
     "minutes, like preparing the game."},
    {Text::MakingAndroidApp, "Haciendo la app de Android", "Making the Android app"},
    {Text::MakingAndroidAppHint, "Elegir esta fila otra vez la detiene.", "Choosing this row again stops it."},
    {Text::AndroidAppReady,
     "La app está en %s. Cópiala al celular junto con la imagen .iso del disco, que está en %s.",
     "The app is in %s. Copy it to the phone together with the disc's .iso image, which is in %s."},
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
    {Text::FrameRate, "FPS", "FPS"},
    {Text::FrameRateGame, "30 (como el juego)", "30 (as the game)"},
    {Text::FrameRateMade, "%d (con cuadros intermedios)", "%d (frames made in between)"},
    {Text::FrameRateHint, "30 es como el juego; con más, se hacen cuadros intermedios.",
     "30 is as the game; higher makes frames in between."},
    {Text::ShowFrameRate, "Mostrar los FPS", "Show the FPS"},
    {Text::ShowFrameRateHint, "Arriba a la derecha, sobre el juego.", "At the top right, over the game."},
    {Text::HideTouchButtons, "Sin botones t\xC3\xA1" "ctiles con control", "No touch buttons with a controller"},
    {Text::HideTouchButtonsHint,
     "Mientras haya un control conectado no salen en la pantalla; si se desconecta, vuelven.",
     "While a controller is connected they stay off the screen; if it is disconnected, they come back."},
    {Text::HideTouchTurbo, "Sin bot\xC3\xB3n TURBO t\xC3\xA1" "ctil", "No TURBO touch button"},
    {Text::HideTouchTurboHint, "Quita TURBO de los botones t\xC3\xA1" "ctiles; Tab y el control siguen acelerando.",
     "Takes TURBO off the touch buttons; Tab and the controller still speed the game up."},
    {Text::CheatInfiniteStamina, "Estamina infinita", "Infinite stamina"},
    {Text::CheatInfiniteStaminaHint, "Trabajar no gasta estamina.", "Work spends no stamina."},
    {Text::CheatInfiniteMoney, "Dinero infinito", "Infinite money"},
    {Text::CheatInfiniteMoneyHint, "Tu dinero se queda en 999,999 G.", "Your money stays at 999,999 G."},
    {Text::CheatStopTime, "Detener el tiempo", "Stop time"},
    {Text::CheatStopTimeHint, "El reloj no avanza; dormir sigue pasando al día siguiente.",
     "The clock does not move; sleeping still goes on to the next day."},
    {Text::CheatMaxFriendship, "Amistad al máximo", "Maximum friendship"},
    {Text::CheatMaxFriendshipHint,
     "La amistad con todos queda al máximo y no se puede deshacer en esa partida: pueden empezar escenas de amor y de rivales.",
     "Friendship with everyone goes to the maximum, and it cannot be undone in that save: love and rival scenes may start."},
    {Text::FrameRateCounter, "%d FPS", "%d FPS"},
    {Text::Volume, "Volumen", "Volume"},
    {Text::VolumeHint, "La música y los sonidos del juego; 0% lo silencia.",
     "The game's music and sounds; 0% mutes it."},

    {Text::BackToGame, "Volver al juego", "Back to the game"},
    {Text::QuitGame, "Salir del juego", "Quit the game"},
    {Text::ShareLog, "Enviar el registro", "Send the log"},
    {Text::ShareLogHint,
     "Manda el registro de la última vez que se abrió el juego (por correo, un chat o Drive), para ver qué falló.",
     "Sends the log of the last time the game was opened (by email, a chat or Drive), to see what went wrong."},
    {Text::PlayHintInApp,
     "Abre el juego. Dentro del juego, el botón Options abre las opciones y los mods, y TURBO lo acelera.",
     "Opens the game. Inside the game, the Options button opens the options and the mods, and TURBO speeds it up."},
    {Text::ExitHintInApp, "Cierra la app.", "Closes the app."},
    {Text::QuitQuestion, "¿Salir del juego? Lo que no hayas guardado en el juego se pierde.",
     "Quit the game? Whatever you have not saved in the game is lost."},
    {Text::QuitYes, "Sí, salir", "Yes, quit"},
    {Text::QuitNo, "No, seguir jugando", "No, keep playing"},
    {Text::MenuOpenNote, "Menú abierto: el juego no recibe los controles. F10 o Select para volver.",
     "Menu open: the game gets no input. F10 or Select to go back."},
    {Text::PlayerSpeed, "Velocidad del personaje", "Speed of the character"},
    {Text::NeedsRestart, "requiere reiniciar el juego", "needs the game restarted"},
    {Text::RestartPending, "Reinicia el juego para aplicar los cambios", "Restart the game to apply the changes"},
    {Text::ModAppliesAtOnce, "cambia al momento", "changes at once"},
    {Text::ModNameJamieMarriage, "Casarse con Jamie", "Marry Jamie"},
    {Text::ModNamePlayerSpeed, "Personaje más rápido", "Faster character"},
    {Text::ModNameCarrySpeed, "Sin frenos al cargar", "No slowdown when carrying"},
    {Text::ModNameFarCamera, "Cámara más lejos", "Farther camera"},
    {Text::ModNameMarryAnyone, "Casarse con quien quieras", "Marry anyone"},
    {Text::ModNameLongerDays, "Días más largos", "Longer days"},
    {Text::ModNameGentleStamina, "Estamina más amable", "Gentler stamina"},
    {Text::ModNameFasterTools, "Herramientas que suben rápido", "Faster tools"},
    {Text::QualityOfLife, "Calidad de vida", "Quality of life"},
    {Text::ControllerSettings, "Ajustes del control", "Controller settings"},
    {Text::HideMenu, "Ocultar (F10)", "Hide (F10)"},
    {Text::GameLanguage, "Idioma", "Language"},
    {Text::GameLanguageShort, "Idioma", "Language"},
    {Text::GameLanguageHint,
     "Del juego y del launcher. El launcher solo está en español e inglés; en el juego cambian los diálogos.",
     "Of the game and of the launcher. The launcher is only in Spanish and English; in the game, the dialogues change."},
    {Text::GameLanguageRestart,
     "Los diálogos cambian al reiniciar el juego; sus menús y objetos siguen en inglés. Este menú solo está en español e inglés.",
     "The dialogues change when the game is restarted; its menus and items stay in English. This menu is only in Spanish and English."},
    {Text::AnalogStick, "Palanca", "Stick"},
    {Text::DirectionUp, "arriba", "up"},
    {Text::DirectionDown, "abajo", "down"},
    {Text::DirectionLeft, "izquierda", "left"},
    {Text::DirectionRight, "derecha", "right"},
    {Text::PressKey, "Presiona una tecla o un botón del ratón...", "Press a key or a mouse button..."},
    {Text::PressButton, "Presiona un botón del control...", "Press a button on the controller..."},
    {Text::RestoreMapping, "Restablecer", "Restore defaults"},
    {Text::MouseLeft, "Ratón izquierdo", "Mouse left"},
    {Text::MouseMiddle, "Ratón central", "Mouse middle"},
    {Text::MouseRight, "Ratón derecho", "Mouse right"},
    {Text::MouseOther, "Botón del ratón", "Mouse button"},
    {Text::EditorIntroKeyboard,
     "Elige una parte del control de GameCube y presiona la tecla (o el botón del ratón) que la juega. Escape "
     "cancela. Se guarda solo.",
     "Pick a part of the GameCube controller and press the key (or mouse button) that plays it. Escape cancels. "
     "It is kept by itself."},
    {Text::EditorIntroGamepad,
     "Elige un botón de GameCube y presiona el botón de tu control que lo juega. Las palancas siguen a las de tu "
     "control. Cada control guarda su propia asignación.",
     "Pick a GameCube button and press the button on your controller that plays it. The sticks follow your "
     "controller's. Each controller keeps its own mapping."},
    {Text::EditorConnectGamepad, "Conecta un control para asignarlo también.", "Connect a controller to set it up too."},
    {Text::NoKey, "Sin tecla", "No key"},
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
    {Text::ControlsDevice, "Dispositivo", "Device"},
    {Text::ControlsHintKeyboard,
     "Elige una parte del control de GameCube y presiona la tecla (o el botón del ratón) que la juega. Esc cancela. "
     "Se guarda solo.",
     "Pick a part of the GameCube controller and press the key (or mouse button) that plays it. Esc cancels. It is "
     "kept by itself."},
    {Text::ControlsHintGamepad,
     "Elige un botón de GameCube y presiona el botón de tu control que lo juega. Esc cancela. Las palancas siguen a "
     "las de tu control. Cada control guarda su asignación.",
     "Pick a GameCube button and press the button on your controller that plays it. Esc cancels. The sticks follow "
     "your controller's. Each controller keeps its own mapping."},
    {Text::ControlsRestoreHint, "Devuelve este dispositivo a la asignación del juego.",
     "Puts this device back to the game's own mapping."},
    {Text::PressShort, "Presiona...", "Press..."},
    {Text::ControlAssigned, "%s ahora es %s.", "%s is now %s."},
    {Text::Turbo, "Turbo (acelerar)", "Turbo (fast forward)"},
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
