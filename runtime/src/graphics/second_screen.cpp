// See second_screen.h.

#include "second_screen.h"

#include "runtime_log.h"
#include "theme.h"

// aurora's own device and render thread (aurora-main/lib), which the second screen shares.
#include "gfx/render_worker.hpp"
#include "webgpu/gpu.hpp"

#include <aurora/webgpu.hpp>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>

#include <SDL3/SDL_video.h>

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#endif

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace second_screen {
namespace {

namespace render_worker = aurora::gfx::render_worker;
namespace webgpu = aurora::webgpu;

// How long Android waits, at most, for the second screen to let go of its surface.
constexpr auto DETACH_TIMEOUT = std::chrono::seconds(1);

// The colour of the second screen around the menu and while it is closed.
constexpr wgpu::Color BACKGROUND{0.03, 0.03, 0.05, 1.0};

// The menu's picture takes at most this much of the screen's height; below it are the buttons.
constexpr float PICTURE_MOST_HEIGHT = 0.82f;

// Where a surface comes from: the app's window on the other screen, or the test window.
struct Source {
    ANativeWindow* android = nullptr;  // one reference of it belongs to the second screen
    SDL_Window* test_window = nullptr;
    int width = 0;
    int height = 0;
};

// The test window's mouse is a finger while its button is down; when the surface goes, every
// finger is lifted.
constexpr int MOUSE_FINGER = -1;
constexpr int EVERY_FINGER = -2;

// A finger of the app, or the test window's mouse.
struct Pointer {
    Touch action = Touch::Move;
    int finger = 0;
    float x = 0.f;  // from 0 to 1 of the screen
    float y = 0.f;
};

void release_android_window(ANativeWindow* window) {
#if defined(__ANDROID__)
    if (window != nullptr) {
        ANativeWindow_release(window);
    }
#else
    (void)window;
#endif
}

// One frame of the buttons, kept while the render thread draws it: ImGui's lists are copied, as
// aurora does with its own frames (aurora-main/lib/imgui.cpp).
struct FrozenFrame {
    ImDrawData data;
    std::vector<std::unique_ptr<ImDrawList>> lists;
};

// ---- Shared between the threads ----------------------------------------------------------------

std::mutex g_mutex;
std::optional<Source> g_new_source;  // given, and not yet made into a surface
std::vector<Pointer> g_pointers;     // not yet seen by the game's thread
std::shared_ptr<FrozenFrame> g_buttons_frame;

// Set by the render thread: the size of the surface that is ready to draw on (0 when none is),
// and the shape of the game's picture (width / height).
std::atomic<int> g_ready_width{0};
std::atomic<int> g_ready_height{0};
std::atomic<float> g_picture_aspect{4.f / 3.f};
// Set by the game's thread: whether this frame's picture is the game's menu.
std::atomic<bool> g_menu_shown{false};

// ---- The render thread's ------------------------------------------------------------------------

wgpu::Surface g_surface;
wgpu::SurfaceConfiguration g_configuration;
ANativeWindow* g_window_in_use = nullptr;
wgpu::Texture g_kept;           // the last picture before the menu, in the surface's format
wgpu::TextureView g_kept_view;
wgpu::Extent3D g_kept_size{};
bool g_showing_menu = false;     // the second screen shows the menu (and goes dark when it closes)

void release_surface() {
    if (g_surface) {
        g_surface.Unconfigure();
        g_surface = nullptr;
    }
    release_android_window(std::exchange(g_window_in_use, nullptr));
    g_kept = nullptr;
    g_kept_view = nullptr;
    g_showing_menu = false;
    g_ready_width = 0;
    g_ready_height = 0;
}

wgpu::Surface create_surface(const Source& source) {
    if (source.test_window != nullptr) {
        return webgpu::create_window_surface(webgpu::g_instance, source.test_window, "Second screen");
    }
#if defined(__ANDROID__)
    wgpu::SurfaceSourceAndroidNativeWindow android;
    android.window = source.android;
    const wgpu::SurfaceDescriptor descriptor{.nextInChain = &android, .label = "Second screen"};
    return webgpu::g_instance.CreateSurface(&descriptor);
#else
    return nullptr;
#endif
}

// The surface takes the format of aurora's own, because aurora's pipelines (the copy of the picture,
// ImGui) draw on both.
bool configure(int width, int height) {
    wgpu::SurfaceCapabilities capabilities;
    if (g_surface.GetCapabilities(webgpu::g_device.GetAdapter(), &capabilities) != wgpu::Status::Success) {
        runtime_log::warning("second screen", "the surface says nothing of what it can do");
        return false;
    }
    const wgpu::TextureFormat format = webgpu::g_graphicsConfig.surfaceConfiguration.format;
    const auto formats_end = capabilities.formats + capabilities.formatCount;
    if (std::find(capabilities.formats, formats_end, format) == formats_end) {
        runtime_log::warning("second screen", "the surface cannot take the game screen's colour format");
        return false;
    }
    g_configuration = wgpu::SurfaceConfiguration{
        .device = webgpu::g_device,
        .format = format,
        .usage = wgpu::TextureUsage::RenderAttachment,
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .presentMode = webgpu::select_present_mode(capabilities),
    };
    g_surface.Configure(&g_configuration);
    return true;
}

void make_surface(const Source& source) {
    release_surface();
    g_window_in_use = source.android;
    g_surface = create_surface(source);
    if (!g_surface || !configure(source.width, source.height)) {
        runtime_log::warning("second screen", "no surface can be made on the second screen; the menu stays on the game's");
        release_surface();
        return;
    }
    g_ready_width = source.width;
    g_ready_height = source.height;
    runtime_log::info("second screen", "the game's menu goes to the second screen (%dx%d)", source.width,
                      source.height);
}

// Draws `picture` into `target` with aurora's copy pipeline, in the given part of it.
void copy_picture(const wgpu::CommandEncoder& encoder, const wgpu::TextureView& target, wgpu::LoadOp load,
                  const webgpu::PresentPicture& picture, float left, float top, float width, float height,
                  const FrozenFrame* buttons) {
    const wgpu::RenderPassColorAttachment attachment{
        .view = target,
        .loadOp = load,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = BACKGROUND,
    };
    const wgpu::RenderPassDescriptor descriptor{
        .label = "Second screen",
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
    };
    const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&descriptor);
    if (picture.view && width > 0.f && height > 0.f) {
        const webgpu::TextureWithSampler source{
            .texture = picture.texture,
            .view = picture.view,
            .size = picture.size,
            .format = picture.format,
            .sampler = picture.sampler,
        };
        pass.SetPipeline(webgpu::g_CopyPipeline);
        pass.SetBindGroup(0, webgpu::create_copy_bind_group(source), 0, nullptr);
        pass.SetViewport(left, top, width, height, 0.f, 1.f);
        pass.Draw(3);
    }
    if (buttons != nullptr && buttons->data.CmdListsCount > 0) {
        // aurora's ImGui renderer: whichever ImGui context is current, it is the same one
        // (create_context()).
        ImGui_ImplWGPU_RenderDrawData(const_cast<ImDrawData*>(&buttons->data), pass.Get());
    }
    pass.End();
}

// The picture kept for the window while the menu is open: a copy of each frame's picture while it
// is closed.
void keep_picture(const wgpu::CommandEncoder& encoder, const webgpu::PresentPicture& picture) {
    if (!picture.view || picture.size.width == 0 || picture.size.height == 0) {
        return;
    }
    if (!g_kept || g_kept_size.width != picture.size.width || g_kept_size.height != picture.size.height) {
        g_kept_size = wgpu::Extent3D{picture.size.width, picture.size.height, 1};
        const wgpu::TextureDescriptor descriptor{
            .label = "Picture before the game's menu",
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
            .size = g_kept_size,
            .format = g_configuration.format,
        };
        g_kept = webgpu::g_device.CreateTexture(&descriptor);
        g_kept_view = g_kept.CreateView();
    }
    copy_picture(encoder, g_kept_view, wgpu::LoadOp::Clear, picture, 0.f, 0.f,
                 static_cast<float>(g_kept_size.width), static_cast<float>(g_kept_size.height), nullptr);
}

// The part of the second screen the menu's picture takes: as wide as it fits, at the top.
struct Area {
    float left, top, width, height;
};
Area picture_area(float screen_width, float screen_height, float aspect) {
    float width = screen_width;
    float height = width / aspect;
    if (height > screen_height * PICTURE_MOST_HEIGHT) {
        height = screen_height * PICTURE_MOST_HEIGHT;
        width = height * aspect;
    }
    return Area{(screen_width - width) * 0.5f, 0.f, width, height};
}

// The frame's picture, between the two hooks.
webgpu::PresentPicture g_frame_picture;
bool g_frame_menu = false;

// aurora's present hook, on the render thread, once a frame (second_screen.h): it only notes the
// picture, which is drawn elsewhere once the frame's commands are submitted (after_present), and
// gives the window the kept picture while the menu is open.
void before_present(const wgpu::CommandEncoder&, webgpu::PresentPicture& picture) {
    g_frame_picture = {};
    if (!g_surface) {
        return;
    }
    const float aspect = picture.aspect > 0.f ? picture.aspect
                         : picture.size.height > 0
                             ? static_cast<float>(picture.size.width) / static_cast<float>(picture.size.height)
                             : 4.f / 3.f;
    g_picture_aspect = aspect;
    g_frame_picture = picture;
    g_frame_picture.aspect = aspect;
    g_frame_menu = g_menu_shown.load();
    // The window keeps the farm as it was before the menu.
    if (g_frame_menu && g_kept) {
        picture.texture = g_kept;
        picture.view = g_kept_view;
        picture.size = g_kept_size;
        picture.format = g_configuration.format;
    }
}

// Once the frame is submitted: the picture is kept (menu closed) or drawn on the second screen
// (menu open), with commands of its own that come after the frame's.
void after_present() {
    const webgpu::PresentPicture picture = std::exchange(g_frame_picture, {});
    if (!g_surface || !picture.view) {
        return;
    }
    const bool menu = g_frame_menu;
    const wgpu::CommandEncoder encoder = webgpu::g_device.CreateCommandEncoder();
    if (!menu) {
        keep_picture(encoder, picture);
    }
    bool present = false;
    if (menu || g_showing_menu) {
        wgpu::SurfaceTexture texture;
        g_surface.GetCurrentTexture(&texture);
        if (texture.status == wgpu::SurfaceGetCurrentTextureStatus::Outdated ||
            texture.status == wgpu::SurfaceGetCurrentTextureStatus::Lost) {
            g_surface.Configure(&g_configuration);  // the screen turned or changed; the next frame shows
        } else if (texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
                   texture.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
            const wgpu::TextureView target = texture.texture.CreateView();
            if (menu) {
                std::shared_ptr<FrozenFrame> buttons;
                {
                    std::lock_guard lock(g_mutex);
                    buttons = g_buttons_frame;
                }
                const Area area = picture_area(static_cast<float>(g_configuration.width),
                                               static_cast<float>(g_configuration.height), picture.aspect);
                copy_picture(encoder, target, wgpu::LoadOp::Clear, picture, area.left, area.top, area.width,
                             area.height, buttons.get());
            } else {
                // The menu closed: the second screen goes dark until it opens again.
                copy_picture(encoder, target, wgpu::LoadOp::Clear, webgpu::PresentPicture{}, 0.f, 0.f, 0.f, 0.f,
                             nullptr);
            }
            g_showing_menu = menu;
            present = true;
        }
    }
    const wgpu::CommandBuffer commands = encoder.Finish();
    webgpu::g_queue.Submit(1, &commands);
    if (present) {
        g_surface.Present();
    }
}

// ---- The game's thread --------------------------------------------------------------------------

ImGuiContext* g_context = nullptr;
SDL_Window* g_test_window = nullptr;
std::map<int, ImVec2> g_fingers;  // the fingers on the second screen, in its pixels
// Where fingers touched lately: a tap quicker than a frame still presses its button for a few
// frames, long enough for the game to read it.
struct Tap {
    ImVec2 at;
    int frames;
};
constexpr int TAP_FRAMES = 3;
std::vector<Tap> g_taps;
bool g_hooks_set = false;

// The buttons under the menu, and what each plays.
enum class Job { Left, Up, Down, Right, L, R, B, A };
struct Button {
    Job job;
    const char* label;
    ImVec2 min;
    ImVec2 max;
};
std::vector<Button> g_buttons;  // as last laid out
// The menu turns the screen black before the game says it is open (game_scene.h), so the window
// keeps its picture from the moment Y is pressed, and gives it up if the menu does not come within
// OPENING_FRAMES.
constexpr int OPENING_FRAMES = 45;
int g_opening_frames = 0;
// The game turns the screen black between the menu's pages too, where it may not say that its menu
// is open for a frame or two: the menu stays on the second screen that long after it closes.
constexpr int CLOSING_FRAMES = 8;
int g_closing_frames = 0;
bool g_y_held = false;
bool g_player_walked = false;  // at the end of the last frame, before the game reads the controller
uint16_t g_held_buttons = 0;     // PAD_BUTTON_* the fingers hold
int g_stick_x = 0;
int g_stick_y = 0;

// A surface given since the last frame is made on the render thread. The lock is held while it is
// queued, so that a detach() that comes meanwhile is queued after it and lets it go.
void take_new_source() {
    std::lock_guard lock(g_mutex);
    if (!g_new_source) {
        return;
    }
    const Source source = *std::exchange(g_new_source, std::nullopt);
    render_worker::enqueue_work([source] { make_surface(source); });
}

// The second ImGui context: its own size, and aurora's fonts and renderer.
void create_context() {
    ImGuiContext* game = ImGui::GetCurrentContext();
    const ImGuiIO& game_io = ImGui::GetIO();
    g_context = ImGui::CreateContext(game_io.Fonts);
    ImGui::SetCurrentContext(g_context);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.BackendRendererUserData = game_io.BackendRendererUserData;
    io.BackendRendererName = "aurora's, shared";
    io.BackendFlags = game_io.BackendFlags;
    ImGui::SetCurrentContext(game);
}

// The fingers given since the last frame.
void follow_fingers(float width, float height) {
    std::vector<Pointer> pointers;
    {
        std::lock_guard lock(g_mutex);
        pointers.swap(g_pointers);
    }
    for (const Pointer& pointer : pointers) {
        if (pointer.finger == EVERY_FINGER) {
            g_fingers.clear();
            g_taps.clear();
        } else if (pointer.action == Touch::Up || pointer.action == Touch::Cancel) {
            g_fingers.erase(pointer.finger);
        } else {
            g_fingers[pointer.finger] = ImVec2(pointer.x * width, pointer.y * height);
            if (pointer.action == Touch::Down) {
                g_taps.push_back(Tap{g_fingers[pointer.finger], TAP_FRAMES});
            }
        }
    }
}

// The buttons in the band under the menu's picture, in one row of eight as on the controller: the
// stick's four ways at the left, B and A at the right, and L and R between them.
void lay_out_buttons(float width, float height) {
    const Area area = picture_area(width, height, g_picture_aspect.load());
    const float margin = width * 0.03f;
    const float top = area.height + height * 0.02f;
    const float bottom = height - height * 0.03f;
    // Eight buttons and the gaps between them (a fifth of a button, and three between the groups).
    const float size = std::min(bottom - top, (width - 2.f * margin) / (8.f + 7.f * 0.2f + 4.f * 0.2f));
    const float gap = size * 0.2f;
    const float group_gap = (width - 2.f * margin - 8.f * size - 5.f * gap) / 2.f;
    const float middle = (top + bottom) * 0.5f;
    g_buttons.clear();
    float x = margin;
    const auto add = [&](Job job, const char* label, float after) {
        g_buttons.push_back(Button{job, label, ImVec2(x, middle - size * 0.5f), ImVec2(x + size, middle + size * 0.5f)});
        x += size + after;
    };
    add(Job::Left, "", gap);
    add(Job::Up, "", gap);
    add(Job::Down, "", gap);
    add(Job::Right, "", group_gap);
    add(Job::L, "L", gap);
    add(Job::R, "R", group_gap);
    add(Job::B, "B", gap);
    add(Job::A, "A", 0.f);
}

bool held(const Button& button) {
    const auto inside = [&](ImVec2 p) {
        return p.x >= button.min.x && p.x <= button.max.x && p.y >= button.min.y && p.y <= button.max.y;
    };
    return std::any_of(g_fingers.begin(), g_fingers.end(), [&](const auto& finger) { return inside(finger.second); }) ||
           std::any_of(g_taps.begin(), g_taps.end(), [&](const Tap& tap) { return inside(tap.at); });
}

// The buttons' look: the touch buttons' (input/touch_buttons.cpp), cream with a dark blue edge,
// A green and B red as on the GameCube controller.
void draw_button(ImDrawList* draw, const Button& button, bool pressed) {
    const ImU32 edge = IM_COL32(40, 52, 110, 255);
    ImU32 fill = IM_COL32(244, 236, 214, 255);
    if (button.job == Job::A) {
        fill = IM_COL32(64, 186, 120, 255);
    } else if (button.job == Job::B) {
        fill = IM_COL32(222, 74, 70, 255);
    }
    if (pressed) {
        fill = IM_COL32(150, 150, 150, 255);
    }
    const ImVec2 center((button.min.x + button.max.x) * 0.5f, (button.min.y + button.max.y) * 0.5f);
    const float radius = (button.max.x - button.min.x) * 0.5f;
    draw->AddCircleFilled(center, radius, fill, 48);
    draw->AddCircle(center, radius, edge, 48, radius * 0.08f);
    const float r = radius * 0.45f;
    switch (button.job) {
        case Job::Left:
            draw->AddTriangleFilled(center + ImVec2(-r, 0), center + ImVec2(r * 0.6f, -r), center + ImVec2(r * 0.6f, r), edge);
            return;
        case Job::Right:
            draw->AddTriangleFilled(center + ImVec2(r, 0), center + ImVec2(-r * 0.6f, r), center + ImVec2(-r * 0.6f, -r), edge);
            return;
        case Job::Up:
            draw->AddTriangleFilled(center + ImVec2(0, -r), center + ImVec2(r, r * 0.6f), center + ImVec2(-r, r * 0.6f), edge);
            return;
        case Job::Down:
            draw->AddTriangleFilled(center + ImVec2(0, r), center + ImVec2(-r, -r * 0.6f), center + ImVec2(r, -r * 0.6f), edge);
            return;
        default: {
            ImFont* font = theme::fonts().heading;
            const float size = radius * 1.1f;
            const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.f, button.label);
            draw->AddText(font, size, center - extent * 0.5f, button.job == Job::L || button.job == Job::R ? edge : IM_COL32_WHITE,
                          button.label);
        }
    }
}

