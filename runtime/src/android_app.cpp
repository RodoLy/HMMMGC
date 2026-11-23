// See android_app.h.

#include "android_app.h"

#if defined(__ANDROID__)
#include <SDL3/SDL_system.h>
#include <jni.h>

#include <cstdio>
#endif

namespace android_app {

#if defined(__ANDROID__)
namespace {

// Calls a method of the game's activity that takes nothing and gives nothing back.
void call_activity(const char* method_name) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    jobject activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }
    jclass type = env->GetObjectClass(activity);
    if (jmethodID method = env->GetMethodID(type, method_name, "()V")) {
        env->CallVoidMethod(activity, method);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}

}  // namespace

void share_log() {
    std::fflush(stderr);  // the log goes to native.log (main.cpp); all of it is in the file first
    call_activity("shareLog");
}
#else
void share_log() {}
#endif

}  // namespace android_app
