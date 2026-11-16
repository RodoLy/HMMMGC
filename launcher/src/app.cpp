// See app.h. What the launcher does; view.cpp draws it.

#include "app.h"

#include "texts.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <span>
#include <system_error>
#include <utility>

namespace {

using texts::Text;
using texts::text;

// The disc images the dialog offers.
constexpr const char* DISC_PATTERN = "iso;gcm;rvz;wia;gcz;ciso";
// The disc this port plays, and how many of the images found of it the game tab offers.
constexpr const char* GAME_CODE = "G4AEE9";
constexpr int MOST_FOUND_DISCS_OFFERED = 3;

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

}  // namespace

App::App(SDL_Window* window, const std::string& disc, std::optional<Tab> first_tab, bool still)
    : window_(window), package_(paths::find_package()), player_(paths::find_player()), still_(still) {
    settings_ = settings::Settings::load(player_.settings);
    apply_language();
    missing_ = package_.missing();
    read_prepared();
    mods_ = mods::load(package_.mods, package_mods());
    tab_ = first_tab.value_or(Tab::Game);
    controls_ = controls_profile::Profile::load(controls_profile::file_beside(player_.settings));
    if (!disc.empty() && missing_.empty()) {
        start_preparation(disc);
    }
    disc_search_.start(package_.root, GAME_CODE);
}

// -- What the launcher knows ----------------------------------------------------------------------

void App::apply_language() {
    const std::string chosen = settings::GAME_LANGUAGES[settings_.language()];
    texts::set_language(chosen == "es" ? texts::Language::Spanish : texts::Language::English);
}

void App::save_settings() { settings_.save(player_.settings); }

void App::read_prepared() { prepared_ = PreparedGame::read(player_); }

bool App::prepared_is_current() const { return prepared_.is_current(player_); }

// The disc the player chose before, if it is still there.
std::string App::remembered_disc() const {
    for (const std::string& disc : {settings_.text(settings::DISC_IMAGE, ""), prepared_.source_disc}) {
        if (!disc.empty() && file_exists(paths::from_utf8(disc))) {
            return disc;
        }
    }
    return {};
}

// The mods the package's runtime has (the kit's recipe), which are the game's.
std::vector<std::string> App::package_mods() const { return kit_mods(package_); }

std::string App::error_message(const std::string& reason) const {
    const std::pair<const char*, Text> reasons[] = {
        {"not_a_disc", Text::ErrorNotADisc},     {"compressed", Text::ErrorCompressed},
        {"wrong_game", Text::ErrorWrongGame},    {"wrong_version", Text::ErrorWrongVersion},
        {"modified_game", Text::ErrorModifiedGame}, {"setup", Text::ErrorSetup},
        {"not_started", Text::ErrorNotStarted}, {"no_compiler", Text::ErrorNoCompiler},
    };
    for (const auto& [name, message] : reasons) {
        if (reason == name) {
            return text(message);
        }
    }
    return text(Text::ErrorToolFailed);
}

// -- Actions --------------------------------------------------------------------------------------

void SDLCALL App::disc_chosen(void* user_data, const char* const* files, int /*filter*/) {
    App* app = static_cast<App*>(user_data);
    const std::lock_guard lock(app->dialog_mutex_);
    // No list: the dialog failed; an empty list: the player closed it. Either way, no disc.
    app->chosen_disc_ = files != nullptr && files[0] != nullptr ? std::string(files[0]) : std::string();
}

void App::choose_disc() {
    if (dialog_open_ || preparation_.state() == Preparation::State::Running) {
        return;
    }
    dialog_open_ = true;
    // The filter must stay valid until the dialog answers, so it lives in the App.
    filter_name_ = text(Text::DiscFilter);
    filter_ = {filter_name_.c_str(), DISC_PATTERN};
    SDL_ShowOpenFileDialog(disc_chosen, this, window_, &filter_, 1, nullptr, false);
}

void App::start_preparation(const std::string& disc) {
    if (!missing_.empty()) {
        return;
    }
    settings_.set(settings::DISC_IMAGE, disc);
    save_settings();
    toast_.clear();
    preparation_.start(package_, player_, disc, HMMM_PACKAGE_VERSION);
    if (tab_ == Tab::Game) {
        selected_ = 0;
    }
}