// What the held buttons play: the stick pushed all the way (the game's menus move with the stick),
// and L, R, B and A.
void play_held_buttons() {
    g_held_buttons = 0;
    g_stick_x = 0;
    g_stick_y = 0;
    for (const Button& button : g_buttons) {
        if (!held(button)) {
            continue;
        }
        switch (button.job) {
            case Job::Left: g_stick_x = -100; break;
            case Job::Right: g_stick_x = 100; break;
            case Job::Up: g_stick_y = 100; break;
            case Job::Down: g_stick_y = -100; break;
            case Job::L: g_held_buttons |= PAD_TRIGGER_L; break;
            case Job::R: g_held_buttons |= PAD_TRIGGER_R; break;
            case Job::B: g_held_buttons |= PAD_BUTTON_B; break;
            case Job::A: g_held_buttons |= PAD_BUTTON_A; break;
        }
    }
}

// The buttons, as an ImGui frame of the second context, for the render thread to draw.
void draw_buttons(int width, int height) {
    if (g_context == nullptr) {
        create_context();
    }
    ImGuiContext* game = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(g_context);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
    io.DeltaTime = 1.f / 60.f;
    ImGui::NewFrame();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (const Button& button : g_buttons) {
        draw_button(draw, button, held(button));
    }
    ImGui::Render();
    auto frozen = std::make_shared<FrozenFrame>();
    const ImDrawData* data = ImGui::GetDrawData();
    frozen->data = *data;
    frozen->data.CmdLists.resize(data->CmdListsCount);
    for (int i = 0; i < data->CmdListsCount; ++i) {
        frozen->lists.emplace_back(data->CmdLists[i]->CloneOutput());
        frozen->data.CmdLists[i] = frozen->lists.back().get();
    }
    ImGui::SetCurrentContext(game);
    std::lock_guard lock(g_mutex);
    g_buttons_frame = std::move(frozen);
}

}  // namespace

