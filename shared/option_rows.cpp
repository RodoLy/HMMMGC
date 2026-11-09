// See option_rows.h.

#include "option_rows.h"

#include "texts.h"
#include "theme.h"

#include <imgui.h>

#include <iterator>
#include <string>
#include <vector>

namespace option_rows {
namespace {

using texts::Text;
using texts::text;

void focus_if(bool focus) {
    if (focus) {
        ImGui::SetKeyboardFocusHere();
    }
}

// A switch for a value that is true or false.
bool flag_row(settings::Settings& settings, const char* name, Text label, Text hint, bool focus) {
    next_row(text(label), text(hint));
    bool value = settings.flag(name, false);
    focus_if(focus);
    if (!theme::toggle(name, &value)) {
        return false;
    }
    settings.set(name, value);
    return true;
}

// A percentage from `minimum` to `maximum`, by `step`.
bool percent_row(settings::Settings& settings, const char* name, int fallback, int minimum, int maximum, int step,
                 Text label, Text hint, bool focus) {
    next_row(text(label), text(hint));
    int value = settings.number(name, fallback, minimum, maximum);
    focus_if(focus);
    if (!theme::percent(name, &value, minimum, maximum, step, control_width())) {
        return false;
    }
    settings.set(name, value);
    return true;
}

}  // namespace

bool begin(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.52f);
    ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthStretch, 0.48f);
    return true;
}

void end() { ImGui::EndTable(); }

void next_row(const char* label, const char* hint) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    theme::row_label(label, hint);
    ImGui::TableNextColumn();
}

float control_width() { return theme::row_control_width(); }

bool resolution(settings::Settings& settings, bool focus) {
    next_row(text(Text::Resolution), text(Text::ResolutionHint));
    // "As the window", and then each multiple of the console's 640x528 with its size.
    std::vector<std::string> labels;
    int index = 0;
    const int scale = settings.one_of(settings::RESOLUTION_SCALE, settings::RESOLUTION_SCALES);
    for (size_t i = 0; i < std::size(settings::RESOLUTION_SCALES); ++i) {
        const int value = settings::RESOLUTION_SCALES[i];
        labels.push_back(value == 0 ? text(Text::ResolutionWindow)
                                    : std::to_string(value) + "x  (" + std::to_string(640 * value) + "\xC3\x97" +
                                          std::to_string(528 * value) + ")");
        if (value == scale) {
            index = static_cast<int>(i);
        }
    }
    focus_if(focus);
    if (!theme::choice("resolution", &index, labels, control_width())) {
        return false;
    }
    settings.set(settings::RESOLUTION_SCALE, settings::RESOLUTION_SCALES[index]);
    return true;
}

bool fullscreen(settings::Settings& settings, bool focus) {
    return flag_row(settings, settings::FULLSCREEN, Text::Fullscreen, Text::FullscreenHint, focus);
}

bool widescreen(settings::Settings& settings, bool focus) {
    return flag_row(settings, settings::WIDESCREEN, Text::Widescreen, Text::WidescreenHint, focus);
}

bool frame_rate(settings::Settings& settings, bool focus) {
    next_row(text(Text::FrameRate));
    std::vector<std::string> labels;
    int index = 0;
    const int rate = settings.one_of(settings::FRAME_RATE, settings::FRAME_RATES);
    for (size_t i = 0; i < std::size(settings::FRAME_RATES); ++i) {
        const int value = settings::FRAME_RATES[i];
        labels.push_back(value == 30 ? text(Text::FrameRateGame) : texts::format(Text::FrameRateMade, value));
        if (value == rate) {
            index = static_cast<int>(i);
        }
    }
    focus_if(focus);
    if (!theme::choice("frame rate", &index, labels, control_width())) {
        return false;
    }
    settings.set(settings::FRAME_RATE, settings::FRAME_RATES[index]);
    return true;
}

bool zoom(settings::Settings& settings, bool focus) {
    return percent_row(settings, settings::ZOOM, 100, 50, 200, 5, Text::Zoom, Text::ZoomHint, focus);
}

bool game_speed(settings::Settings& settings, bool focus) {
    return percent_row(settings, settings::GAME_SPEED, 100, 100, 400, 10, Text::GameSpeed, Text::GameSpeedHint, focus);
}

bool volume(settings::Settings& settings, bool focus) {
    return percent_row(settings, settings::VOLUME, 100, 0, 100, 5, Text::Volume, Text::VolumeHint, focus);
}

}  // namespace option_rows