void App::start_android_app(const std::string& disc) {
    if (!missing_.empty() || preparation_.state() == Preparation::State::Running) {
        return;
    }
    toast_.clear();
    android_app_.start_android_app(package_, player_, disc);
}

// The end of making the Android app: where the app is (its folder opens), or what failed.
void App::after_android_app() {
    if (android_app_.state() == Preparation::State::Succeeded) {
        const std::filesystem::path disc = paths::from_utf8(android_app_.disc_to_copy());
        toast_ = texts::format(Text::AndroidAppReady, paths::utf8(player_.android_app).c_str(),
                               paths::utf8(disc.parent_path()).c_str());
        paths::open_in_system(player_.android_app);
    } else if (android_app_.state() == Preparation::State::Failed) {
        toast_ = error_message(android_app_.error_reason());
    }
}

// What follows the end of a preparation: the game, when it is ready.
void App::after_preparation() {
    if (preparation_.state() == Preparation::State::Succeeded) {
        read_prepared();
        toast_ = text(Text::Prepared);
    }
}

void App::play() {
    if (!prepared_is_current()) {
        return;
    }
    std::string error;
    game_ = ChildProcess::start(game_start_options(package_, player_, settings_, mods_, prepared_), error);
    if (game_ == nullptr) {
        toast_ = std::string(text(Text::GameNotStarted)) + " (" + error + ")";
        return;
    }
    SDL_HideWindow(window_);
}

// The newer of the two logs: the preparation's or the game's.
void App::open_log() {
    std::error_code error;
    std::optional<std::filesystem::path> newest;
    std::filesystem::file_time_type newest_time;
    for (const std::filesystem::path& log : {player_.prepare_log, player_.game_log}) {
        if (!file_exists(log)) {
            continue;
        }
        const auto time = std::filesystem::last_write_time(log, error);
        if (!newest || time > newest_time) {
            newest = log;
            newest_time = time;
        }
    }
    if (newest) {
        paths::open_in_system(*newest);
    } else {
        toast_ = text(Text::NoLog);
    }
}

void App::update() {
    if (game_ == nullptr) {
        return;
    }
    const std::optional<int> code = game_->exit_code();
    if (!code) {
        return;
    }
    game_.reset();
    // The options menu inside the game keeps in settings.json what the player changed there.
    settings_ = settings::Settings::load(player_.settings);
    SDL_ShowWindow(window_);
    SDL_RaiseWindow(window_);
    toast_ = *code != 0 ? text(Text::GameFailed) : "";
}

void App::handle_event(const SDL_Event& event) {
    // On the controls tab, the key or the button pressed while a part waits is given to it.
    if (waiting_slot_ >= 0) {
        const bool keyboard = selected_device_ == 0;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                stop_waiting();
            } else if (keyboard) {
                give_key(static_cast<int>(event.key.scancode));
            }
        } else if (keyboard && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            // The left button chooses the rows, so it is not given to a part.
            switch (event.button.button) {
            case SDL_BUTTON_MIDDLE: give_key(controls_profile::MOUSE_MIDDLE); break;
            case SDL_BUTTON_RIGHT: give_key(controls_profile::MOUSE_RIGHT); break;
            case SDL_BUTTON_X1: give_key(controls_profile::MOUSE_X1); break;
            case SDL_BUTTON_X2: give_key(controls_profile::MOUSE_X2); break;
            default: break;
            }
        } else if (!keyboard && event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            give_button(event.gbutton.button);
        } else if (!keyboard && event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.value > 16000 &&
                   (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
                    event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) {
            give_button(controls_profile::axis_as_button(event.gaxis.axis));
        }
    }
    // A disc image dropped on the window is chosen as the disc.
    if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr &&
        preparation_.state() != Preparation::State::Running) {
        choose_tab(Tab::Game);
        start_preparation(event.drop.data);
    }
}