void attach_android_window(ANativeWindow* window, int width, int height) {
    std::lock_guard lock(g_mutex);
    if (g_new_source) {
        release_android_window(g_new_source->android);
    }
    g_new_source = Source{.android = window, .width = width, .height = height};
}

void detach() {
    std::lock_guard lock(g_mutex);
    if (g_new_source) {
        release_android_window(std::exchange(g_new_source, std::nullopt)->android);
    }
    // Queued after anything the game queued before, and waited for: when this returns, nothing
    // draws on the surface any more. (Without aurora's render thread, the work runs right here.)
    auto released = std::make_shared<std::promise<void>>();
    std::future<void> done = released->get_future();
    render_worker::enqueue_work([released] {
        release_surface();
        released->set_value();
    });
    if (done.wait_for(DETACH_TIMEOUT) != std::future_status::ready) {
        runtime_log::warning("second screen", "the render thread did not let go of the surface in time");
    }
    g_pointers.push_back(Pointer{.action = Touch::Cancel, .finger = EVERY_FINGER});
}

void touch(Touch action, int finger, float x, float y) {
    std::lock_guard lock(g_mutex);
    g_pointers.push_back(Pointer{.action = action, .finger = finger, .x = x, .y = y});
}

void update(bool wanted) {
    if (!g_hooks_set) {
        g_hooks_set = true;
        webgpu::set_present_hooks(before_present, after_present);
    }
    take_new_source();
#if defined(__ANDROID__)
    // The app's GameActivity.useSecondScreen(boolean), which shows or takes away the other screen.
    static std::optional<bool> told;
    if (told == wanted) {
        return;
    }
    told = wanted;
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }
    jclass type = env->GetObjectClass(activity);
    if (jmethodID method = env->GetMethodID(type, "useSecondScreen", "(Z)V")) {
        env->CallVoidMethod(activity, method, static_cast<jboolean>(wanted));
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
#else
    (void)wanted;  // the test window is there whenever HMMM_SECOND_SCREEN asks for it
#endif
}

