// Finding the player's disc image by itself, so that a player with only a controller does not need
// the system's file dialog: the launcher looks, in the background, in the folders where a disc
// image usually is and keeps the ones of this game.
//
// Encontrar sola la imagen del disco del jugador, para que quien solo tiene un control no necesite
// la ventana de archivos del sistema: el launcher busca, en segundo plano, en las carpetas donde
// suele estar una imagen del disco y se queda con las de este juego.
//
// Where it looks: the launcher's own folder, Downloads, Documents, the Desktop, the user's folder
// and Games in it, and the folders of Dolphin's game list (ISOPath0, ISOPath1, ... in Dolphin.ini),
// each a few folders deep. A file counts when it is a disc image of this game: its code, read from
// its header (game_code), is G4AEE9.
#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// The code of the game on a GameCube disc image ("G4AEE9"), or empty when it is not one or the
// code cannot be read without unpacking it. It reads .iso and .gcm as they are, the copy of the
// disc's header that .rvz and .wia keep in their own header, and the first block of a .ciso. A .gcz
// keeps no header that can be read, so it gives nothing.
std::string game_code(const std::filesystem::path& disc_image);

class DiscSearch {
public:
    ~DiscSearch();

    // Starts looking for images of the disc with `code`; `launcher_folder` is one of the places.
    void start(const std::filesystem::path& launcher_folder, const std::string& code);
    bool finished() const { return finished_; }
    // What it has found so far, as UTF-8 paths, in the order of the places above.
    std::vector<std::string> found() const;

private:
    void look(std::vector<std::filesystem::path> places, std::string code);

    std::thread thread_;
    std::atomic<bool> finished_{false};
    std::atomic<bool> stop_{false};
    mutable std::mutex mutex_;
    std::vector<std::string> found_;
};
