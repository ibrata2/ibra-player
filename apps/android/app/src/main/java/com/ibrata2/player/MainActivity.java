package com.ibrata2.player;

import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.media3.common.MediaItem;
import androidx.media3.common.Player;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.ui.PlayerView;

import java.util.ArrayList;
import java.util.List;

/** iBRA PLAYER handheld player. Car surfaces require a separate MediaLibraryService integration. */
public final class MainActivity extends Activity {
    private static final int PICK_MEDIA = 41;
    private static final int BG = Color.rgb(12, 14, 18);
    private static final int CARD = Color.rgb(21, 24, 30);
    private static final int RED = Color.rgb(233, 52, 63);
    private static final int WHITE = Color.rgb(246, 246, 248);
    private static final int MUTED = Color.rgb(158, 164, 174);

    private final List<Uri> items = new ArrayList<>();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private ExoPlayer player;
    private PlayerView playerView;
    private FrameLayout videoFrame;
    private View emptyState;
    private TextView status;
    private TextView fileTitle;
    private TextView currentTime;
    private TextView durationTime;
    private TextView playButton;
    private SeekBar timeline;
    private LinearLayout playlist;
    private int currentIndex = -1;
    private boolean seeking;

    private final Runnable progressUpdater = new Runnable() {
        @Override public void run() {
            if (player != null && !seeking) {
                long duration = player.getDuration();
                long position = player.getCurrentPosition();
                timeline.setMax((int) Math.max(1, Math.min(Integer.MAX_VALUE, duration)));
                timeline.setProgress((int) Math.max(0, position));
                currentTime.setText(formatTime(position));
                durationTime.setText(formatTime(duration));
            }
            handler.postDelayed(this, 500);
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        Window window = getWindow();
        window.setStatusBarColor(BG);
        window.setNavigationBarColor(BG);
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        buildInterface();

        player = new ExoPlayer.Builder(this).build();
        playerView.setPlayer(player);
        player.addListener(new Player.Listener() {
            @Override public void onIsPlayingChanged(boolean isPlaying) {
                playButton.setText(isPlaying ? "Ⅱ" : "▶");
                status.setText(isPlaying ? "يعمل الآن" : "متوقف مؤقتًا");
            }
            @Override public void onPlayerError(androidx.media3.common.PlaybackException error) {
                status.setText("تعذّر تشغيل هذا الملف");
            }
        });
        handler.post(progressUpdater);

        Uri incoming = getIntent() == null ? null : getIntent().getData();
        if (incoming != null) addMedia(incoming, true);
    }

    private void buildInterface() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(BG);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        int side = dp(20);
        page.setPadding(side, dp(10), side, dp(22));
        scroll.addView(page, new ScrollView.LayoutParams(-1, -2));

        LinearLayout header = row();
        header.setGravity(Gravity.CENTER_VERTICAL);
        TextView logo = text("▶", 20, WHITE, true);
        logo.setGravity(Gravity.CENTER);
        logo.setBackground(shape(RED, 15, 0));
        header.addView(logo, new LinearLayout.LayoutParams(dp(48), dp(48)));
        LinearLayout brand = column();
        brand.setPadding(dp(12), 0, 0, 0);
        brand.addView(text("iBRA PLAYER", 19, WHITE, true));
        brand.addView(text("مشغّل الوسائط", 12, MUTED, false));
        header.addView(brand, new LinearLayout.LayoutParams(0, -2, 1));
        Button open = button("فتح ملف", RED, WHITE, true);
        open.setOnClickListener(v -> chooseMedia());
        header.addView(open, new LinearLayout.LayoutParams(-2, dp(44)));
        page.addView(header);

        LinearLayout intro = row();
        intro.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout introCopy = column();
        TextView greeting = text("مساحة تليق بمشاهدتك", 22, WHITE, true);
        introCopy.addView(greeting);
        introCopy.addView(text("صوت وصورة بواجهة هادئة وواضحة", 13, MUTED, false));
        intro.addView(introCopy, new LinearLayout.LayoutParams(0, -2, 1));
        TextView live = text("●  جاهز", 12, Color.rgb(135, 221, 174), true);
        live.setPadding(dp(12), dp(8), dp(12), dp(8));
        live.setBackground(shape(Color.rgb(23, 34, 30), 18, Color.rgb(40, 61, 52)));
        intro.addView(live);
        LinearLayout.LayoutParams introParams = new LinearLayout.LayoutParams(-1, -2);
        introParams.topMargin = dp(24);
        introParams.bottomMargin = dp(14);
        page.addView(intro, introParams);

        videoFrame = new FrameLayout(this);
        videoFrame.setBackground(shape(Color.rgb(7, 8, 11), 18, Color.rgb(47, 52, 62)));
        videoFrame.setClipToOutline(true);
        int width = getResources().getDisplayMetrics().widthPixels - dp(40);
        boolean landscape = getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE;
        int height = landscape ? Math.min((int)(width / 1.78f), (int)(getResources().getDisplayMetrics().heightPixels * .43f))
                               : (int)(width / 1.78f);
        height = Math.max(dp(170), height);
        playerView = new PlayerView(this);
        playerView.setUseController(false);
        playerView.setShutterBackgroundColor(Color.rgb(7, 8, 11));
        playerView.setBackgroundColor(Color.rgb(7, 8, 11));
        videoFrame.addView(playerView, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout empty = column();
        empty.setGravity(Gravity.CENTER);
        TextView emblem = text("▶", 21, WHITE, true);
        emblem.setGravity(Gravity.CENTER);
        emblem.setBackground(shape(Color.rgb(54, 31, 39), 22, Color.rgb(93, 45, 57)));
        empty.addView(emblem, new LinearLayout.LayoutParams(dp(66), dp(66)));
        TextView emptyTitle = text("شاهد بطريقتك", 18, WHITE, true);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(-2, -2);
        titleParams.topMargin = dp(12);
        empty.addView(emptyTitle, titleParams);
        empty.addView(text("اختر ملفًا من جهازك لبدء التشغيل", 13, MUTED, false));
        emptyState = empty;
        videoFrame.addView(empty, new FrameLayout.LayoutParams(-1, -1));
        page.addView(videoFrame, new LinearLayout.LayoutParams(-1, height));

        LinearLayout nowCard = cardColumn();
        LinearLayout.LayoutParams nowParams = new LinearLayout.LayoutParams(-1, -2);
        nowParams.topMargin = dp(14);
        page.addView(nowCard, nowParams);
        LinearLayout nowHeader = row();
        nowHeader.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout nowCopy = column();
        nowCopy.addView(text("يعمل الآن", 12, MUTED, true));
        fileTitle = text("لا يوجد ملف قيد التشغيل", 15, WHITE, true);
        fileTitle.setSingleLine(true);
        fileTitle.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);
        nowCopy.addView(fileTitle);
        nowHeader.addView(nowCopy, new LinearLayout.LayoutParams(0, -2, 1));
        status = text("بانتظار اختيار ملف", 12, MUTED, false);
        nowHeader.addView(status);
        nowCard.addView(nowHeader);

        LinearLayout seekRow = row();
        seekRow.setGravity(Gravity.CENTER_VERTICAL);
        currentTime = text("00:00", 11, MUTED, false);
        durationTime = text("00:00", 11, MUTED, false);
        timeline = new SeekBar(this);
        timeline.setMax(1);
        timeline.setProgressTintList(android.content.res.ColorStateList.valueOf(RED));
        timeline.setThumbTintList(android.content.res.ColorStateList.valueOf(WHITE));
        timeline.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) { if (fromUser && player != null) currentTime.setText(formatTime(progress)); }
            @Override public void onStartTrackingTouch(SeekBar bar) { seeking = true; }
            @Override public void onStopTrackingTouch(SeekBar bar) { seeking = false; if (player != null) player.seekTo(bar.getProgress()); }
        });
        seekRow.addView(currentTime);
        seekRow.addView(timeline, new LinearLayout.LayoutParams(0, dp(34), 1));
        seekRow.addView(durationTime);
        LinearLayout.LayoutParams seekParams = new LinearLayout.LayoutParams(-1, -2);
        seekParams.topMargin = dp(10);
        nowCard.addView(seekRow, seekParams);

        LinearLayout controls = row();
        controls.setGravity(Gravity.CENTER);
        controls.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        TextView back = controlButton("−10", false);
        back.setOnClickListener(v -> seekBy(-10_000));
        TextView play = controlButton("▶", true);
        playButton = play;
        play.setOnClickListener(v -> togglePlayback());
        TextView forward = controlButton("+10", false);
        forward.setOnClickListener(v -> seekBy(10_000));
        controls.addView(back);
        controls.addView(play);
        controls.addView(forward);
        LinearLayout.LayoutParams controlsParams = new LinearLayout.LayoutParams(-1, -2);
        controlsParams.topMargin = dp(5);
        nowCard.addView(controls, controlsParams);

        LinearLayout playlistCard = cardColumn();
        LinearLayout.LayoutParams playlistParams = new LinearLayout.LayoutParams(-1, -2);
        playlistParams.topMargin = dp(14);
        page.addView(playlistCard, playlistParams);
        LinearLayout listHeader = row();
        listHeader.setGravity(Gravity.CENTER_VERTICAL);
        TextView listTitle = text("قائمة التشغيل", 15, WHITE, true);
        listHeader.addView(listTitle, new LinearLayout.LayoutParams(0, -2, 1));
        Button add = button("＋  إضافة ملفات", Color.rgb(34, 38, 47), WHITE, false);
        add.setOnClickListener(v -> chooseMedia());
        listHeader.addView(add);
        playlistCard.addView(listHeader);
        playlist = column();
        LinearLayout.LayoutParams playlistContentParams = new LinearLayout.LayoutParams(-1, -2);
        playlistContentParams.topMargin = dp(10);
        playlistCard.addView(playlist, playlistContentParams);
        showEmptyPlaylist();

        TextView footer = text("iBRA PLAYER   •   تشغيل محلي يحترم خصوصيتك", 11, Color.rgb(111, 118, 130), false);
        footer.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams footerParams = new LinearLayout.LayoutParams(-1, dp(42));
        footerParams.topMargin = dp(8);
        page.addView(footer, footerParams);
        setContentView(scroll);
    }

    private void chooseMedia() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"video/*", "audio/*"});
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        startActivityForResult(intent, PICK_MEDIA);
    }

    private void addMedia(Uri uri, boolean playNow) {
        if (uri == null || items.contains(uri)) return;
        items.add(uri);
        try { getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION); }
        catch (SecurityException ignored) { }
        refreshPlaylist();
        if (playNow || currentIndex < 0) playAt(items.size() - 1);
    }

    private void refreshPlaylist() {
        playlist.removeAllViews();
        if (items.isEmpty()) { showEmptyPlaylist(); return; }
        for (int i = 0; i < items.size(); i++) {
            final int index = i;
            LinearLayout item = row();
            item.setGravity(Gravity.CENTER_VERTICAL);
            item.setPadding(dp(12), dp(9), dp(12), dp(9));
            item.setBackground(shape(i == currentIndex ? Color.rgb(54, 34, 41) : Color.rgb(27, 30, 37), 12,
                    i == currentIndex ? Color.rgb(93, 48, 60) : Color.rgb(39, 43, 52)));
            TextView icon = text("♫", 16, i == currentIndex ? Color.rgb(255, 133, 141) : MUTED, true);
            item.addView(icon, new LinearLayout.LayoutParams(dp(28), -2));
            TextView name = text(items.get(i).getLastPathSegment() == null ? "ملف وسائط" : items.get(i).getLastPathSegment(), 13, WHITE, i == currentIndex);
            name.setSingleLine(true);
            name.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);
            item.addView(name, new LinearLayout.LayoutParams(0, -2, 1));
            TextView number = text(String.format(java.util.Locale.getDefault(), "%02d", i + 1), 11, MUTED, false);
            item.addView(number);
            item.setOnClickListener(v -> playAt(index));
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
            params.bottomMargin = dp(7);
            playlist.addView(item, params);
        }
    }

    private void showEmptyPlaylist() {
        TextView empty = text("قائمتك فارغة — أضف ملفاتك المفضلة", 13, MUTED, false);
        empty.setGravity(Gravity.CENTER);
        empty.setPadding(0, dp(12), 0, dp(12));
        playlist.addView(empty);
    }

    private void playAt(int index) {
        if (player == null || index < 0 || index >= items.size()) return;
        currentIndex = index;
        player.setMediaItem(MediaItem.fromUri(items.get(index)));
        player.prepare();
        player.play();
        fileTitle.setText(items.get(index).getLastPathSegment() == null ? "ملف وسائط" : items.get(index).getLastPathSegment());
        status.setText("جارٍ التحضير");
        emptyState.setVisibility(View.GONE);
        refreshPlaylist();
    }

    private void togglePlayback() {
        if (player == null) return;
        if (items.isEmpty()) { chooseMedia(); return; }
        if (player.isPlaying()) player.pause();
        else player.play();
    }

    private void seekBy(long delta) {
        if (player == null || items.isEmpty()) return;
        player.seekTo(Math.max(0, Math.min(player.getDuration(), player.getCurrentPosition() + delta)));
    }

    @Override public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        int width = getResources().getDisplayMetrics().widthPixels - dp(40);
        boolean landscape = configuration.orientation == Configuration.ORIENTATION_LANDSCAPE;
        int height = landscape ? Math.min((int)(width / 1.78f), (int)(getResources().getDisplayMetrics().heightPixels * .43f))
                               : (int)(width / 1.78f);
        videoFrame.getLayoutParams().height = Math.max(dp(170), height);
        videoFrame.requestLayout();
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_MEDIA || resultCode != RESULT_OK || data == null) return;
        if (data.getClipData() != null) {
            for (int i = 0; i < data.getClipData().getItemCount(); i++) addMedia(data.getClipData().getItemAt(i).getUri(), items.isEmpty());
        } else if (data.getData() != null) addMedia(data.getData(), true);
    }

    @Override protected void onStop() {
        super.onStop();
        if (player != null) player.pause();
    }

    @Override protected void onDestroy() {
        handler.removeCallbacks(progressUpdater);
        if (playerView != null) playerView.setPlayer(null);
        if (player != null) { player.release(); player = null; }
        super.onDestroy();
    }

    private LinearLayout row() { LinearLayout v = new LinearLayout(this); v.setOrientation(LinearLayout.HORIZONTAL); return v; }
    private LinearLayout column() { LinearLayout v = new LinearLayout(this); v.setOrientation(LinearLayout.VERTICAL); return v; }
    private LinearLayout cardColumn() {
        LinearLayout v = column();
        v.setPadding(dp(15), dp(13), dp(15), dp(13));
        v.setBackground(shape(CARD, 17, Color.rgb(39, 43, 52)));
        return v;
    }
    private TextView text(String value, int size, int color, boolean bold) {
        TextView v = new TextView(this);
        v.setText(value);
        v.setTextSize(size);
        v.setTextColor(color);
        v.setGravity(Gravity.CENTER_VERTICAL);
        if (bold) v.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        return v;
    }
    private TextView controlButton(String label, boolean primary) {
        TextView v = text(label, primary ? 20 : 13, WHITE, true);
        v.setGravity(Gravity.CENTER);
        v.setBackground(shape(primary ? RED : Color.rgb(35, 39, 48), primary ? 30 : 16,
                primary ? RED : Color.rgb(51, 56, 67)));
        v.setLayoutParams(new LinearLayout.LayoutParams(primary ? dp(58) : dp(54), primary ? dp(58) : dp(48)));
        if (!primary) v.setPadding(dp(8), 0, dp(8), 0);
        if (primary) {
            LinearLayout.LayoutParams p = (LinearLayout.LayoutParams)v.getLayoutParams();
            p.setMargins(dp(12), 0, dp(12), 0);
        }
        return v;
    }
    private Button button(String label, int background, int foreground, boolean primary) {
        Button v = new Button(this);
        v.setText(label);
        v.setTextColor(foreground);
        v.setTextSize(13);
        v.setAllCaps(false);
        v.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        v.setMinHeight(0);
        v.setMinimumHeight(dp(42));
        v.setPadding(dp(14), 0, dp(14), 0);
        v.setBackground(shape(background, 13, primary ? background : Color.rgb(52, 57, 68)));
        return v;
    }
    private GradientDrawable shape(int color, int radius, int stroke) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(radius));
        if (stroke != 0) d.setStroke(dp(1), stroke);
        return d;
    }
    private int dp(float value) { return (int)(value * getResources().getDisplayMetrics().density + 0.5f); }
    private String formatTime(long millis) {
        if (millis < 0) millis = 0;
        long seconds = millis / 1000;
        long hours = seconds / 3600;
        long minutes = (seconds % 3600) / 60;
        long remainder = seconds % 60;
        return hours > 0 ? String.format(java.util.Locale.getDefault(), "%d:%02d:%02d", hours, minutes, remainder)
                : String.format(java.util.Locale.getDefault(), "%02d:%02d", minutes, remainder);
    }
}