void App::choose_tab(Tab tab) {
    if (tab_ == Tab::Controls && tab != Tab::Controls) {
        gamepads_.close();
    }
    stop_waiting();
    if (tab == Tab::Controls) {
        // The game's options menu writes the file too.
        controls_ = controls_profile::Profile::load(controls_profile::file_beside(player_.settings));
    }
    tab_ = tab;
    selected_ = 0;
    scroll_ = 0.f;
    follow_selection_ = true;
    description_scroll_ = 0.f;
    toast_.clear();
}

void App::move_selection(int step) {
    const int count = tab_ == Tab::Mods         ? static_cast<int>(mods_.size())
                      : tab_ == Tab::Game       ? static_cast<int>(game_rows().size())
                      : tab_ == Tab::Options    ? static_cast<int>(options_rows().size())
                      : tab_ == Tab::Cheats     ? static_cast<int>(cheats_rows().size())
                                                : static_cast<int>(controls_rows().size());
    if (count == 0) {
        return;
    }
    selected_ = (selected_ + step + count) % count;
    follow_selection_ = true;
    description_scroll_ = 0.f;
    toast_.clear();
}

// -- The rows of the tabs -------------------------------------------------------------------------

std::string App::preparation_text() const { return stage_text(preparation_); }

std::string App::stage_text(const Preparation& preparation) {
    constexpr Text STAGES[] = {Text::StageDisc,    Text::StageTranslate, Text::StageAdapt,
                               Text::StageCompile, Text::StageLink,      Text::StageFinish};
    const int stage = preparation.stage();
    if (stage < 0 || stage >= static_cast<int>(std::size(STAGES))) {
        return text(Text::PreparingRow);
    }
    std::string said = texts::format(Text::PreparingStage, stage + 1, static_cast<int>(std::size(STAGES))) +
                       text(STAGES[stage]);
    if (preparation.stage_total() > 1) {
        said += " (" + texts::format(Text::StageCount, preparation.stage_done(), preparation.stage_total()) + ")";
    }
    return said;
}

std::string disc_name(const std::string& code) { return code == GAME_CODE ? "GameCube USA" : code; }

