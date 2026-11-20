package io.github.rodoly.hmmmgc;

import android.content.Context;
import android.os.Bundle;

import dev.encounter.aurora.AuroraSurface;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/**
 * The game: SDL's activity, which loads libmain.so (the game compiled for Android on the player's
 * computer) and calls its SDL_main with the arguments below, as the native runtime takes them on a
 * computer. It draws on aurora's surface, which tells aurora when the window can be drawn on.
 *
 * On a handheld with two screens, the game's menu goes to the other one (SecondScreen).
 *
 * El juego: la actividad de SDL, que carga libmain.so y llama a su SDL_main con estos argumentos.
 * En una consola con dos pantallas, el menú del juego va a la otra (SecondScreen).
 */
public class GameActivity extends SDLActivity {
    private SecondScreen secondScreen;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);  // loads libmain.so, which SecondScreen's native methods are in
        secondScreen = new SecondScreen(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (secondScreen != null) {
            secondScreen.start();
        }
    }

    @Override
    protected void onStop() {
        if (secondScreen != null) {
            secondScreen.stop();
        }
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        if (secondScreen != null) {
            secondScreen.destroy();
        }
        super.onDestroy();
    }

    /**
     * Called by the game (runtime/src/graphics/second_screen.cpp) on its own thread: whether the
     * game's menu goes to the other screen, from the option "No second screen".
     */
    public void useSecondScreen(boolean wanted) {
        runOnUiThread(() -> {
            if (secondScreen != null) {
                secondScreen.setWanted(wanted);
            }
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
