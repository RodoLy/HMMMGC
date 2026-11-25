// See child_process.h. Inside the Android app the launcher starts no other program (the game is in
// the same one, in_app.h), and an app may not start programs anyway: the parts of the launcher that
// would (preparing the game, the update) are never offered there, and this says so if they ask.
//
// Dentro de la app de Android el launcher no arranca otros programas: el juego está en el mismo.

#include "child_process.h"

std::unique_ptr<ChildProcess> ChildProcess::start(const Options& /*options*/, std::string& error) {
    error = "programs are not started inside the Android app";
    return nullptr;
}
