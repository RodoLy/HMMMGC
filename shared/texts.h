// What the launcher says, in Spanish and in English.
//
// Lo que dice el launcher, en español y en inglés.
#pragma once

#include <string>

namespace texts {

enum class Language { Spanish, English };

// The language of the computer's preferred locales: Spanish if it comes first, English otherwise.
Language system_language();
void set_language(Language language);
Language language();
// "es" or "en", as settings.json and the mods' manifests write it.
const char* language_code();

enum class Text {
    Subtitle,
    Play,
    Options,
    Mods,
    Controls,
    Exit,
    Back,
    Version,
    DiscLabel,
    ChangeDisc,
    OpenLog,

    WelcomeTitle,
    WelcomeBody,
    WelcomeFormats,
    ChooseDisc,
    DiscFilter,
    UpdateTitle,
    UpdateBody,
    PrepareAgainTitle,
    PrepareAgainBody,
    Prepare,
    PackageIncomplete,

    PreparingTitle,
    PreparingBody,
    StageDisc,
    StageTranslate,
    StageAdapt,
    StageCompile,
    StageLink,
    StageFinish,
    StageCount,
    Elapsed,
    ShowDetails,
    HideDetails,
    Cancel,
    Cancelled,
    Prepared,
    TryAgain,
    ChooseOther,

    ErrorNotADisc,
    ErrorCompressed,
    ErrorWrongGame,
    ErrorWrongVersion,
    ErrorModifiedGame,
    ErrorToolFailed,
    ErrorSetup,
    ErrorNotStarted,
    GameNotStarted,
    GameFailed,

    PictureSection,
    GameSection,
    LanguageSection,
    Resolution,
    ResolutionWindow,
    ResolutionHint,
    Fullscreen,
    FullscreenHint,
    Widescreen,
    WidescreenHint,
    FrameRate,
    FrameRateGame,
    FrameRateMade,
    Zoom,
    ZoomHint,
    GameSpeed,
    GameSpeedHint,
    LanguageAutomatic,

    ModsIntro,
    ModOn,
    ModOff,
    ModVersion,
    ModNoSettings,
    ModsIncluded,

    ControlsIntro,
    Keyboard,
    NoGamepads,
    AutomaticMapping,
    TryButtons,
    GameCubeButton,
    YourButton,
    ControlStick,
    CStick,
    DPad,
    LeftStick,
    RightStick,
    ArrowKeys,

    Count
};

const char* text(Text which);

// `text(which)` with the "%s" or "%d" of its translation filled in.
std::string format(Text which, const char* value);
std::string format(Text which, int value);
std::string format(Text which, int first, int second);

}  // namespace texts
