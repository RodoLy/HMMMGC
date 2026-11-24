package io.github.rodoly.hmmmgc;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;

import dev.encounter.aurora.AuroraSurface;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/**
 * The game: SDL's activity, which loads libmain.so (the game compiled for Android on the player's
 * computer) and calls its SDL_main with the arguments below, as the native runtime takes them on a
 * computer. It draws on aurora's surface, which tells aurora when the window can be drawn on.
 *
 * El juego: la actividad de SDL, que carga libmain.so y llama a su SDL_main con estos argumentos.
 */
public class GameActivity extends SDLActivity {
    /**
     * Called by the game (runtime/src/android_app.cpp, the F10 menu's "Send the log") on its own
     * thread: Android's share sheet with native.log, which LogProvider lets the chosen app read.
     */
    public void shareLog() {
        runOnUiThread(() -> {
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("text/plain");
            send.putExtra(Intent.EXTRA_STREAM, LogProvider.LOG);
            send.putExtra(Intent.EXTRA_SUBJECT, "Harvest Moon MM native.log");
            send.setClipData(ClipData.newRawUri("native.log", LogProvider.LOG));
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivity(Intent.createChooser(send, getString(R.string.send_log)));
        });
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        return new AuroraSurface(context);
    }

    @Override
    protected String[] getArguments() {
        return new String[] {
            "--disc", Storage.disc(this).getPath(),
            "--user-folder", Storage.playerFolder(this).getPath(),
            // A phone that is slow to draw the first frames is not a game that stopped.
            "--watchdog", "0",
        };
    }
}
