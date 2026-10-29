package com.ibrata2.player;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** When the car turns on (head unit boots / ACC on), optionally reopen the player and continue. */
public final class BootReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context ctx, Intent intent) {
        if (!new Prefs(ctx).bool("autoStart", false)) return;
        Intent open = new Intent(ctx, MainActivity.class)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra(MainActivity.EXTRA_AUTOPLAY, true);
        try {
            ctx.startActivity(open);
        } catch (Exception ignored) {
            // Some Android versions block starting from the background; the user can still open the app.
        }
    }
}
