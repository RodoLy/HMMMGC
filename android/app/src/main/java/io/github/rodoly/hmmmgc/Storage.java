package io.github.rodoly.hmmmgc;

import android.content.Context;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

/**
 * Where the app keeps the player's things, all in its own storage (no permission is needed for it,
 * and uninstalling the app removes it):
 *
 * <pre>
 *   files/disc/G4AEE9.iso        the disc image, copied once from the one the player chose
 *   files/HarvestMoonMM/         the player's folder: settings.json, the saves, native.log
 * </pre>
 *
 * Dónde guarda la app las cosas del jugador, todas en su propio almacenamiento.
 */
final class Storage {
    /** The disc of the game, as its header starts: the game's code, then its maker. */
    static final String GAME_ID = "G4AEE9";
    /** How big a whole GameCube disc is, which a copied image must be. */
    static final long DISC_SIZE = 1_459_978_240L;

    private Storage() {}

    static File disc(Context context) {
        return new File(new File(context.getFilesDir(), "disc"), GAME_ID + ".iso");
    }

    static File playerFolder(Context context) {
        return new File(context.getFilesDir(), "HarvestMoonMM");
    }

    /** Whether the copied disc is there, whole, and is the game. */
    static boolean hasTheDisc(Context context) {
        File disc = disc(context);
        if (disc.length() != DISC_SIZE) {
            return false;
        }
        try (InputStream in = new FileInputStream(disc)) {
            return isTheGame(in);
        } catch (IOException e) {
            return false;
        }
    }

    /** Whether a disc image starts with the game's code (it is read from the stream's start). */
    static boolean isTheGame(InputStream in) throws IOException {
        byte[] header = new byte[GAME_ID.length()];
        int read = 0;
        while (read < header.length) {
            int count = in.read(header, read, header.length - read);
            if (count < 0) {
                return false;
            }
            read += count;
        }
        return GAME_ID.equals(new String(header, StandardCharsets.US_ASCII));
    }

    /**
     * The settings that the player chose on the computer where the app was made, which the app
     * carries (assets/settings.json): copied into the player's folder the first time, and then the
     * game's options menu changes them there.
     */
    static void putTheFirstSettings(Context context) {
        File folder = playerFolder(context);
        File settings = new File(folder, "settings.json");
        if (settings.exists()) {
            return;
        }
        folder.mkdirs();
        try (InputStream in = context.getAssets().open("settings.json");
                OutputStream out = new FileOutputStream(settings)) {
            byte[] buffer = new byte[8192];
            for (int count; (count = in.read(buffer)) > 0; ) {
                out.write(buffer, 0, count);
            }
        } catch (IOException e) {
            // An app made without settings: the game's own defaults.
        }
    }
}
