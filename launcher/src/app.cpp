// See app.h. What the launcher does; view.cpp draws it.

#include "app.h"

#include "texts.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_stdinc.h>
#include <nlohmann/json.hpp>

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

constexpr const char* PACKAGE_VERSION = HMMM_PACKAGE_VERSION;

// The disc images the dialog offers.
constexpr const char* DISC_PATTERN = "iso;gcm;rvz;wia;gcz;ciso";

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

// The code of the game on a GameCube disc image read as it is (an .iso or .gcm): the first six
// letters of its header, which ends at 0x1C with the magic number of every GameCube disc.
std::string game_code(const std::filesystem::path& disc_image) {
    std::ifstream disc(disc_image, std::ios::binary);
    unsigned char header[0x20] = {};
    if (!disc.read(reinterpret_cast<char*>(header), sizeof(header))) {
        return {};
    }
    constexpr unsigned char GAMECUBE_MAGIC[] = {0xC2, 0x33, 0x9F, 0x3D};
    const bool gamecube = std::equal(std::begin(GAMECUBE_MAGIC), std::end(GAMECUBE_MAGIC), header + 0x1C);
    std::string code(header, header + 6);
    const bool readable =
        std::ranges::all_of(code, [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
    return gamecube && readable ? code : "";
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
    if (!disc.empty() && missing_.empty()) {
        start_preparation(disc);
    }
}

// -- What the launcher knows ----------------------------------------------------------------------

void App::apply_language() {
    const std::string chosen = settings_.text(settings::LANGUAGE, "en");
    texts::set_language(chosen == "es" ? texts::Language::Spanish : texts::Language::English);
}

void App::save_settings() { settings_.save(player_.settings); }

void App::read_prepared() {
    prepared_ = {};
    std::ifstream stream(player_.game / "prepared.json", std::ios::binary);
    const nlohmann::json document = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
    if (!document.is_object()) {
        return;
    }
    prepared_.exists = true;
    prepared_.game = document.value("game", "");
    prepared_.disc = document.value("disc", "");
    prepared_.source_disc = document.value("source_disc", "");
    prepared_.program = document.value("program", "");
    prepared_.package_version = document.value("package_version", "");
}

bool App::prepared_is_current() const {
    return prepared_.exists && prepared_.package_version == PACKAGE_VERSION && !prepared_.program.empty() &&
           file_exists(player_.game / paths::from_utf8(prepared_.program)) &&
           file_exists(paths::from_utf8(prepared_.disc));
}

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
std::vector<std::string> App::package_mods() const {
    std::ifstream stream(package_.kit / "recipe.json", std::ios::binary);
    const nlohmann::json recipe = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
    std::vector<std::string> ids;
    if (recipe.is_object() && recipe.contains("mods") && recipe["mods"].is_array()) {
        for (const auto& id : recipe["mods"]) {
            if (id.is_string()) {
                ids.push_back(id.get<std::string>());
            }
        }
    }
    return ids;
}

std::string App::error_message(const std::string& reason) const {
    const std::pair<const char*, Text> reasons[] = {
        {"not_a_disc", Text::ErrorNotADisc},     {"compressed", Text::ErrorCompressed},
        {"wrong_game", Text::ErrorWrongGame},    {"wrong_version", Text::ErrorWrongVersion},
        {"modified_game", Text::ErrorModifiedGame}, {"setup", Text::ErrorSetup},
        {"not_started", Text::ErrorNotStarted},
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
    preparation_.start(package_, player_, disc, PACKAGE_VERSION);
    if (tab_ == Tab::Game) {
        selected_ = 0;
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
    std::vector<std::string> enabled;
    std::vector<std::pair<std::string, std::string>> environment;
    for (const mods::Mod& mod : mods_) {
        if (settings_.mod_enabled(mod.id)) {
            enabled.push_back(mod.id);
        }
        for (const mods::Setting& setting : mod.settings) {
            if (!setting.environment.empty()) {
                const int value =
                    settings_.number(setting.key.c_str(), setting.fallback, setting.minimum, setting.maximum);
                environment.emplace_back(setting.environment, std::to_string(value));
            }
        }
    }
    std::string mod_list;
    for (const std::string& id : enabled) {
        mod_list += (mod_list.empty() ? "" : ",") + id;
    }
    // SDL writes numbers with a point whatever the computer's language: the game reads "1.5".
    char zoom[16];
    SDL_snprintf(zoom, sizeof(zoom), "%.2f", settings::zoom_for(settings_.number(settings::ZOOM_OUT, 0, 0, 100)));
    char resolution[16];
    SDL_snprintf(resolution, sizeof(resolution), "%g",
                 settings::resolution_scale(settings_.one_of(settings::RESOLUTION, settings::RESOLUTIONS)));

    ChildProcess::Options options;
    options.arguments = {
        paths::utf8(player_.game / paths::from_utf8(prepared_.program)),
        "--disc", prepared_.disc,
        "--symbols", paths::utf8(package_.symbols),
        "--settings", paths::utf8(player_.settings),
        "--mods", mod_list.empty() ? "none" : mod_list,
        "--zoom", zoom,
        "--resolution-scale", resolution,
        "--fullscreen", settings_.flag(settings::FULLSCREEN, false) ? "1" : "0",
        "--widescreen", settings_.flag(settings::WIDESCREEN, false) ? "1" : "0",
        "--frame-rate", std::to_string(settings_.one_of(settings::FRAME_RATE, settings::FRAME_RATES)),
        "--volume", std::to_string(settings_.number(settings::VOLUME, 100, 0, 100)),
    };
    options.environment = std::move(environment);
    options.working_directory = paths::utf8(player_.game);
    options.output_file = paths::utf8(player_.game_log);
    std::string error;
    game_ = ChildProcess::start(options, error);
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
    tab_ = tab;
    selected_ = 0;
    scroll_ = 0.f;
    description_scroll_ = 0.f;
    toast_.clear();
}

void App::move_selection(int step) {
    const int count = tab_ == Tab::Mods         ? static_cast<int>(mods_.size())
                      : tab_ == Tab::Game       ? static_cast<int>(game_rows().size())
                      : tab_ == Tab::Options    ? static_cast<int>(options_rows().size())
                                                : static_cast<int>(controls_rows().size());
    if (count == 0) {
        return;
    }
    selected_ = (selected_ + step + count) % count;
    description_scroll_ = 0.f;
    toast_.clear();
}

// -- The rows of the tabs -------------------------------------------------------------------------

std::string App::preparation_text() const {
    constexpr Text STAGES[] = {Text::StageDisc,    Text::StageTranslate, Text::StageAdapt,
                               Text::StageCompile, Text::StageLink,      Text::StageFinish};
    const int stage = preparation_.stage();
    if (stage < 0 || stage >= static_cast<int>(std::size(STAGES))) {
        return text(Text::PreparingRow);
    }
    std::string said = texts::format(Text::PreparingStage, stage + 1, static_cast<int>(std::size(STAGES))) +
                       text(STAGES[stage]);
    if (preparation_.stage_total() > 1) {
        said += " (" + texts::format(Text::StageCount, preparation_.stage_done(), preparation_.stage_total()) + ")";
    }
    return said;
}

std::vector<App::Row> App::game_rows() {
    const Preparation::State state = preparation_.state();
    const bool preparing = state == Preparation::State::Running;
    std::vector<Row> rows;

    Row play_row;
    play_row.play = true;
    play_row.value = "Enter";
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
        const std::string disc = remembered_disc();
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
        } else if (prepared_.exists && prepared_.package_version != PACKAGE_VERSION) {
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
    disc.value = disc_code_.empty() ? prepared_.game : disc_code_;
    disc.tone = disc.value.empty() ? Tone::None : Tone::Value;
    disc.hint = text(Text::ChangeDiscHint);
    disc.activate = [this] { choose_disc(); };
    rows.push_back(std::move(disc));

    Row log;
    log.symbol = "\xE2\x89\xA1";  // ≡
    log.label = text(Text::OpenLog);
    log.hint = text(Text::OpenLogHint);
    log.activate = [this] { open_log(); };
    rows.push_back(std::move(log));

    Row exit;
    exit.symbol = "\xE2\x9C\x95";  // ✕
    exit.label = text(Text::Exit);
    exit.value = "Esc";
    exit.tone = Tone::Key;
    exit.hint = text(Text::ExitHint);
    exit.activate = [this] {
        preparation_.cancel();
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
    percent(settings::ZOOM_OUT, 0, 0, 100, 5, Text::Zoom, Text::ZoomHint);
    percent(settings::VOLUME, 100, 0, 100, 10, Text::Volume, Text::VolumeHint);
    {
        // The language of the game's dialogues, of those the disc has.
        Row row;
        const int index = settings_.game_language();
        row.label = text(Text::GameLanguage);
        row.hint = text(Text::GameLanguageHint);
        row.value = settings::GAME_LANGUAGE_NAMES[index];
        row.tone = Tone::Value;
        row.arrows = true;
        row.change = [this, index, changed](int step) {
            const int count = static_cast<int>(std::size(settings::GAME_LANGUAGES));
            settings_.set(settings::GAME_LANGUAGE, settings::GAME_LANGUAGES[(index + step + count) % count]);
            changed();
        };
        row.activate = [change = row.change] { change(1); };
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<App::Row> App::controls_rows() {
    // The gamepads come first and the keyboard last; the first gamepad is the one that plays.
    const std::vector<controls::Device> devices = gamepads_.devices();
    const bool gamepad = devices.size() > 1;
    std::vector<Row> rows;

    Row keyboard;
    keyboard.label = text(Text::Keyboard);
    keyboard.value = gamepad ? text(Text::NotInUse) : text(Text::InUse);
    keyboard.tone = gamepad ? Tone::Off : Tone::Value;
    keyboard.hint = text(Text::KeyboardHint);
    rows.push_back(std::move(keyboard));

    Row controller;
    controller.label = text(Text::Controller);
    controller.value = gamepad ? devices.front().name : text(Text::NotConnected);
    controller.tone = gamepad ? Tone::Value : Tone::Warning;
    controller.hint = gamepad ? text(Text::ControllerHint) : text(Text::ControllerNoneHint);
    rows.push_back(std::move(controller));

    // The game maps every gamepad by itself, by its kind (controls.h).
    Row mapping;
    mapping.label = text(Text::AutomaticMapping);
    mapping.value = text(Text::ModOn);
    mapping.tone = Tone::On;
    mapping.hint = text(Text::TryButtons);
    rows.push_back(std::move(mapping));
    return rows;
}

// On the controls' tab, what is pressed on the gamepad that plays says what it is on a GameCube.
void App::watch_controller() {
    const std::vector<controls::Device> devices = gamepads_.devices();
    if (devices.size() < 2) {
        pressed_before_ = {};
        return;
    }
    const controls::State state = gamepads_.state(devices.front());
    for (size_t part = 0; part < controls::PART_COUNT; ++part) {
        if (state.pressed[part] && !pressed_before_[part]) {
            const auto which = static_cast<controls::Part>(part);
            toast_ = texts::format(Text::PressedPart, controls::part_name(which).c_str(),
                                   controls::binding(devices.front(), which).c_str());
        }
    }
    pressed_before_ = state.pressed;
}

void App::follow_disc() {
    const std::string disc = prepared_is_current() ? prepared_.disc : remembered_disc();
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
    if (started_ < 0.0) {
        started_ = seconds;
    }
    follow_disc();

    const auto rows_now = [this] {
        switch (tab_) {
        case Tab::Game: return game_rows();
        case Tab::Options: return options_rows();
        case Tab::Controls: return controls_rows();
        case Tab::Mods: break;
        }
        return std::vector<Row>();
    };
    handle_keys(rows_now());
    if (tab_ == Tab::Controls) {
        watch_controller();
    }
    // The keys may have changed the tab or a value: the rows are made again to draw them.
    draw(still_ ? 100.0 : seconds - started_, rows_now());
    if (pending_) {
        const std::function<void()> action = std::move(pending_);
        pending_ = nullptr;
        action();
    }
}
