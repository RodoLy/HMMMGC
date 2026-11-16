package io.github.rodoly.hmmmgc;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * The first screen. When the disc image is already in the app's storage it only starts the game.
 * The first time, it asks for the image (.iso or .gcm, which the computer that made the app can
 * give: it converts an .rvz), checks that it is Harvest Moon: Magical Melody for the USA, and copies
 * it into the app's storage, showing how far the copy is.
 *
 * La primera pantalla. Si la imagen del disco ya está en el almacenamiento de la app, solo arranca
 * el juego. La primera vez la pide, comprueba que sea el juego y la copia.
 */
public class DiscActivity extends Activity {
    private static final int CHOOSE_DISC = 1;

    private final Handler main = new Handler(Looper.getMainLooper());
    private TextView message;
    private ProgressBar progress;
    private Button choose;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        Storage.putTheFirstSettings(this);
        if (Storage.hasTheDisc(this)) {
            startTheGame();
            return;
        }
        setContentView(R.layout.disc);
        message = findViewById(R.id.message);
        progress = findViewById(R.id.progress);
        choose = findViewById(R.id.choose);
        choose.setOnClickListener(view -> chooseTheDisc());
    }

    private void chooseTheDisc() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, CHOOSE_DISC);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == CHOOSE_DISC && result == RESULT_OK && data != null && data.getData() != null) {
            copy(data.getData());
        }
    }

    /** Copies the chosen image on a thread of its own; the screen shows how far it is. */
    private void copy(Uri chosen) {
        choose.setEnabled(false);
        progress.setVisibility(View.VISIBLE);
        progress.setProgress(0);
        message.setText(R.string.checking);
        new Thread(() -> {
            int problem = copyTheDisc(chosen);
            main.post(() -> {
                if (problem == 0) {
                    startTheGame();
                } else {
                    progress.setVisibility(View.INVISIBLE);
                    choose.setEnabled(true);
                    message.setText(problem);
                }
            });
        }, "copy-disc").start();
    }

    /** 0 when the disc is copied, or the text that says what is wrong. */
    private int copyTheDisc(Uri chosen) {
        File disc = Storage.disc(this);
        File partial = new File(disc.getPath() + ".part");
        disc.getParentFile().mkdirs();
        try (InputStream check = getContentResolver().openInputStream(chosen)) {
            if (check == null || !Storage.isTheGame(check)) {
                return R.string.not_the_game;
            }
        } catch (IOException e) {
            return R.string.not_read;
        }
        if (disc.getParentFile().getUsableSpace() < Storage.DISC_SIZE + 64L * 1024 * 1024) {
            return R.string.no_space;
        }
        long copied = 0;
        try (InputStream in = getContentResolver().openInputStream(chosen);
                OutputStream out = new FileOutputStream(partial)) {
            byte[] buffer = new byte[1 << 20];
            for (int count; (count = in.read(buffer)) > 0; ) {
                out.write(buffer, 0, count);
                copied += count;
                final int percent = (int) (100 * copied / Storage.DISC_SIZE);
                main.post(() -> {
                    progress.setProgress(percent);
                    message.setText(getString(R.string.copying, percent));
                });
            }
        } catch (IOException e) {
            partial.delete();
            return R.string.not_read;
        }
        if (copied != Storage.DISC_SIZE) {
            // A compressed image (.rvz) or an incomplete one: the game reads whole discs.
            partial.delete();
            return R.string.not_whole;
        }
        return partial.renameTo(disc) ? 0 : R.string.not_read;
    }

    private void startTheGame() {
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }
}
