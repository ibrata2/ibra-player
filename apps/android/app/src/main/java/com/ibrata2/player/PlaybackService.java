package com.ibrata2.player;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.KeyEvent;

import androidx.annotation.NonNull;
import androidx.annotation.OptIn;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.media3.common.AudioAttributes;
import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.MimeTypes;
import androidx.media3.common.Player;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.DefaultRenderersFactory;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.session.MediaSession;
import androidx.media3.session.MediaSessionService;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Owns the player. Because it publishes a MediaSession, the head unit's
 * steering-wheel media buttons reach iBRA PLAYER even when another app
 * (maps, radio screen…) is in the foreground.
 */
@OptIn(markerClass = UnstableApi.class)
public final class PlaybackService extends MediaSessionService {

    static final String EXTRA_PATH = "ibra.path";
    static final String EXTRA_VIDEO = "ibra.video";

    private ExoPlayer player;
    private MediaSession session;
    private Prefs prefs;
    private final Handler handler = new Handler(Looper.getMainLooper());

    private final Runnable saver = new Runnable() {
        @Override public void run() {
            saveState();
            handler.postDelayed(this, 5000);
        }
    };

    /** AOSP music-service broadcast used by many Chinese head units for steering-wheel keys. */
    private final BroadcastReceiver legacyCommands = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) {
            String cmd = i.getStringExtra("command");
            if (cmd == null) cmd = i.getAction();
            if (cmd == null || player == null) return;
            if (cmd.endsWith("next")) perform(player, "next");
            else if (cmd.endsWith("previous")) perform(player, "previous");
            else if (cmd.endsWith("togglepause")) perform(player, "playPause");
            else if (cmd.endsWith("pause")) player.pause();
            else if (cmd.endsWith("play")) player.play();
            else if (cmd.endsWith("stop")) player.pause();
        }
    };

    @Override public void onCreate() {
        super.onCreate();
        prefs = new Prefs(this);
        DefaultRenderersFactory renderers = new DefaultRenderersFactory(this)
                // Prefer the unit's own decoders, fall back to FFmpeg for AC-3/DTS/TrueHD etc.
                .setExtensionRendererMode(DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON)
                .setEnableDecoderFallback(true);
        player = new ExoPlayer.Builder(this, renderers)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(C.USAGE_MEDIA).setContentType(C.AUDIO_CONTENT_TYPE_MUSIC).build(), true)
                .setHandleAudioBecomingNoisy(true)
                .setSeekBackIncrementMs(10_000)
                .setSeekForwardIncrementMs(10_000)
                .setWakeMode(C.WAKE_MODE_LOCAL)
                .build();
        player.addListener(new Player.Listener() {
            @Override public void onMediaItemTransition(@Nullable MediaItem item, int reason) {
                if (item == null) return;
                long pos = prefs.bool("rememberPos", true) ? prefs.position(item.mediaId) : 0;
                if (pos > 0 && reason != Player.MEDIA_ITEM_TRANSITION_REASON_REPEAT) player.seekTo(pos);
                CharSequence t = item.mediaMetadata.title;
                boolean video = item.mediaMetadata.extras != null && item.mediaMetadata.extras.getBoolean(EXTRA_VIDEO);
                prefs.addRecent(item.mediaId, t == null ? "" : t.toString(), video);
            }
            @Override public void onIsPlayingChanged(boolean playing) { saveState(); }
        });

        Intent open = new Intent(this, MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent pi = PendingIntent.getActivity(this, 0, open,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        session = new MediaSession.Builder(this, player)
                .setSessionActivity(pi)
                .setCallback(new Callback())
                .build();

        IntentFilter f = new IntentFilter();
        f.addAction("com.android.music.musicservicecommand");
        f.addAction("com.android.music.musicservicecommand.next");
        f.addAction("com.android.music.musicservicecommand.previous");
        f.addAction("com.android.music.musicservicecommand.togglepause");
        f.addAction("com.android.music.musicservicecommand.pause");
        ContextCompat.registerReceiver(this, legacyCommands, f, ContextCompat.RECEIVER_EXPORTED);
        handler.postDelayed(saver, 5000);
    }

    @Nullable @Override public MediaSession onGetSession(@NonNull MediaSession.ControllerInfo info) { return session; }

    @Override public void onTaskRemoved(@Nullable Intent rootIntent) {
        saveState();
        if (player == null || !player.getPlayWhenReady() || player.getMediaItemCount() == 0) stopSelf();
    }

    @Override public void onDestroy() {
        saveState();
        handler.removeCallbacks(saver);
        try { unregisterReceiver(legacyCommands); } catch (Exception ignored) { }
        if (session != null) { session.release(); session = null; }
        if (player != null) { player.release(); player = null; }
        super.onDestroy();
    }

    private void saveState() {
        if (player == null || player.getMediaItemCount() == 0) return;
        MediaItem cur = player.getCurrentMediaItem();
        if (cur != null && prefs.bool("rememberPos", true)) prefs.setPosition(cur.mediaId, player.getCurrentPosition(), player.getDuration());
        List<String> uris = new ArrayList<>(), titles = new ArrayList<>();
        int n = Math.min(player.getMediaItemCount(), 500);
        for (int i = 0; i < n; i++) {
            MediaItem m = player.getMediaItemAt(i);
            uris.add(m.mediaId);
            Bundle ex = m.mediaMetadata.extras;
            String title = m.mediaMetadata.title == null ? "" : m.mediaMetadata.title.toString();
            String path = ex == null ? "" : ex.getString(EXTRA_PATH, "");
            boolean video = ex != null && ex.getBoolean(EXTRA_VIDEO);
            titles.add(title + "\u0001" + path + "\u0001" + (video ? "1" : "0"));
        }
        prefs.saveSession(uris, titles, player.getCurrentMediaItemIndex(), player.getCurrentPosition());
    }

    /** Executes a steering-wheel / remote action. */
    static void perform(Player p, String action) {
        switch (action) {
            case "playPause":
                if (p.isPlaying()) p.pause();
                else { if (p.getPlaybackState() == Player.STATE_ENDED) p.seekTo(0); p.prepare(); p.play(); }
                break;
            case "next": if (p.hasNextMediaItem()) p.seekToNextMediaItem(); else p.seekTo(0, 0); break;
            case "previous":
                if (p.getCurrentPosition() > 5000 || !p.hasPreviousMediaItem()) p.seekTo(0); else p.seekToPreviousMediaItem();
                break;
            case "forward": p.seekForward(); break;
            case "rewind": p.seekBack(); break;
            case "stop": p.pause(); p.seekTo(0); break;
            case "repeat":
                int m = p.getRepeatMode();
                p.setRepeatMode(m == Player.REPEAT_MODE_OFF ? Player.REPEAT_MODE_ALL : m == Player.REPEAT_MODE_ALL ? Player.REPEAT_MODE_ONE : Player.REPEAT_MODE_OFF);
                break;
            case "shuffle": p.setShuffleModeEnabled(!p.getShuffleModeEnabled()); break;
            default: break;
        }
    }

    /** Builds a playable item, attaching subtitle files that sit next to a video. */
    static MediaItem build(String uri, String title, String sub, String path, boolean video) {
        Bundle extras = new Bundle();
        extras.putString(EXTRA_PATH, path == null ? "" : path);
        extras.putBoolean(EXTRA_VIDEO, video);
        MediaMetadata md = new MediaMetadata.Builder()
                .setTitle(title).setArtist(sub).setDisplayTitle(title)
                .setMediaType(video ? MediaMetadata.MEDIA_TYPE_VIDEO : MediaMetadata.MEDIA_TYPE_MUSIC)
                .setIsPlayable(true).setIsBrowsable(false).setExtras(extras).build();
        MediaItem.Builder b = new MediaItem.Builder().setMediaId(uri).setUri(Uri.parse(uri)).setMediaMetadata(md);
        if (video && path != null && !path.isEmpty()) b.setSubtitleConfigurations(findSubtitles(path));
        return b.build();
    }

    static List<MediaItem.SubtitleConfiguration> findSubtitles(String videoPath) {
        List<MediaItem.SubtitleConfiguration> out = new ArrayList<>();
        File v = new File(videoPath);
        File dir = v.getParentFile();
        if (dir == null) return out;
        String base = MediaRepo.stripExt(v.getName());
        File[] files = dir.listFiles();
        if (files == null) return out;
        for (File f : files) {
            String n = f.getName();
            String lower = n.toLowerCase(java.util.Locale.ROOT);
            if (!n.startsWith(base) || !f.canRead()) continue;
            String mime = lower.endsWith(".srt") ? MimeTypes.APPLICATION_SUBRIP
                    : (lower.endsWith(".ass") || lower.endsWith(".ssa")) ? MimeTypes.TEXT_SSA
                    : lower.endsWith(".vtt") ? MimeTypes.TEXT_VTT : null;
            if (mime == null) continue;
            String lang = lower.contains(".ar.") || lower.contains("arabic") || lower.contains(".ara.") ? "ar" : null;
            out.add(new MediaItem.SubtitleConfiguration.Builder(Uri.fromFile(f))
                    .setMimeType(mime).setLabel(n).setLanguage(lang)
                    .setSelectionFlags(out.isEmpty() ? C.SELECTION_FLAG_DEFAULT : 0).build());
        }
        return out;
    }

    private final class Callback implements MediaSession.Callback {
        @Override public ListenableFuture<List<MediaItem>> onAddMediaItems(@NonNull MediaSession s,
                @NonNull MediaSession.ControllerInfo c, @NonNull List<MediaItem> items) {
            List<MediaItem> out = new ArrayList<>();
            for (MediaItem m : items) {
                if (m.localConfiguration != null) { out.add(m); continue; }
                Bundle ex = m.mediaMetadata.extras;
                out.add(build(m.mediaId, m.mediaMetadata.title == null ? "" : m.mediaMetadata.title.toString(),
                        m.mediaMetadata.artist == null ? "" : m.mediaMetadata.artist.toString(),
                        ex == null ? null : ex.getString(EXTRA_PATH), ex != null && ex.getBoolean(EXTRA_VIDEO)));
            }
            return Futures.immediateFuture(out);
        }

        @Override public boolean onMediaButtonEvent(@NonNull MediaSession s, @NonNull MediaSession.ControllerInfo c, @NonNull Intent intent) {
            KeyEvent ev = Build.VERSION.SDK_INT >= 33
                    ? intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT, KeyEvent.class)
                    : intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT);
            if (ev == null) return false;
            Map<Integer, String> map = prefs.keyMap();
            String action = map.get(ev.getKeyCode());
            if (action == null) return false;
            if (ev.getAction() == KeyEvent.ACTION_DOWN && ev.getRepeatCount() == 0) {
                if ("openPlayer".equals(action)) {
                    Intent i = new Intent(PlaybackService.this, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    try { startActivity(i); } catch (Exception ignored) { }
                } else if (!"ignore".equals(action)) {
                    perform(player, action);
                }
            }
            return true;
        }
    }
}