void open_test_window_if_asked() {
    const char* asked = std::getenv("HMMM_SECOND_SCREEN");
    int width = 0;
    int height = 0;
    if (asked == nullptr || std::sscanf(asked, "%dx%d", &width, &height) != 2 || width <= 0 || height <= 0) {
        return;
    }
    g_test_window = SDL_CreateWindow("Harvest Moon MM - second screen", width, height, 0);
    if (g_test_window == nullptr) {
        runtime_log::warning("second screen", "the test window could not be opened: %s", SDL_GetError());
        return;
    }
    int pixel_width = width;
    int pixel_height = height;
    SDL_GetWindowSizeInPixels(g_test_window, &pixel_width, &pixel_height);
    std::lock_guard lock(g_mutex);
    g_new_source = Source{.test_window = g_test_window, .width = pixel_width, .height = pixel_height};
}

void handle_event(const SDL_Event& event) {
    if (g_test_window == nullptr) {
        return;
    }
    const SDL_WindowID test_window = SDL_GetWindowID(g_test_window);
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(g_test_window, &width, &height);
    const auto mouse = [&](Touch action, float x, float y) {
        touch(action, MOUSE_FINGER, x / static_cast<float>(std::max(width, 1)), y / static_cast<float>(std::max(height, 1)));
    };
    static bool pressed = false;
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID == test_window) {
        pressed = true;
        mouse(Touch::Down, event.button.x, event.button.y);
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.windowID == test_window) {
        pressed = false;
        mouse(Touch::Up, event.button.x, event.button.y);
    } else if (event.type == SDL_EVENT_MOUSE_MOTION && event.motion.windowID == test_window && pressed) {
        mouse(Touch::Move, event.motion.x, event.motion.y);
    }
}

