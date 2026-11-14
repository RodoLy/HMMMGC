// What the launcher says, in Spanish and in English.
//
// Lo que dice el launcher, en español y en inglés.
#pragma once

#include <string>

namespace texts {

enum class Language { Spanish, English };

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
    ChangeDisc,
    OpenLog,

    GameTab,
    PanelPicture,
    PanelModsOn,
    PanelDevices,
    DiscBox,
    StatusReady,
    StatusNotPrepared,
    StatusPreparing,
    StatusFailed,
    VersionBox,
    PlayHint,
    PrepareHint,
    ChooseDiscHint,
    PreparingRow,
    PreparingStage,
    ChangeDiscHint,
    OpenLogHint,
    NoLog,
    ExitHint,
    Opening,

    DiscFilter,
    UpdateBody,
    Prepare,
    PackageIncomplete,

    StageDisc,
    StageTranslate,
    StageAdapt,
    StageCompile,
    StageLink,
    StageFinish,
    StageCount,
    Cancel,
    Cancelled,
    Prepared,
    TryAgain,

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
    SoundSection,
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
    FrameRateHint,
    ShowFrameRate,
    ShowFrameRateHint,
    FrameRateCounter,
    Zoom,
    ZoomHint,
    Volume,
    VolumeHint,
    ZoomBeyondStart,

    BackToGame,
    QuitGame,
    QuitQuestion,
    QuitYes,
    QuitNo,
    MenuOpenNote,
    GameShortcuts,
    AnalogStick,
    GameLanguage,
    GameLanguageShort,
    GameLanguageHint,
    GameLanguageRestart,
    ControllerSettings,
    HideMenu,
    DirectionUp,
    DirectionDown,
    DirectionLeft,
    DirectionRight,
    PressKey,
    PressButton,
    RestoreMapping,
    MouseLeft,
    MouseMiddle,
    MouseRight,
    MouseOther,
    EditorIntroKeyboard,
    EditorIntroGamepad,
    EditorConnectGamepad,
    NoKey,
    ModsRestartNote,
    NeedsRestart,
    RestartPending,
    ModAppliesOnRestart,
    ModNameJamieMarriage,
    ModNamePlayerSpeed,
    ModNameCarrySpeed,

    ModOn,
    ModOff,
    ModNoSettings,
    NoManifest,
    NoManifestHint,
    NoDescription,

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
    InUse,
    NotInUse,
    Controller,
    NotConnected,
    KeyboardHint,
    ControllerNoneHint,
    ControllerHint,
    PressedPart,

    Count
};

const char* text(Text which);

// `text(which)` with the "%s" or "%d" of its translation filled in.
std::string format(Text which, const char* value);
std::string format(Text which, int value);
std::string format(Text which, int first, int second);
std::string format(Text which, const char* first, const char* second);

}  // namespace texts
