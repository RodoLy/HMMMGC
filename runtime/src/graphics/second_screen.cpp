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

#include <SDL3/SDL_timer.h>
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
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace second_screen {
namespace {

namespace render_worker = aurora::gfx::render_worker;
namespace webgpu = aurora::webgpu;

// The menu on the second screen is laid out for this many of the theme's design pixels
// (shared/theme.h), as the game's screen is for 1280x720: on the AYN Thor's lower screen
// (1240x1080) that is about one and a half times the design, big enough for a finger.
constexpr float DESIGN_WIDTH = 800.f;
constexpr float DESIGN_HEIGHT = 700.f;

// The second screen is drawn again at most this often: a menu needs no more, and every frame
// drawn there is time taken from the game's.
constexpr Uint64 FRAME_INTERVAL_NS = 1'000'000'000 / 30;

// How long Android waits, at most, for the second screen to let go of its surface.
constexpr auto DETACH_TIMEOUT = std::chrono::seconds(1);

// The colour of the screen under the menu: the dark of ImGui's own windows.
constexpr wgpu::Color BACKGROUND{0.06, 0.06, 0.07, 1.0};

// Where a surface comes from: the app's window on the other screen, or the test window.
struct Source {
    ANativeWindow* android = nullptr;  // one reference of it belongs to the second screen
    SDL_Window* test_window = nullptr;
    int width = 0;
    int height = 0;
};

// A finger of the app, or the test window's mouse (which moves without being pressed).
struct Pointer {
    bool mouse = false;
    Touch action = Touch::Move;
    int finger = 0;
    float x = 0.f;  // from 0 to 1 of the screen
    float y = 0.f;
};

// A key of the test window's keyboard, for ImGui.
struct Key {
    ImGuiKey key = ImGuiKey_None;
    bool down = false;
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

// ---- Shared between the app's thread and the game's -------------------------------------------

std::mutex g_mutex;
std::optional<Source> g_new_source;  // given, and not yet made into a surface
std::vector<Pointer> g_pointers;     // not yet given to ImGui
std::vector<Key> g_keys;

// Set by the render thread: the size of the surface that is ready to draw on, 0 when none is.
std::atomic<int> g_ready_width{0};
std::atomic<int> g_ready_height{0};

// ---- The render thread's ------------------------------------------------------------------------

wgpu::Surface g_surface;
wgpu::SurfaceConfiguration g_configuration;
ANativeWindow* g_window_in_use = nullptr;

void release_surface() {
    if (g_surface) {
        g_surface.Unconfigure();
        g_surface = nullptr;
    }
    release_android_window(std::exchange(g_window_in_use, nullptr));
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

// The surface takes the format of aurora's own, because the same ImGui pipelines draw on both.
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
    runtime_log::info("second screen", "the options menu is on the second screen (%dx%d)", source.width,
                      source.height);
}

// One frame of ImGui, kept while the render thread draws it: ImGui's lists are copied, as aurora
// does with its own frames (aurora-main/lib/imgui.cpp).
struct FrozenFrame {
    ImDrawData data;
    std::vector<std::unique_ptr<ImDrawList>> lists;
};

void draw_frame(const FrozenFrame& frame) {
    if (!g_surface) {
        return;
    }
    wgpu::SurfaceTexture texture;
    g_surface.GetCurrentTexture(&texture);
    if (texture.status == wgpu::SurfaceGetCurrentTextureStatus::Outdated ||
        texture.status == wgpu::SurfaceGetCurrentTextureStatus::Lost) {
        g_surface.Configure(&g_configuration);  // the screen turned or changed; the next frame shows
        return;
    }
    if (texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
        return;
    }
    const wgpu::TextureView view = texture.texture.CreateView();
    const wgpu::CommandEncoder encoder = webgpu::g_device.CreateCommandEncoder();
    const wgpu::RenderPassColorAttachment attachment{
        .view = view,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = BACKGROUND,
    };
    const wgpu::RenderPassDescriptor pass_descriptor{
        .label = "Second screen",
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment,
    };
    const wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_descriptor);
    // aurora's ImGui renderer: whichever ImGui context is current, it is the same one (frame()).
    ImGui_ImplWGPU_RenderDrawData(const_cast<ImDrawData*>(&frame.data), pass.Get());
    pass.End();
    const wgpu::CommandBuffer commands = encoder.Finish();
    webgpu::g_queue.Submit(1, &commands);
    g_surface.Present();
}

// ---- The game's thread --------------------------------------------------------------------------

ImGuiContext* g_context = nullptr;
SDL_Window* g_test_window = nullptr;
int g_finger = -1;          // the finger that is ImGui's mouse, -1 when none
Uint64 g_last_frame_ns = 0;  // when the last frame was queued
bool g_had_controller = false;

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

// The second ImGui context: its own size and input, and aurora's fonts and renderer.
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
    io.BackendFlags = game_io.BackendFlags | ImGuiBackendFlags_HasGamepad;
    ImGui::SetCurrentContext(game);
}

// The pointers and keys given since the last frame, as ImGui's mouse and keyboard, on a layout of
// width x height.
void give_pointers(ImGuiIO& io, float width, float height) {
    std::vector<Pointer> pointers;
    std::vector<Key> keys;
    {
        std::lock_guard lock(g_mutex);
        pointers.swap(g_pointers);
        keys.swap(g_keys);
    }
    for (const Pointer& pointer : pointers) {
        const ImVec2 at(pointer.x * width, pointer.y * height);
        if (pointer.mouse) {
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMousePosEvent(at.x, at.y);
            if (pointer.action == Touch::Down || pointer.action == Touch::Up) {
                io.AddMouseButtonEvent(0, pointer.action == Touch::Down);
            }
            continue;
        }
        // The first finger down is the mouse until it lifts; the others are not seen.
        if (pointer.action == Touch::Down && g_finger < 0) {
            g_finger = pointer.finger;
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(at.x, at.y);
            io.AddMouseButtonEvent(0, true);
        } else if (pointer.finger == g_finger) {
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(at.x, at.y);
            if (pointer.action == Touch::Up || pointer.action == Touch::Cancel) {
                io.AddMouseButtonEvent(0, false);
                io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);  // a lifted finger hovers nothing
                g_finger = -1;
            }
        }
    }
    for (const Key& key : keys) {
        io.AddKeyEvent(key.key, key.down);
    }
}