std::vector<App::Row> App::game_rows() {
    const Preparation::State state = preparation_.state();
    const bool preparing = state == Preparation::State::Running;
    const std::vector<std::string> found_discs = disc_search_.found();
    std::vector<Row> rows;

    Row play_row;
    play_row.play = true;
    play_row.value = using_pad_ ? "A" : "Enter";
    play_row.tone = Tone::Key;
    if (preparing) {
        play_row.label = text(Text::PreparingRow);
        play_row.value = std::to_string(static_cast<int>(preparation_.overall() * 100.f)) + "%";
        play_row.tone = Tone::Value;
        play_row.progress = preparation_.overall();
        play_row.hint = preparation_text();
    } else if (prepared_is_current()) {
        play_row.label = text(Text::Play);
        play_row.hint = text(Text::PlayHint);
        play_row.activate = [this] {
            toast_ = text(Text::Opening);
            play();
        };
    } else {
        std::string disc = remembered_disc();
        if (disc.empty() && !found_discs.empty()) {
            disc = found_discs.front();
        }
        const bool failed = state == Preparation::State::Failed || state == Preparation::State::Cancelled;
        play_row.label = failed ? text(Text::TryAgain) : text(Text::Prepare);
        if (!missing_.empty()) {
            std::string names;
            for (const std::string& name : missing_) {
                names += (names.empty() ? "" : ", ") + name;
            }
            play_row.hint = texts::format(Text::PackageIncomplete, names.c_str());
        } else if (state == Preparation::State::Failed) {
            play_row.hint = error_message(preparation_.error_reason());
        } else if (state == Preparation::State::Cancelled) {
            play_row.hint = text(Text::Cancelled);
        } else if (disc.empty()) {
            play_row.hint = text(Text::ChooseDiscHint);
        } else if (prepared_.exists && prepared_.package_version != HMMM_PACKAGE_VERSION) {
            play_row.hint = text(Text::UpdateBody);
        } else {
            play_row.hint = text(Text::PrepareHint);
        }
        play_row.activate = [this, disc, hint = play_row.hint] {
            if (!missing_.empty()) {
                toast_ = hint;
            } else if (disc.empty()) {
                choose_disc();
            } else {
                start_preparation(disc);
            }
        };
    }
    rows.push_back(std::move(play_row));

    Row disc;
    disc.symbol = "\xE2\x97\x8E";  // ◎
    disc.label = text(Text::ChangeDisc);
    disc.value = disc_name(disc_code_.empty() ? prepared_.game : disc_code_);
    disc.tone = disc.value.empty() ? Tone::None : Tone::Value;
    disc.hint = text(Text::ChangeDiscHint);
    disc.activate = [this] { choose_disc(); };
    rows.push_back(std::move(disc));

    // The images of the disc found in the usual folders (disc_search.h): one press prepares the game
    // with one, so that a controller is enough.
    if (!preparing && missing_.empty()) {
        int offered = 0;
        for (const std::string& found : found_discs) {
            if (found == shown_disc_ || offered == MOST_FOUND_DISCS_OFFERED) {
                continue;
            }
            const std::filesystem::path path = paths::from_utf8(found);
            Row row;
            row.symbol = "\xE2\x97\x8E";  // ◎
            row.label = texts::format(Text::FoundDisc, paths::utf8(path.filename()).c_str());
            row.hint = texts::format(Text::FoundDiscHint, paths::utf8(path.parent_path()).c_str());
            row.activate = [this, found] { start_preparation(found); };
            rows.push_back(std::move(row));
            ++offered;
        }
    }

    // The Android app, when the Android package is unpacked beside this one (paths.h).
    if (!package_.android.empty() && missing_.empty()) {
        Row row;
        row.symbol = "\xE2\x96\xA1";  // □, a phone
        if (android_app_.state() == Preparation::State::Running) {
            row.label = text(Text::MakingAndroidApp);
            row.value = std::to_string(static_cast<int>(android_app_.overall() * 100.f)) + "%";
            row.tone = Tone::Value;
            row.hint = stage_text(android_app_) + "\n" + text(Text::MakingAndroidAppHint);
            row.activate = [this] { android_app_.cancel(); };
        } else {
            std::string disc = remembered_disc();
            if (disc.empty() && !found_discs.empty()) {
                disc = found_discs.front();
            }
            row.label = text(Text::AndroidApp);
            row.hint = android_app_.state() == Preparation::State::Failed
                           ? error_message(android_app_.error_reason())
                           : std::string(text(Text::AndroidAppHint));
            row.activate = [this, disc] {
                if (disc.empty()) {
                    choose_disc();
                } else if (!preparing_anything()) {
                    start_android_app(disc);
                }
            };
        }
        rows.push_back(std::move(row));
    }

    {
        // One language for the game's dialogues and for the launcher, of those the disc has.
        Row row;
        const int index = settings_.language();
        row.label = text(Text::GameLanguage);
        row.hint = text(Text::GameLanguageHint);
        row.value = settings::GAME_LANGUAGE_NAMES[index];
        row.tone = Tone::Value;
        row.arrows = true;
        row.symbol = "\xE2\x99\xAA";  // ♪
        row.change = [this, index](int step) {
            const int count = static_cast<int>(std::size(settings::GAME_LANGUAGES));
            settings_.set(settings::LANGUAGE, settings::GAME_LANGUAGES[(index + step + count) % count]);
            save_settings();
            apply_language();
            mods_ = mods::load(package_.mods, package_mods());
        };
        row.activate = [change = row.change] { change(1); };
        rows.push_back(std::move(row));
    }

    // Here, beside what fails, so that the details are one row away when the box says to look.
    Row log;
    log.symbol = "\xE2\x89\xA1";  // ≡
    log.label = text(Text::OpenLog);
    log.hint = text(Text::OpenLogHint);
    log.activate = [this] { open_log(); };
    rows.push_back(std::move(log));

    Row exit;
    exit.symbol = "\xE2\x9C\x95";  // ✕
    exit.label = text(Text::Exit);
    exit.value = using_pad_ ? "B" : "Esc";
    exit.tone = Tone::Key;
    exit.hint = text(Text::ExitHint);
    exit.activate = [this] {
        preparation_.cancel();
        android_app_.cancel();
        quit_ = true;
    };
    rows.push_back(std::move(exit));
    return rows;
}

