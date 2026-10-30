package com.ibrata2.player;

import android.accessibilityservice.AccessibilityService;
import android.content.ComponentName;
import android.content.Context;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;

/**
 * Optional global button capture. Many head units inject steering-wheel keys as
 * ordinary key events that only reach the app in front (or only their own music
 * app). With this service enabled iBRA PLAYER receives them everywhere.
 * Keys are only consumed while iBRA PLAYER is the active player, so radio and
 * other apps keep working when you switch to them.
 */
public final class KeyCaptureService extends AccessibilityService {

    static boolean isEnabled(Context ctx) {
        String list = Settings.Secure.getString(ctx.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (TextUtils.isEmpty(list)) return false;
        String me = new ComponentName(ctx, KeyCaptureService.class).flattenToString();
        String shortMe = new ComponentName(ctx, KeyCaptureService.class).flattenToShortString();
        for (String s : list.split(":")) if (s.equalsIgnoreCase(me) || s.equalsIgnoreCase(shortMe)) return true;
        return false;
    }

    @Override protected boolean onKeyEvent(KeyEvent e) {
        int code = e.getKeyCode();
        switch (code) {   // never interfere with system navigation
            case KeyEvent.KEYCODE_BACK: case KeyEvent.KEYCODE_HOME: case KeyEvent.KEYCODE_APP_SWITCH:
            case KeyEvent.KEYCODE_POWER: case KeyEvent.KEYCODE_VOLUME_UP: case KeyEvent.KEYCODE_VOLUME_DOWN:
            case KeyEvent.KEYCODE_VOLUME_MUTE:
                return false;
            default: break;
        }
        if (KeyBus.learn != null) {
            if (KeyBus.accept(e)) KeyBus.report(code, "accessibility");
            return true;
        }
        if (KeyBus.uiVisible) return false;   // the activity handles keys itself while it is in front
        String action = new Prefs(this).keyMap().get(code);
        if (action == null || "ignore".equals(action) || !PlaybackService.isActivePlayer()) return false;
        if (KeyBus.accept(e)) {
            KeyBus.lastEvent = KeyBus.name(code) + "  ·  accessibility";
            PlaybackService.runAction(this, action);
        }
        return true;
    }

    @Override public void onAccessibilityEvent(AccessibilityEvent event) { }
    @Override public void onInterrupt() { }
}
