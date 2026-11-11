// See app.h.

#include "app.h"

#include "option_rows.h"
#include "texts.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

namespace {

using texts::Text;
using texts::text;

constexpr const char* PACKAGE_VERSION = HMMM_PACKAGE_VERSION;

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

std::string file_name(const std::string& utf8_path) { return paths::utf8(paths::from_utf8(utf8_path).filename()); }

// The keys that go back from a screen: Escape, and the controller's B (its right face button).
bool back_pressed() {
    return ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
}

// The disc images the dialog offers.
constexpr const char* DISC_PATTERN = "iso;gcm;rvz;wia;gcz;ciso";

}  // namespace

App::App(SDL_Window* window, const std::string& disc, std::optional<Screen> first_screen)
    : window_(window), package_(paths::find_package()), player_(paths::find_player()) {
    settings_ = settings::Settings::load(player_.settings);
    apply_language();
    missing_ = package_.missing();
    read_prepared();
    mods_ = mods::load(package_.mods, package_mods());
    screen_ = prepared_is_current() ? Screen::Home : Screen::Disc;
    if (!disc.empty() && missing_.empty()) {
        start_preparation(disc);
    } else if (first_screen) {
        screen_ = *first_screen;
    }
}

// -- What the launcher knows ----------------------------------------------------------------------

void App::apply_language() {
    const std::string chosen = settings_.text(settings::LANGUAGE, "auto");
    texts::set_language(chosen == "es"   ? texts::Language::Spanish
                        : chosen == "en" ? texts::Language::English
                                         : texts::system_language());
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

void App::go(Screen screen) {
    if (screen_ == Screen::Controls && screen != Screen::Controls) {
        gamepads_.close();
    }
    screen_ = screen;
    focus_first_ = true;
}

void SDLCALL App::disc_chosen(void* user_data, const char* const* files, int /*filter*/) {
    App* app = static_cast<App*>(user_data);
    const std::lock_guard lock(app->dialog_mutex_);
    // No list: the dialog failed; an empty list: the player closed it. Either way, no disc.
    app->chosen_disc_ = files != nullptr && files[0] != nullptr ? std::string(files[0]) : std::string();
}

void App::choose_disc() {
    if (dialog_open_) {
        return;
    }
    dialog_open_ = true;
    // The filter must stay valid until the dialog answers, so it lives in the App.
    filter_name_ = text(Text::DiscFilter);
    filter_ = {filter_name_.c_str(), DISC_PATTERN};
    SDL_ShowOpenFileDialog(disc_chosen, this, window_, &filter_, 1, nullptr, false);
}

void App::start_preparation(const std::string& disc) {
    settings_.set(settings::DISC_IMAGE, disc);
    save_settings();
    show_details_ = false;
    message_.clear();
    preparation_.start(package_, player_, disc, PACKAGE_VERSION);
    go(Screen::Preparing);
}

void App::play() {
    if (!prepared_is_current()) {
        go(Screen::Disc);
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
                const int value = settings_.number(setting.key.c_str(), setting.fallback, setting.minimum, setting.maximum);
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
    SDL_snprintf(zoom, sizeof(zoom), "%.2f", settings_.number(settings::ZOOM, 100, 50, 200) / 100.0);
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
        "--game-speed", std::to_string(settings_.number(settings::GAME_SPEED, 100, 100, 400)),
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
        message_ = std::string(text(Text::GameNotStarted)) + " (" + error + ")";
        message_has_log_ = false;
        return;
    }
    message_.clear();
    SDL_HideWindow(window_);
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
    if (*code != 0) {
        message_ = text(Text::GameFailed);
        message_has_log_ = true;
    }
    focus_first_ = true;
}

void App::handle_event(const SDL_Event& event) {
    // A disc image dropped on the window is chosen as the disc.
    if (event.type == SDL_EVENT_DROP_FILE && event.drop.data != nullptr && missing_.empty() &&
        screen_ != Screen::Preparing) {
        start_preparation(event.drop.data);
    }
}

// -- Screens --------------------------------------------------------------------------------------

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

    const ImVec2 size = ImGui::GetIO().DisplaySize;
    theme::draw_background(ImGui::GetBackgroundDrawList(), size, seconds);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("##launcher", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    switch (screen_) {
    case Screen::Home: home(size, seconds); break;
    case Screen::Disc: disc_screen(size); break;
    case Screen::Preparing: preparing(size, seconds); break;
    case Screen::Options: options(size); break;
    case Screen::Mods: mods_screen(size); break;
    case Screen::Controls: controls_screen(size); break;
    }
    ImGui::End();
}

void App::home(ImVec2 size, double seconds) {
    (void)seconds;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    theme::draw_title_sign(draw, ImVec2(size.x * 0.5f, theme::px(150)), false, text(Text::Subtitle));

    const ImVec2 button(theme::px(360), theme::px(56));
    const float spacing = theme::px(14);
    float y = theme::px(318);
    const auto next_button = [&](const char* label, theme::ButtonColor color) {
        ImGui::SetCursorScreenPos(ImVec2((size.x - button.x) * 0.5f, y));
        if (focus_first_) {
            ImGui::SetKeyboardFocusHere();
            focus_first_ = false;
        }
        y += button.y + spacing;
        return theme::menu_button(label, button, color);
    };
    if (next_button(text(Text::Play), theme::ButtonColor::Green)) {
        play();
    }
    if (next_button(text(Text::Options), theme::ButtonColor::Orange)) {
        go(Screen::Options);
    }
    if (next_button(text(Text::Mods), theme::ButtonColor::Orange)) {
        go(Screen::Mods);
    }
    if (next_button(text(Text::Controls), theme::ButtonColor::Orange)) {
        go(Screen::Controls);
    }
    if (next_button(text(Text::Exit), theme::ButtonColor::Gray)) {
        quit_ = true;
    }

    // A problem with the last game, over the fence.
    if (!message_.empty()) {
        const float width = theme::px(760);
        ImGui::SetCursorScreenPos(ImVec2((size.x - width) * 0.5f, size.y - theme::px(178)));
        ImGui::BeginChild("##message", ImVec2(width, theme::px(120)), ImGuiChildFlags_NavFlattened,
                          ImGuiWindowFlags_NoBackground);
        theme::note(theme::NoteKind::Error, message_);
        if (message_has_log_ && theme::menu_button(text(Text::OpenLog), ImVec2(theme::px(260), theme::px(44)))) {
            paths::open_in_system(player_.game_log);
        }
        ImGui::EndChild();
    }

    // The disc and the version, at the bottom corners.
    ImFont* font = theme::fonts().small;
    const auto corner_label = [&](const std::string& label, bool right) {
        const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, label.c_str());
        const ImVec2 padding(theme::px(12), theme::px(6));
        const ImVec2 min(right ? size.x - extent.x - padding.x * 2 - theme::px(14) : theme::px(14),
                         size.y - extent.y - padding.y * 2 - theme::px(12));
        draw->AddRectFilled(min, min + extent + padding * 2, IM_COL32(255, 255, 255, 200), theme::px(10));
        draw->AddText(font, font->FontSize, min + padding, theme::TEXT, label.c_str());
    };
    if (!prepared_.source_disc.empty()) {
        corner_label(texts::format(Text::DiscLabel, file_name(prepared_.source_disc).c_str()), false);
    }
    corner_label(texts::format(Text::Version, PACKAGE_VERSION), true);
}

