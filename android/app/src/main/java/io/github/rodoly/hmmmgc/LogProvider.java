package io.github.rodoly.hmmmgc;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Gives the game's log (native.log, in the player's folder of the app's own storage) to the app the
 * player sends it with, read-only and only when GameActivity.shareLog grants it: the log lives
 * where no file manager reaches it, and a bug report needs it.
 *
 * Le da el registro del juego (native.log) a la app con la que el jugador lo envía, solo para leer
 * y solo cuando GameActivity.shareLog lo permite.
 */
public final class LogProvider extends ContentProvider {
    static final String AUTHORITY = "io.github.rodoly.hmmmgc.log";
    static final Uri LOG = Uri.parse("content://" + AUTHORITY + "/native.log");

    private File log() {
        return new File(Storage.playerFolder(getContext()), "native.log");
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        return ParcelFileDescriptor.open(log(), ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        return "text/plain";
    }

    // The name and size, which the apps that receive a file ask for.
    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] arguments, String order) {
        MatrixCursor cursor = new MatrixCursor(new String[] {OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE});
        cursor.addRow(new Object[] {"native.log", log().length()});
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("read-only");
    }

    @Override
    public int delete(Uri uri, String selection, String[] arguments) {
        throw new UnsupportedOperationException("read-only");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] arguments) {
        throw new UnsupportedOperationException("read-only");
    }
}
