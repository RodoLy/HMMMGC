package io.github.rodoly.hmmmgc;

import android.content.Context;

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