void App::begin_screen(ImVec2 size, const char* title, float height_share) {
    theme::draw_title_sign(ImGui::GetWindowDrawList(), ImVec2(size.x * 0.5f, theme::px(54)), true);
    const float top = theme::px(112);
    const float bottom = size.y - theme::px(96);
    const float height = (bottom - top) * height_share;
    const float width = std::min(size.x - theme::px(100), theme::px(1180));
    const ImVec2 min((size.x - width) * 0.5f, top + (bottom - top - height) * 0.5f);
    theme::begin_panel("##panel", min, min + ImVec2(width, height), title);
}

bool App::end_screen(ImVec2 size, bool can_go_back) {
    theme::end_panel();
    if (!can_go_back) {
        return false;
    }
    const ImVec2 button(theme::px(240), theme::px(54));
    ImGui::SetCursorScreenPos(ImVec2(size.x - button.x - theme::px(60), size.y - button.y - theme::px(26)));
    const bool pressed = theme::menu_button(text(Text::Back), button, theme::ButtonColor::Orange);
    if (pressed || back_pressed()) {
        go(Screen::Home);
        return true;
    }
    return false;
}

void App::disc_screen(ImVec2 size) {
    const bool update = prepared_.exists && prepared_.package_version != PACKAGE_VERSION;
    const std::string remembered = remembered_disc();
    const char* title = update ? text(Text::UpdateTitle)
                        : !remembered.empty() ? text(Text::PrepareAgainTitle)
                                              : text(Text::WelcomeTitle);
    begin_screen(size, title, 0.7f);
    const ImVec2 button(theme::px(420), theme::px(56));
    if (!missing_.empty()) {
        std::string names;
        for (const std::string& name : missing_) {
            names += (names.empty() ? "" : ", ") + name;
        }
        theme::note(theme::NoteKind::Error, texts::format(Text::PackageIncomplete, names.c_str()));
    } else {
        if (update) {
            theme::paragraph(text(Text::UpdateBody));
        } else if (!remembered.empty()) {
            theme::paragraph(texts::format(Text::PrepareAgainBody, file_name(remembered).c_str()));
        } else {
            theme::paragraph(text(Text::WelcomeBody));
            theme::paragraph(text(Text::WelcomeFormats), theme::TEXT_SOFT);
        }
        if (!message_.empty()) {
            theme::note(theme::NoteKind::Error, message_);
        }
        ImGui::Dummy(ImVec2(0, theme::px(6)));
        if (focus_first_) {
            ImGui::SetKeyboardFocusHere();
            focus_first_ = false;
        }
        if (!remembered.empty()) {
            if (theme::menu_button(text(Text::Prepare), button, theme::ButtonColor::Green)) {
                start_preparation(remembered);
            }
            if (theme::menu_button(text(Text::ChooseOther), button)) {
                choose_disc();
            }
        } else if (theme::menu_button(text(Text::ChooseDisc), button, theme::ButtonColor::Green)) {
            choose_disc();
        }
    }
    if (theme::menu_button(text(Text::Exit), ImVec2(theme::px(240), theme::px(50)), theme::ButtonColor::Gray)) {
        quit_ = true;
    }
    end_screen(size, false);
}