std::vector<App::Row> App::options_rows() {
    std::vector<Row> rows;
    const auto changed = [this] { save_settings(); };

    // A switch: Enter, a click, left and right turn it on or off.
    const auto flag = [&](const char* key, Text label, Text hint) {
        Row row;
        const bool on = settings_.flag(key, false);
        row.label = text(label);
        row.hint = text(hint);
        row.value = on ? text(Text::ModOn) : text(Text::ModOff);
        row.tone = on ? Tone::On : Tone::Off;
        row.arrows = true;
        row.change = [this, key, changed](int) {
            settings_.set(key, !settings_.flag(key, false));
            changed();
        };
        row.activate = [change = row.change] { change(1); };
        rows.push_back(std::move(row));
    };
    // One of a few values, in turn: left and right go round them, as Enter and a click do.
    const auto choice = [&](const char* key, std::span<const int> values, std::vector<std::string> labels, Text label,
                            Text hint) {
        Row row;
        const int value = settings_.one_of(key, values);
        const auto index = static_cast<int>(std::ranges::find(values, value) - values.begin());
        row.label = text(label);
        row.hint = text(hint);
        row.value = labels[static_cast<size_t>(index)];
        row.tone = Tone::Value;
        row.arrows = true;
        row.change = [this, key, values, index, changed](int step) {
            const int count = static_cast<int>(values.size());
            settings_.set(key, values[static_cast<size_t>((index + step + count) % count)]);
            changed();
        };
        row.activate = [change = row.change] { change(1); };
        rows.push_back(std::move(row));
    };
    // A percentage: left and right stop at its ends; Enter and a click go round.
    const auto percent = [&](const char* key, int fallback, int minimum, int maximum, int step, Text label, Text hint) {
        Row row;
        const int value = settings_.number(key, fallback, minimum, maximum);
        row.label = text(label);
        row.hint = text(hint);
        row.value = std::to_string(value) + "%";
        row.tone = Tone::Value;
        row.arrows = true;
        row.change = [this, key, value, minimum, maximum, step, changed](int direction) {
            settings_.set(key, std::clamp(value + step * direction, minimum, maximum));
            changed();
        };
        row.activate = [this, key, value, minimum, maximum, step, changed] {
            settings_.set(key, value + step > maximum ? minimum : value + step);
            changed();
        };
        rows.push_back(std::move(row));
    };

    choice(settings::RESOLUTION, settings::RESOLUTIONS,
           {text(Text::ResolutionWindow), "480p", "720p", "1080p", "1440p"}, Text::Resolution, Text::ResolutionHint);
    flag(settings::FULLSCREEN, Text::Fullscreen, Text::FullscreenHint);
    flag(settings::WIDESCREEN, Text::Widescreen, Text::WidescreenHint);
    choice(settings::FRAME_RATE, settings::FRAME_RATES, {text(Text::FrameRateGame), "60", "120"}, Text::FrameRate,
           Text::FrameRateHint);
    flag(settings::SHOW_FRAME_RATE, Text::ShowFrameRate, Text::ShowFrameRateHint);
    flag(settings::HIDE_TOUCH_BUTTONS, Text::HideTouchButtons, Text::HideTouchButtonsHint);
    flag(settings::HIDE_TOUCH_TURBO, Text::HideTouchTurbo, Text::HideTouchTurboHint);
    percent(settings::VOLUME, 100, 0, 100, 10, Text::Volume, Text::VolumeHint);
    return rows;
}