void draw(bool game_menu_open, bool player_walks) {
    const int width = g_ready_width.load();
    const int height = g_ready_height.load();
    if (game_menu_open) {
        g_opening_frames = 0;
        g_closing_frames = CLOSING_FRAMES;
    } else if (g_opening_frames > 0) {
        --g_opening_frames;
    } else if (g_closing_frames > 0) {
        --g_closing_frames;
    }
    g_player_walked = player_walks;
    const bool shown =
        (game_menu_open || g_opening_frames > 0 || g_closing_frames > 0) && width > 0 && height > 0;
    g_menu_shown = shown;
    follow_fingers(static_cast<float>(width), static_cast<float>(height));
    if (!shown) {
        g_fingers.clear();
        g_taps.clear();
        g_buttons.clear();
        g_held_buttons = 0;
        g_stick_x = 0;
        g_stick_y = 0;
        return;
    }
    lay_out_buttons(static_cast<float>(width), static_cast<float>(height));
    play_held_buttons();
    draw_buttons(width, height);
    for (Tap& tap : g_taps) {
        --tap.frames;
    }
    std::erase_if(g_taps, [](const Tap& tap) { return tap.frames <= 0; });
}

void add_to(PADStatus& status) {
    // Y pressed while walking opens the menu, from this very frame (the one the game reads it in).
    const bool y = (status.button & PAD_BUTTON_Y) != 0;
    if (y && !g_y_held && g_player_walked && available()) {
        g_opening_frames = OPENING_FRAMES + 1;
    }
    g_y_held = y;
    if (g_held_buttons == 0 && g_stick_x == 0 && g_stick_y == 0) {
        return;
    }
    status.err = PAD_ERR_NONE;
    status.button |= g_held_buttons;
    if (g_held_buttons & PAD_TRIGGER_L) {
        status.triggerLeft = 255;
    }
    if (g_held_buttons & PAD_TRIGGER_R) {
        status.triggerRight = 255;
    }
    if (g_stick_x != 0) {
        status.stickX = static_cast<s8>(g_stick_x);
    }
    if (g_stick_y != 0) {
        status.stickY = static_cast<s8>(g_stick_y);
    }
}

