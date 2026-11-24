// The launcher's update: when the launcher opens, it asks GitHub, in the background, which is the
// newest published version; when it is newer than this package, the game's tab offers "Update to
// vX.Y.Z"; that row shows what the new version brings (notes()), and "Update" downloads the
// package for this system, unpacks it and puts it over this one, and the launcher starts again. The player's things (saves, options, controls, the prepared
// game) are in their own folder (paths.h) and stay; the game is prepared again afterwards, as after
// any new package.
//
// La actualización del launcher: al abrirse le pregunta a GitHub, en segundo plano, cuál es la
// versión publicada más nueva; si es más nueva que este paquete, la pestaña del juego ofrece
// "Actualizar a vX.Y.Z", que enseña las novedades y, con "Actualizar", baja el paquete de este
// sistema, lo desempaca, lo pone sobre este y vuelve a abrir el launcher. Lo del jugador (partidas, opciones, controles) está en su propia
// carpeta y se queda.
//
// How
// ---
// The system's own curl and tar do the network and the unpacking: Windows 10 (1803 and newer),
// macOS and Linux bring both, and Windows' tar opens zips too. They run as child processes on a
// thread, so the launcher never waits for them. Without a connection, or when GitHub is slow, the
// check gives up after a few seconds and nothing shows: the launcher works as always. Only the
// update row downloads anything. A developer's build ("dev") never checks.
//
// Putting the new files over the old ones: each old file is renamed first (Windows lets a running
// program be renamed, not overwritten) and the new one copied in its place; the renamed ones are
// deleted the next time the launcher starts. On a Mac the package is HarvestMoonMM.app and the
// texts beside it, so what is replaced is the folder that holds the app.
#pragma once

#include "paths.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Updater {
public:
    enum class State {
        Idle,
        Checking,
        UpToDate,     // or the check failed: nothing to offer
        Available,    // latest() is newer, and has a package for this system
        Downloading,
        Installing,
        Installed,    // restart() opens the new launcher
        Failed,       // error() says why
    };

    ~Updater();

    // Asks GitHub for the newest version, on a thread. `current` is this package's ("v0.4.2").
    void check(const std::string& current);
    // Downloads the newest package and puts it over `package`, on a thread. `work` is a folder of
    // the player's where the download and its unpacking go (and are deleted after).
    void install(const paths::Package& package, const std::filesystem::path& work);
    // Starts the new launcher; the caller then quits.
    bool restart(const paths::Package& package);
    // Deletes what an update left behind: the renamed old files, and its work folder.
    static void clean_up(const paths::Package& package, const std::filesystem::path& work);

    State state() const { return state_; }
    std::string latest() const;
    // The size of the download, and how much of it has arrived (0 to 1).
    std::uint64_t size() const { return size_; }
    float progress() const;
    std::string error() const;
    // What the newest version brings, from its release notes: the section of the launcher's
    // language ("## Novedades" or "## What's new"), as lines without Markdown. Each point of a
    // list starts with "• ". Empty when the notes have no such section and no other text.
    std::vector<std::string> notes(bool spanish) const;

private:
    void check_on_thread(std::string current);
    void install_on_thread(paths::Package package, std::filesystem::path work);
    void fail(const std::string& why);

    std::thread thread_;
    std::atomic<State> state_{State::Idle};
    mutable std::mutex mutex_;
    std::string latest_;
    std::string url_;
    std::string asset_;
    std::string error_;
    std::string body_;  // the release notes, in Markdown
    std::atomic<std::uint64_t> size_{0};
    std::atomic<std::uint64_t> arrived_{0};
};
