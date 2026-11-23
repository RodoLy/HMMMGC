// What the game asks of the Android app around it (android/app/src/main/java/.../GameActivity.java),
// through JNI. Off Android these do nothing.
//
// Lo que el juego le pide a la app de Android que lo envuelve, por JNI. Fuera de Android no hacen
// nada.
#pragma once

namespace android_app {

// Opens Android's share sheet with native.log, the game's log, which lives in the app's own storage
// where no file manager reaches it: the player can send it by mail, a chat or to a GitHub issue.
void share_log();

}  // namespace android_app
