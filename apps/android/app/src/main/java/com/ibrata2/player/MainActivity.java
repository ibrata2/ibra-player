package com.ibrata2.player;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import androidx.media3.common.MediaItem;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.ui.PlayerView;

/** Initial handheld playback shell. Automotive integrations are added as a separate service. */
public final class MainActivity extends Activity {
    private static final int PICK_MEDIA = 41;
    private ExoPlayer player;
    private PlayerView playerView;
    private TextView status;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(0xff111318);
        getWindow().setNavigationBarColor(0xff111318);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(16, 18, 16, 12);
        root.setBackgroundColor(0xff111318);

        TextView title = new TextView(this);
        title.setText("iBRA PLAYER");
        title.setTextColor(0xfff5f5f7);
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER_VERTICAL);
        root.addView(title, new LinearLayout.LayoutParams(-1, 56));

        playerView = new PlayerView(this);
        playerView.setUseController(true);
        playerView.setBackgroundColor(0xff08090c);
        root.addView(playerView, new LinearLayout.LayoutParams(-1, 0, 1));

        status = new TextView(this);
        status.setText("اختر ملف فيديو للبدء");
        status.setTextColor(0xffc5c7ce);
        status.setPadding(4, 12, 4, 8);
        root.addView(status);

        Button open = new Button(this);
        open.setText("فتح ملف");
        open.setOnClickListener(v -> chooseMedia());
        root.addView(open, new LinearLayout.LayoutParams(-1, 52));
        setContentView(root);

        player = new ExoPlayer.Builder(this).build();
        playerView.setPlayer(player);
        Uri incoming = getIntent() == null ? null : getIntent().getData();
        if (incoming != null) play(incoming);
    }

    private void chooseMedia() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("video/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(intent, PICK_MEDIA);
    }

    private void play(Uri uri) {
        player.setMediaItem(MediaItem.fromUri(uri));
        player.prepare();
        player.play();
        status.setText(uri.getLastPathSegment() == null ? "يتم التشغيل" : uri.getLastPathSegment());
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == PICK_MEDIA && resultCode == RESULT_OK && data != null && data.getData() != null) {
            final Uri uri = data.getData();
            if ((data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION) != 0) {
                getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            }
            play(uri);
        }
    }

    @Override protected void onStop() {
        super.onStop();
        if (player != null) player.pause();
    }

    @Override protected void onDestroy() {
        if (playerView != null) playerView.setPlayer(null);
        if (player != null) { player.release(); player = null; }
        super.onDestroy();
    }
}
