package com.ibrata2.player;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.view.KeyEvent;

import androidx.annotation.OptIn;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.session.MediaButtonReceiver;

/**
 * Receives steering-wheel / media buttons sent directly to the app.
 *
 * Media3's own MediaButtonReceiver ignores every key except PLAY on Android 8+
 * (it only exists to restart playback), so NEXT / PREVIOUS sent by a head unit
 * straight to the app were silently dropped. This receiver handles all keys
 * while the player is running and only delegates to Media3 to start playback
 * when nothing is running yet.
 */
@OptIn(markerClass = UnstableApi.class)
public final class CarKeyReceiver extends BroadcastReceiver {

    @Override public void onReceive(Context ctx, Intent intent) {
        String action = intent.getAction();
        if (action == null) return;

        if (Intent.ACTION_MEDIA_BUTTON.equals(action)) {
            KeyEvent ev = Build.VERSION.SDK_INT >= 33
                    ? intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT, KeyEvent.class)
                    : intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            if (ev == null) return;
            if (!KeyBus.accept(ev)) return;   // de-duplicates down/up pairs
            int code = ev.getKeyCode();
            if (PlaybackService.isRunning()) {
                KeyBus.dispatch(ctx, code, "media-button receiver");
                if (isOrderedBroadcast()) abortBroadcast();
                return;
            }
            KeyBus.report(code, "media-button receiver (player stopped)");
            // Nothing is playing yet: let Media3 start the service for PLAY / PLAY_PAUSE.
            if (code == KeyEvent.KEYCODE_MEDIA_PLAY || code == KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                    || code == KeyEvent.KEYCODE_HEADSETHOOK) {
                Intent play = new Intent(intent);
                if (code == KeyEvent.KEYCODE_HEADSETHOOK)
                    play.putExtra(Intent.EXTRA_KEY_EVENT, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_MEDIA_PLAY));
                try { new MediaButtonReceiver().onReceive(ctx, play); } catch (Exception ignored) { }
            }
            return;
        }

        // AOSP music-service commands sent to this package (several head units)
        String cmd = intent.getStringExtra("command");
        if (cmd == null) cmd = action;
        String mapped = cmd.endsWith("next") ? "next" : cmd.endsWith("previous") ? "previous"
                : cmd.endsWith("togglepause") ? "playPause" : cmd.endsWith("pause") ? "pause"
                : cmd.endsWith("play") ? "play" : cmd.endsWith("stop") ? "pause" : null;
        if (mapped == null || !KeyBus.acceptCommand(mapped)) return;
        KeyBus.lastEvent = cmd + "  ·  music command";
        PlaybackService.runAction(ctx, mapped);
    }
}