void shutdown() {
    webgpu::set_present_hooks(nullptr, nullptr);
    render_worker::enqueue_work(release_surface);
    render_worker::synchronize();
    if (g_test_window != nullptr) {
        SDL_DestroyWindow(std::exchange(g_test_window, nullptr));
    }
}

bool available() { return g_ready_width.load() > 0; }

}  // namespace second_screen

#if defined(__ANDROID__)
// The app's side (android/app/src/main/java/io/github/rodoly/hmmmgc/SecondScreen.java).

extern "C" JNIEXPORT void JNICALL Java_io_github_rodoly_hmmmgc_SecondScreen_nativeAttach(JNIEnv* env, jclass,
                                                                                         jobject surface, jint width,
                                                                                         jint height) {
    // ANativeWindow_fromSurface gives a reference, which the second screen keeps.
    if (ANativeWindow* window = ANativeWindow_fromSurface(env, surface)) {
        second_screen::attach_android_window(window, width, height);
    }
}

extern "C" JNIEXPORT void JNICALL Java_io_github_rodoly_hmmmgc_SecondScreen_nativeDetach(JNIEnv*, jclass) {
    second_screen::detach();
}

extern "C" JNIEXPORT void JNICALL Java_io_github_rodoly_hmmmgc_SecondScreen_nativeTouch(JNIEnv*, jclass, jint action,
                                                                                        jint finger, jfloat x,
                                                                                        jfloat y) {
    second_screen::touch(static_cast<second_screen::Touch>(action), finger, x, y);
}
#endif