// The Cheats tab: a switch for each cheat, kept in settings.json's "cheats" (shared/settings.h). They
// are carried out by the hidden mod mods/cheats, which the game loads when one of them is on.
std::vector<App::Row> App::cheats_rows() {
    std::vector<Row> rows;
    const auto changed = [this] { save_settings(); };
    const auto cheat = [&](const char* name, Text label, Text hint) {
        Row row;
        row.symbol = "\xE2\x9C\xBF";  // ✿
        const bool on = settings_.cheat(name);
        row.label = text(label);
        row.hint = text(hint);
        row.value = on ? text(Text::ModOn) : text(Text::ModOff);
        row.tone = on ? Tone::On : Tone::Off;
        row.arrows = true;
        row.change = [this, name, changed](int) {
            settings_.set_cheat(name, !settings_.cheat(name));
            changed();
        };
        row.activate = [change = row.change] { change(1); };
        rows.push_back(std::move(row));
    };
    cheat("infiniteStamina", Text::CheatInfiniteStamina, Text::CheatInfiniteStaminaHint);
    cheat("infiniteMoney", Text::CheatInfiniteMoney, Text::CheatInfiniteMoneyHint);
    cheat("stopTime", Text::CheatStopTime, Text::CheatStopTimeHint);
    cheat("maxFriendship", Text::CheatMaxFriendship, Text::CheatMaxFriendshipHint);
    return rows;
}

// The symbol in a part's square: its letter, or the way it points.
const char* slot_symbol(controls_profile::Slot slot) {
    constexpr const char* SYMBOLS[controls_profile::SLOT_COUNT] = {
        "A", "B", "X", "Y", "Z", "L", "R", "S",
        "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x86\x90", "\xE2\x86\x92",  // ↑ ↓ ← →, the D-pad
        "\xE2\x96\xB2", "\xE2\x96\xBC", "\xE2\x97\x80", "\xE2\x96\xB6",  // ▲ ▼ ◀ ▶, the sticks
        "\xE2\x96\xB2", "\xE2\x96\xBC", "\xE2\x97\x80", "\xE2\x96\xB6",
        "\xE2\x86\x92",  // →, the turbo
    };
    return SYMBOLS[static_cast<int>(slot)];
}

std::vector<App::Row> App::controls_rows() {
    // The keyboard (with the mouse) first, then each gamepad connected; the list below is what
    // plays each part of the GameCube controller on the one chosen, and a part chosen waits for
    // the key or the button that will play it (handle_event).
    std::vector<controls::Device> devices = gamepads_.devices();
    devices.pop_back();  // the keyboard, which devices() lists last
    const int count = static_cast<int>(devices.size()) + 1;
    selected_device_ = std::clamp(selected_device_, 0, count - 1);
    const bool keyboard = selected_device_ == 0;
    const controls::Device* gamepad = keyboard ? nullptr : &devices[static_cast<size_t>(selected_device_ - 1)];
    std::vector<Row> rows;

    Row device;
    device.symbol = "\xE2\x9C\x9A";  // ✚
    device.label = text(Text::ControlsDevice);
    device.value = keyboard ? text(Text::Keyboard) : gamepad->name;
    device.tone = Tone::Value;
    device.arrows = count > 1;
    device.hint = count == 1 ? text(Text::ControllerNoneHint)
                  : keyboard ? text(Text::KeyboardHint)
                             : text(Text::ControllerHint);
    device.change = [this, count](int step) {
        stop_waiting();
        selected_device_ = (selected_device_ + step + count) % count;
    };
    device.activate = [change = device.change] { change(1); };
    rows.push_back(std::move(device));

    for (int index = 0; index < controls_profile::SLOT_COUNT; ++index) {
        const auto slot = static_cast<controls_profile::Slot>(index);
        if (!keyboard && !controls_profile::on_gamepad(slot)) {
            continue;
        }
        Row row;
        row.symbol = slot_symbol(slot);
        row.label = controls_profile::slot_name(slot);
        if (waiting_slot_ == index) {
            row.value = text(Text::PressShort);
            row.tone = Tone::Warning;
        } else {
            row.value = keyboard ? controls_profile::key_label(controls_.key_or_default(slot))
                                 : controls_profile::button_label(
                                       gamepad->type, controls_.button_or_default(gamepad->name, gamepad->type, slot));
            row.tone = Tone::Key;
        }
        row.hint = text(keyboard ? Text::ControlsHintKeyboard : Text::ControlsHintGamepad);
        // Chosen again (or Esc), it stops waiting; another part takes the wait over.
        row.activate = [this, index] { waiting_slot_ == index ? stop_waiting() : start_waiting(index); };
        rows.push_back(std::move(row));
    }

    Row restore;
    restore.symbol = "\xE2\x97\x8E";  // ◎
    restore.label = text(Text::RestoreMapping);
    restore.hint = text(Text::ControlsRestoreHint);
    restore.activate = [this, keyboard, name = gamepad != nullptr ? gamepad->name : std::string()] {
        stop_waiting();
        if (keyboard) {
            controls_.clear_keyboard();
        } else {
            controls_.clear_gamepad(name);
        }
        save_controls();
        toast_ = text(Text::ControlsRestoreHint);
    };
    rows.push_back(std::move(restore));
    return rows;
}