void App::preparing(ImVec2 size, double seconds) {
    const Preparation::State state = preparation_.state();
    if (state == Preparation::State::Succeeded) {
        read_prepared();
        message_.clear();
        go(prepared_is_current() ? Screen::Home : Screen::Disc);
        return;
    }
    begin_screen(size, text(Text::PreparingTitle));
    theme::paragraph(text(Text::PreparingBody), theme::TEXT_SOFT);

    // The steps, ticked off as they end.
    constexpr Text STAGE_TEXTS[] = {Text::StageDisc,    Text::StageTranslate, Text::StageAdapt,
                                    Text::StageCompile, Text::StageLink,      Text::StageFinish};
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* font = theme::fonts().body;
    const float row = theme::px(32);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f));
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(seconds) * 5.f);
    for (int index = 0; index < static_cast<int>(Preparation::STAGES.size()); ++index) {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const bool done = index < preparation_.stage();
        const bool current = index == preparation_.stage();
        const bool failed = current && state != Preparation::State::Running;
        const ImVec2 dot(at.x + theme::px(14), at.y + row * 0.5f);
        if (done) {
            draw->AddCircleFilled(dot, theme::px(11), theme::GREEN, 24);
            draw->AddText(theme::fonts().small, theme::fonts().small->FontSize, dot - ImVec2(theme::px(6), theme::px(10)),
                          theme::WHITE, "\xE2\x9C\x93");
        } else if (current) {
            const ImU32 color = failed ? theme::NAME_RED : theme::ORANGE;
            draw->AddCircleFilled(dot, theme::px(8) + (failed ? 0.f : theme::px(3) * pulse), color, 24);
        } else {
            draw->AddCircle(dot, theme::px(9), theme::LINE, 24, theme::px(2.5f));
        }
        std::string label = text(STAGE_TEXTS[index]);
        if (current && preparation_.stage_total() > 1 && Preparation::STAGES[static_cast<size_t>(index)] ==
                                                             std::string_view("compile")) {
            label += "  \xC2\xB7  " + texts::format(Text::StageCount, preparation_.stage_done(), preparation_.stage_total());
        }
        draw->AddText(font, font->FontSize, ImVec2(at.x + theme::px(38), at.y + (row - font->FontSize) * 0.5f),
                      done || current ? theme::TEXT : theme::TEXT_SOFT, label.c_str());
        ImGui::Dummy(ImVec2(0, row));
    }
    ImGui::PopStyleVar();

    ImGui::Dummy(ImVec2(0, theme::px(4)));
    theme::progress_bar(preparation_.overall(), ImVec2(ImGui::GetContentRegionAvail().x, theme::px(30)), seconds);
    const int elapsed = static_cast<int>(preparation_.seconds());
    theme::paragraph(texts::format(Text::Elapsed, elapsed / 60, elapsed % 60), theme::TEXT_SOFT);

    const ImVec2 button(theme::px(300), theme::px(50));
    if (state == Preparation::State::Running) {
        if (focus_first_) {
            ImGui::SetKeyboardFocusHere();
            focus_first_ = false;
        }
        if (theme::menu_button(text(Text::Cancel), button, theme::ButtonColor::Gray)) {
            preparation_.cancel();
        }
    } else {
        theme::note(state == Preparation::State::Cancelled ? theme::NoteKind::Warning : theme::NoteKind::Error,
                    state == Preparation::State::Cancelled ? text(Text::Cancelled)
                                                           : error_message(preparation_.error_reason()));
        if (focus_first_) {
            ImGui::SetKeyboardFocusHere();
            focus_first_ = false;
        }
        const std::string disc = settings_.text(settings::DISC_IMAGE, "");
        if (theme::menu_button(text(Text::TryAgain), button, theme::ButtonColor::Green) && !disc.empty()) {
            start_preparation(disc);
        }
        ImGui::SameLine();
        if (theme::menu_button(text(Text::ChooseOther), button)) {
            choose_disc();
        }
        ImGui::SameLine();
        if (theme::menu_button(text(Text::OpenLog), button)) {
            paths::open_in_system(player_.prepare_log);
        }
    }

    // What the tools print, for whoever wants to see it.
    ImGui::PushFont(theme::fonts().small);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::TEXT_SOFT);
    if (ImGui::Selectable(show_details_ ? text(Text::HideDetails) : text(Text::ShowDetails), false,
                          ImGuiSelectableFlags_None, ImGui::CalcTextSize(text(Text::ShowDetails)) * 1.2f)) {
        show_details_ = !show_details_;
    }
    ImGui::PopStyleColor();
    if (show_details_) {
        ImGui::BeginChild("##details", ImVec2(0, std::max(theme::px(120), ImGui::GetContentRegionAvail().y)),
                          ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string& line : preparation_.lines()) {
            ImGui::TextUnformatted(line.c_str());
        }
        if (state == Preparation::State::Running) {
            ImGui::SetScrollHereY(1.f);
        }
        ImGui::EndChild();
    }
    ImGui::PopFont();
    end_screen(size, false);
}

