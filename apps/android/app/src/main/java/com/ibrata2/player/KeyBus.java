package com.ibrata2.player;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;

import java.util.Map;

/**
 * One place where steering-wheel / remote buttons arrive, whatever channel the
 * head unit uses: foreground key events, the media session, vendor broadcasts
 * or the optional accessibility key capture.
 */
final class KeyBus {
    interface Listener { void onKey(int code, String source); }

    /** Codes from vendor broadcasts are offset so they can be learned like keys. */
    static final int MTC_BASE = 20000;

    static volatile Listener learn;          // non-null while "learn a button" is open
    static volatile String lastEvent = "";   // shown in Settings → button test
    static volatile boolean uiVisible;       // MainActivity is in the foreground
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    static String name(int code) {
        if (code >= MTC_BASE) return "MTC " + (code - MTC_BASE);
        return KeyEvent.keyCodeToString(code).replace("KEYCODE_", "");
    }

    private static int lastDownCode = -1;
    private static long lastDownTime;
    private static long lastFireTime;
    private static int lastFireCode = -1;

    /**
     * Decides whether a key event should trigger an action. Acts on DOWN; acts
     * on UP only when no DOWN for that key was seen (units that send UP only).
     * Also drops the same key arriving twice through two channels.
     */
    static synchronized boolean accept(KeyEvent ev) {
        long now = android.os.SystemClock.uptimeMillis();
        int code = ev.getKeyCode();
        if (ev.getAction() == KeyEvent.ACTION_DOWN) {
            if (ev.getRepeatCount() > 0) return false;
            lastDownCode = code;
            lastDownTime = now;
        } else if (ev.getAction() == KeyEvent.ACTION_UP) {
            if (code == lastDownCode && now - lastDownTime < 2000) return false;   // pair already handled
        } else {
            return false;
        }
        if (code == lastFireCode && now - lastFireTime < 250) return false;       // same press via two channels
        lastFireCode = code;
        lastFireTime = now;
        return true;
    }

    private static String lastCmd = "";
    private static long lastCmdTime;

    /** Same de-duplication for text commands that may arrive through two receivers. */
    static synchronized boolean acceptCommand(String cmd) {
        long now = android.os.SystemClock.uptimeMillis();
        if (cmd.equals(lastCmd) && now - lastCmdTime < 300) return false;
        lastCmd = cmd;
        lastCmdTime = now;
        return true;
    }

    /** Records the event; returns true when it was taken by the learning dialog. */
    static boolean report(int code, String source) {
        lastEvent = name(code) + "  ·  " + source;
        final Listener l = learn;
        if (l == null) return false;
        MAIN.post(() -> l.onKey(code, source));
        return true;
    }

    /** Maps a raw code to an action and runs it on the player. Returns true if handled. */
    static boolean dispatch(Context ctx, int code, String source) {
        if (report(code, source)) return true;
        Map<Integer, String> map = new Prefs(ctx).keyMap();
        String action = map.get(code);
        if (action == null || "ignore".equals(action)) return false;
        return PlaybackService.runAction(ctx, action);
    }
}