void App::start_waiting(int slot) {
    waiting_slot_ = slot;
    waiting_since_ = SDL_GetTicks();
    toast_ = text(selected_device_ == 0 ? Text::PressKey : Text::PressButton);
}

void App::stop_waiting() {
    if (waiting_slot_ >= 0) {
        settled_until_ = SDL_GetTicks() + 300;
        toast_.clear();
    }
    waiting_slot_ = -1;
}

bool App::choosing_controls() const { return waiting_slot_ >= 0 || SDL_GetTicks() < settled_until_; }

void App::save_controls() { controls_.save(controls_profile::file_beside(player_.settings)); }

void App::give_key(int key) {
    const auto slot = static_cast<controls_profile::Slot>(waiting_slot_);
    controls_.set_key(slot, key);
    save_controls();
    stop_waiting();
    toast_ = texts::format(Text::ControlAssigned, controls_profile::slot_name(slot).c_str(),
                           controls_profile::key_label(key).c_str());
}

void App::give_button(std::uint32_t native) {
    std::vector<controls::Device> devices = gamepads_.devices();
    if (selected_device_ < 1 || selected_device_ >= static_cast<int>(devices.size())) {
        stop_waiting();
        return;
    }
    const controls::Device& gamepad = devices[static_cast<size_t>(selected_device_ - 1)];
    const auto slot = static_cast<controls_profile::Slot>(waiting_slot_);
    controls_.set_button(gamepad.name, slot, native);
    save_controls();
    stop_waiting();
    toast_ = texts::format(Text::ControlAssigned, controls_profile::slot_name(slot).c_str(),
                           controls_profile::button_label(gamepad.type, native).c_str());
}

void App::follow_disc() {
    std::string disc = prepared_is_current() ? prepared_.disc : remembered_disc();
    if (disc.empty()) {
        // None chosen yet: the first one found is the one "Prepare the game" uses.
        const std::vector<std::string> found = disc_search_.found();
        disc = found.empty() ? "" : found.front();
    }
    if (disc != shown_disc_) {
        shown_disc_ = disc;
        disc_code_ = disc.empty() ? "" : game_code(paths::from_utf8(disc));
    }
}

void App::frame(double seconds) {
    std::optional<std::string> chosen;
    {
        const std::lock_guard lock(dialog_mutex_);
        chosen.swap(chosen_disc_);
    }
    if (chosen) {
        dialog_open_ = false;
        if (!chosen->empty()) {
            start_preparation(*chosen);
        }
    }
    preparation_.update();
    if (preparation_.state() != seen_state_) {
        seen_state_ = preparation_.state();
        after_preparation();
    }
    android_app_.update();
    if (android_app_.state() != seen_android_state_) {
        seen_android_state_ = android_app_.state();
        after_android_app();
    }
    if (started_ < 0.0) {
        started_ = seconds;
    }
    follow_disc();

    const auto rows_now = [this] {
        switch (tab_) {
        case Tab::Game: return game_rows();
        case Tab::Options: return options_rows();
        case Tab::Cheats: return cheats_rows();
        case Tab::Controls: return controls_rows();
        case Tab::Mods: break;
        }
        return std::vector<Row>();
    };
    // A part that waited too long gives up.
    if (waiting_slot_ >= 0 && SDL_GetTicks() - waiting_since_ > 6000) {
        stop_waiting();
    }
    handle_keys(rows_now());
    // The keys may have changed the tab or a value: the rows are made again to draw them.
    draw(still_ ? 100.0 : seconds - started_, rows_now());
    if (pending_) {
        const std::function<void()> action = std::move(pending_);
        pending_ = nullptr;
        action();
    }
}