void App::options(ImVec2 size) {
    begin_screen(size, text(Text::Options));
    bool changed = false;
    theme::heading(text(Text::PictureSection));
    if (option_rows::begin("##picture")) {
        changed |= option_rows::resolution(settings_, std::exchange(focus_first_, false));
        changed |= option_rows::fullscreen(settings_);
        changed |= option_rows::widescreen(settings_);
        changed |= option_rows::frame_rate(settings_);
        changed |= option_rows::show_frame_rate(settings_);
        changed |= option_rows::zoom(settings_);
        option_rows::end();
    }
    theme::heading(text(Text::GameSection));
    if (option_rows::begin("##game")) {
        changed |= option_rows::game_speed(settings_);
        option_rows::end();
    }
    theme::heading(text(Text::SoundSection));
    if (option_rows::begin("##sound")) {
        changed |= option_rows::volume(settings_);
        option_rows::end();
    }

    // The language is the launcher's: it changes its texts and the mods' names at once.
    theme::heading(text(Text::LanguageSection));
    if (option_rows::begin("##language")) {
        option_rows::next_row(text(Text::LanguageSection));
        const std::vector<std::string> codes = {"auto", "es", "en"};
        const std::vector<std::string> names = {text(Text::LanguageAutomatic), "Espa\xC3\xB1ol", "English"};
        const std::string language = settings_.text(settings::LANGUAGE, "auto");
        int language_index = static_cast<int>(std::ranges::find(codes, language) - codes.begin());
        language_index = language_index < static_cast<int>(codes.size()) ? language_index : 0;
        if (theme::choice("language", &language_index, names, option_rows::control_width())) {
            settings_.set(settings::LANGUAGE, codes[static_cast<size_t>(language_index)]);
            apply_language();
            mods_ = mods::load(package_.mods, package_mods());
            changed = true;
        }
        option_rows::end();
    }
    if (changed) {
        save_settings();
    }
    end_screen(size, true);
}

