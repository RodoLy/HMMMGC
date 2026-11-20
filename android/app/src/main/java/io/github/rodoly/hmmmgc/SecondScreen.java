package io.github.rodoly.hmmmgc;

import android.app.Activity;
import android.app.Presentation;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

/**
 * The other screen of a handheld with two, such as the AYN Thor's lower one: the game's own menu
 * (the one Y opens) goes there, with buttons for the finger under it, while the upper screen keeps
 * the farm as it was when the menu opened (runtime/src/graphics/second_screen.h).
 *
 * It is an Android Presentation: a window on another display that belongs to the game's activity,
 * with a SurfaceView whose surface goes to the game (nativeAttach) and whose fingers press the
 * buttons under the menu (nativeTouch). The window never takes the focus, so the controller keeps
 * playing the game.
 *
 * Only a display that Android lists as one for presentations is used, and only when it is not
 * bigger than the game's own: a phone connected to a television keeps the menu on the phone.
 *
 * La otra pantalla de una consola con dos, como la de abajo del AYN Thor: ahí va el menú del propio
 * juego (el que abre Y), con botones para el dedo debajo, y arriba se queda el rancho como estaba.
 * Es una Presentation de Android que nunca toma el foco, así que el control sigue jugando.
 */
final class SecondScreen implements DisplayManager.DisplayListener {
    private static final String TAG = "HMMM second screen";

    private final Activity activity;
    private final DisplayManager displays;
    private MenuPresentation presentation;
    // The game says whether it wants it (the option "No second screen"), on its first frame.
    private boolean wanted = false;
    private boolean started = false;

    SecondScreen(Activity activity) {
        this.activity = activity;
        this.displays = (DisplayManager) activity.getSystemService(Context.DISPLAY_SERVICE);
        displays.registerDisplayListener(this, new Handler(Looper.getMainLooper()));
    }

    /** From the game (GameActivity.useSecondScreen), on the app's thread. */
    void setWanted(boolean wanted) {
        this.wanted = wanted;
        refresh();
    }

    /** While the game's activity shows. */
    void start() {
        started = true;
        refresh();
    }

    void stop() {
        started = false;
        refresh();
    }

    void destroy() {
        displays.unregisterDisplayListener(this);
        stop();
    }

    @Override
    public void onDisplayAdded(int displayId) {
        refresh();
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        refresh();
    }

    @Override
    public void onDisplayChanged(int displayId) {
        refresh();
    }

    // The presentation is on the display it should be on, or gone.
    private void refresh() {
        Display other = wanted && started ? otherDisplay() : null;
        if (presentation != null && (other == null || presentation.getDisplay().getDisplayId() != other.getDisplayId())) {
            presentation.dismiss();
            presentation = null;
        }
        if (other == null || presentation != null) {
            return;
        }
        try {
            presentation = new MenuPresentation(activity, other);
            presentation.show();
            Log.i(TAG, "the menu goes to display " + other.getDisplayId() + " (" + other.getName() + ")");
        } catch (WindowManager.InvalidDisplayException e) {
            Log.w(TAG, "display " + other.getDisplayId() + " does not take the menu", e);
            presentation = null;
        }
    }

    private Display otherDisplay() {
        Display own = ownDisplay();
        for (Display display : displays.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION)) {
            if (display.getDisplayId() != own.getDisplayId() && display.isValid() && area(display) <= area(own)) {
                return display;
            }
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private Display ownDisplay() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.R ? activity.getDisplay()
                                                              : activity.getWindowManager().getDefaultDisplay();
    }

    private static long area(Display display) {
        Display.Mode mode = display.getMode();
        return (long) mode.getPhysicalWidth() * mode.getPhysicalHeight();
    }

    /** The window on the other display: one surface, all of it, for the game's menu. */
    private static final class MenuPresentation extends Presentation
            implements SurfaceHolder.Callback, View.OnTouchListener {

        MenuPresentation(Context context, Display display) {
            super(context, display);
        }

        @Override
        protected void onCreate(Bundle state) {
            super.onCreate(state);
            Window window = getWindow();
            // Never the focus: the keys and the controller stay with the game.
            window.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                    | WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                    | WindowManager.LayoutParams.FLAG_FULLSCREEN);
            SurfaceView view = new SurfaceView(getContext());
            view.getHolder().addCallback(this);
            view.setOnTouchListener(this);
            setContentView(view);
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            // The size comes with surfaceChanged, which always follows.
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            nativeAttach(holder.getSurface(), width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            nativeDetach();  // returns once the game no longer draws on it
        }

        // Every finger, as the game's Touch: 0 down, 1 up, 2 moved, 3 cancelled (as MotionEvent's).
        @Override
        public boolean onTouch(View view, MotionEvent event) {
            float width = Math.max(1, view.getWidth());
            float height = Math.max(1, view.getHeight());
            int action = event.getActionMasked();
            switch (action) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_POINTER_DOWN:
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_POINTER_UP: {
                    int index = event.getActionIndex();
                    boolean down = action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN;
                    nativeTouch(down ? 0 : 1, event.getPointerId(index), event.getX(index) / width,
                            event.getY(index) / height);
                    break;
                }
                case MotionEvent.ACTION_MOVE:
                case MotionEvent.ACTION_CANCEL:
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        nativeTouch(action == MotionEvent.ACTION_MOVE ? 2 : 3, event.getPointerId(i),
                                event.getX(i) / width, event.getY(i) / height);
                    }
                    break;
                default:
                    break;
            }
            return true;
        }
    }

    // In the game (libmain.so, runtime/src/graphics/second_screen.cpp).
    private static native void nativeAttach(android.view.Surface surface, int width, int height);
    private static native void nativeDetach();
    private static native void nativeTouch(int action, int finger, float x, float y);
}