// The controller's buttons and left stick as ImGui's gamepad, as ImGui's own SDL backend gives
// them to the game screen's menu; all let go when the menu does not have the controller.
void give_controller(ImGuiIO& io, SDL_Gamepad* controller) {
    struct Button {
        ImGuiKey key;
        SDL_GamepadButton button;
    };
    static constexpr Button BUTTONS[] = {
        {ImGuiKey_GamepadStart, SDL_GAMEPAD_BUTTON_START},
        {ImGuiKey_GamepadBack, SDL_GAMEPAD_BUTTON_BACK},
        {ImGuiKey_GamepadFaceLeft, SDL_GAMEPAD_BUTTON_WEST},
        {ImGuiKey_GamepadFaceRight, SDL_GAMEPAD_BUTTON_EAST},
        {ImGuiKey_GamepadFaceUp, SDL_GAMEPAD_BUTTON_NORTH},
        {ImGuiKey_GamepadFaceDown, SDL_GAMEPAD_BUTTON_SOUTH},
        {ImGuiKey_GamepadDpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT},
        {ImGuiKey_GamepadDpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
        {ImGuiKey_GamepadDpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP},
        {ImGuiKey_GamepadDpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN},
        {ImGuiKey_GamepadL1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
        {ImGuiKey_GamepadR1, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    };
    for (const Button& button : BUTTONS) {
        io.AddKeyEvent(button.key, controller != nullptr && SDL_GetGamepadButton(controller, button.button));
    }
    // The stick counts from a quarter of the way, as in ImGui's SDL backend.
    const auto stick = [&](ImGuiKey key, SDL_GamepadAxis axis, float sign) {
        const float value =
            controller != nullptr ? sign * static_cast<float>(SDL_GetGamepadAxis(controller, axis)) / 32767.f : 0.f;
        const float pushed = std::clamp((value - 0.25f) / 0.75f, 0.f, 1.f);
        io.AddKeyAnalogEvent(key, pushed > 0.1f, pushed);
    };
    stick(ImGuiKey_GamepadLStickLeft, SDL_GAMEPAD_AXIS_LEFTX, -1.f);
    stick(ImGuiKey_GamepadLStickRight, SDL_GAMEPAD_AXIS_LEFTX, 1.f);
    stick(ImGuiKey_GamepadLStickUp, SDL_GAMEPAD_AXIS_LEFTY, -1.f);
    stick(ImGuiKey_GamepadLStickDown, SDL_GAMEPAD_AXIS_LEFTY, 1.f);
}

// The test window's keys that move in the menu.
ImGuiKey imgui_key(SDL_Keycode key) {
    switch (key) {
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_TAB: return ImGuiKey_Tab;
        default: return ImGuiKey_None;
    }
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
    g_pointers.clear();
}

void touch(Touch action, int finger, float x, float y) {
    std::lock_guard lock(g_mutex);
    g_pointers.push_back(Pointer{.action = action, .finger = finger, .x = x, .y = y});
}

void update(bool wanted) {
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
    const auto pointer = [&](Touch action, float x, float y) {
        std::lock_guard lock(g_mutex);
        g_pointers.push_back(Pointer{.mouse = true,
                                     .action = action,
                                     .x = x / static_cast<float>(std::max(width, 1)),
                                     .y = y / static_cast<float>(std::max(height, 1))});
    };
    switch (event.type) {
        case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.windowID == test_window) {
                pointer(Touch::Move, event.motion.x, event.motion.y);
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.windowID == test_window && event.button.button == SDL_BUTTON_LEFT) {
                pointer(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? Touch::Down : Touch::Up, event.button.x,
                        event.button.y);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.windowID == test_window && imgui_key(event.key.key) != ImGuiKey_None) {
                std::lock_guard lock(g_mutex);
                g_keys.push_back(Key{imgui_key(event.key.key), event.type == SDL_EVENT_KEY_DOWN});
            }
            break;
        default:
            break;
    }
}

void shutdown() {
    render_worker::enqueue_work(release_surface);
    render_worker::synchronize();
    if (g_test_window != nullptr) {
        SDL_DestroyWindow(std::exchange(g_test_window, nullptr));
    }
}

bool available() { return g_ready_width.load() > 0; }

void frame(SDL_Gamepad* controller, const std::function<void(ImVec2 size)>& draw) {
    const int width = g_ready_width.load();
    const int height = g_ready_height.load();
    if (width <= 0 || height <= 0) {
        return;
    }
    // Not more often than FRAME_INTERVAL_NS, unless the controller just came or went.
    const Uint64 now = SDL_GetTicksNS();
    const bool has_controller = controller != nullptr;
    if (now - g_last_frame_ns < FRAME_INTERVAL_NS && has_controller == g_had_controller) {
        return;
    }
    const float delta = g_last_frame_ns == 0 ? 1.f / 30.f : static_cast<float>(now - g_last_frame_ns) / 1e9f;
    g_last_frame_ns = now;
    g_had_controller = has_controller;
    if (g_context == nullptr) {
        create_context();
    }

    // The layout is in the game screen's ImGui units (its fonts are built for theme::px), made
    // larger or smaller on the way to the screen so that it fills it at the second screen's scale.
    const float game_scale = theme::px(1.f);
    const float wanted_scale =
        std::max(0.5f, std::min(static_cast<float>(width) / DESIGN_WIDTH, static_cast<float>(height) / DESIGN_HEIGHT));
    const float to_pixels = wanted_scale / game_scale;
    const ImVec2 size(static_cast<float>(width) / to_pixels, static_cast<float>(height) / to_pixels);

    ImGuiContext* game = ImGui::GetCurrentContext();
    ImFont* game_font = ImGui::GetIO().FontDefault;
    ImGui::SetCurrentContext(g_context);
    ImGuiIO& io = ImGui::GetIO();
    io.FontDefault = game_font;  // the fonts are built again when the game's window changes size
    io.DisplaySize = size;
    io.DisplayFramebufferScale = ImVec2(to_pixels, to_pixels);
    io.DeltaTime = std::max(delta, 1e-4f);
    if (has_controller) {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    } else {
        io.ConfigFlags &= ~(ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);
    }
    give_pointers(io, size.x, size.y);
    give_controller(io, controller);

    ImGui::NewFrame();
    draw(size);
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
    render_worker::enqueue_work([frozen] { draw_frame(*frozen); });
}

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