void App::mods_screen(ImVec2 size) {
    begin_screen(size, text(Text::Mods));
    theme::paragraph(text(Text::ModsIntro), theme::TEXT_SOFT);
    if (mods_.empty()) {
        theme::paragraph(text(Text::ModsIncluded));
        end_screen(size, true);
        return;
    }
    selected_mod_ = std::min(selected_mod_, mods_.size() - 1);
    const float list_width = ImGui::GetContentRegionAvail().x * 0.42f;
    const float height = ImGui::GetContentRegionAvail().y;
    bool changed = false;

    // The list: a card for each mod, with its switch.
    ImGui::BeginChild("##mod list", ImVec2(list_width, height), ImGuiChildFlags_NavFlattened);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t index = 0; index < mods_.size(); ++index) {
        const mods::Mod& mod = mods_[index];
        ImGui::PushID(mod.id.c_str());
        const ImVec2 card_size(ImGui::GetContentRegionAvail().x - theme::px(8), theme::px(92));
        const ImVec2 min = ImGui::GetCursorScreenPos() + ImVec2(theme::px(4), theme::px(4));
        const bool selected = index == selected_mod_;
        draw->AddRectFilled(min, min + card_size, IM_COL32(255, 255, 255, selected ? 255 : 190), theme::px(18));
        draw->AddRect(min, min + card_size, selected ? theme::ORANGE : theme::LINE, theme::px(18), 0,
                      theme::px(selected ? 3.f : 2.f));
        ImFont* heading = theme::fonts().button;
        draw->AddText(heading, heading->FontSize * 0.85f, min + ImVec2(theme::px(18), theme::px(14)), theme::TEXT,
                      mod.name.c_str());
        ImFont* small = theme::fonts().small;
        draw->AddText(small, small->FontSize, min + ImVec2(theme::px(18), theme::px(52)), theme::TEXT_SOFT,
                      mod.summary.c_str(), nullptr, card_size.x - theme::px(120));
        // The card selects the mod; its switch turns it on and off.
        ImGui::SetCursorScreenPos(min);
        ImGui::SetNextItemAllowOverlap();
        if (index == 0 && focus_first_) {
            ImGui::SetKeyboardFocusHere();
            focus_first_ = false;
        }
        if (ImGui::InvisibleButton("##card", card_size, ImGuiButtonFlags_EnableNav)) {
            selected_mod_ = index;
        }
        if (ImGui::IsItemFocused()) {
            selected_mod_ = index;
        }
        ImGui::SetCursorScreenPos(min + ImVec2(card_size.x - theme::px(88), theme::px(26)));
        bool enabled = settings_.mod_enabled(mod.id);
        if (theme::toggle("##enabled", &enabled)) {
            settings_.set_mod_enabled(mod.id, enabled);
            selected_mod_ = index;
            changed = true;
        }
        ImGui::SetCursorScreenPos(min + ImVec2(-theme::px(4), card_size.y + theme::px(10)));
        ImGui::Dummy(ImVec2(0, 0));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine(0, theme::px(24));

    // The selected mod: what it does, its settings and its warning.
    const mods::Mod& mod = mods_[selected_mod_];
    ImGui::BeginChild("##mod details", ImVec2(0, height), ImGuiChildFlags_NavFlattened);
    theme::heading(mod.name);
    std::string about = mod.version.empty() ? "" : texts::format(Text::ModVersion, mod.version.c_str());
    if (!mod.authors.empty()) {
        about += (about.empty() ? "" : "  \xC2\xB7  ") + mod.authors;
    }
    const bool enabled = settings_.mod_enabled(mod.id);
    about += std::string("  \xC2\xB7  ") + (enabled ? text(Text::ModOn) : text(Text::ModOff));
    theme::paragraph(about, theme::TEXT_SOFT);
    theme::paragraph(mod.description);
    for (const mods::Setting& setting : mod.settings) {
        ImGui::Dummy(ImVec2(0, theme::px(4)));
        theme::paragraph(setting.label);
        int value = settings_.number(setting.key.c_str(), setting.fallback, setting.minimum, setting.maximum);
        if (theme::percent(setting.key.c_str(), &value, setting.minimum, setting.maximum, setting.step,
                           ImGui::GetContentRegionAvail().x)) {
            settings_.set(setting.key.c_str(), value);
            changed = true;
        }
    }
    if (!mod.warning.empty() && enabled) {
        ImGui::Dummy(ImVec2(0, theme::px(4)));
        theme::note(theme::NoteKind::Warning, mod.warning);
    }
    ImGui::EndChild();
    if (changed) {
        save_settings();
    }
    end_screen(size, true);
}

void App::controls_screen(ImVec2 size) {
    begin_screen(size, text(Text::Controls));
    controls::draw_screen(gamepads_, selected_device_, std::exchange(focus_first_, false));
    end_screen(size, true);
}
