package com.ibrata2.player;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.util.LruCache;
import android.util.Size;

import java.io.File;
import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Reads videos and music from MediaStore on every storage volume (internal, SD card, USB). */
final class MediaRepo {

    static final class Item {
        String uri, title, sub, path, folder;
        long id, duration;
        boolean video;
    }

    static final class Folder {
        String name, path;
        final List<Item> items = new ArrayList<>();
    }

    final List<Item> videos = new ArrayList<>();
    final List<Item> music = new ArrayList<>();
    final List<Folder> folders = new ArrayList<>();

    private static final LruCache<String, Bitmap> THUMBS = new LruCache<String, Bitmap>(24 * 1024 * 1024) {
        @Override protected int sizeOf(String k, Bitmap b) { return b.getByteCount(); }
    };

    void scan(Context ctx) {
        videos.clear(); music.clear(); folders.clear();
        ContentResolver cr = ctx.getContentResolver();
        Set<String> volumes = new LinkedHashSet<>();
        if (Build.VERSION.SDK_INT >= 29) volumes.addAll(MediaStore.getExternalVolumeNames(ctx));
        else volumes.add("external");
        Set<String> seen = new LinkedHashSet<>();
        for (String vol : volumes) {
            Uri vUri = Build.VERSION.SDK_INT >= 29 ? MediaStore.Video.Media.getContentUri(vol) : MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
            Uri aUri = Build.VERSION.SDK_INT >= 29 ? MediaStore.Audio.Media.getContentUri(vol) : MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
            query(cr, vUri, true, seen);
            query(cr, aUri, false, seen);
        }
        Collator col = Collator.getInstance(new Locale("ar"));
        Collections.sort(videos, (a, b) -> col.compare(a.title, b.title));
        Collections.sort(music, (a, b) -> col.compare(a.title, b.title));

        Map<String, Folder> map = new LinkedHashMap<>();
        List<Item> all = new ArrayList<>(videos);
        all.addAll(music);
        for (Item it : all) {
            String key = it.folder == null ? "?" : it.folder;
            Folder f = map.get(key);
            if (f == null) {
                f = new Folder();
                f.path = key;
                f.name = new File(key).getName();
                if (f.name.isEmpty()) f.name = key;
                map.put(key, f);
            }
            f.items.add(it);
        }
        folders.addAll(map.values());
        Collections.sort(folders, (a, b) -> col.compare(a.name, b.name));
        for (Folder f : folders) Collections.sort(f.items, (a, b) -> col.compare(a.title, b.title));
    }

    @SuppressWarnings("deprecation")
    private void query(ContentResolver cr, Uri base, boolean video, Set<String> seen) {
        List<String> cols = new ArrayList<>();
        cols.add(MediaStore.MediaColumns._ID);
        cols.add(MediaStore.MediaColumns.DISPLAY_NAME);
        cols.add(MediaStore.MediaColumns.TITLE);
        cols.add(MediaStore.MediaColumns.DATA);
        cols.add("duration");
        if (!video) { cols.add(MediaStore.Audio.AudioColumns.ARTIST); cols.add(MediaStore.Audio.AudioColumns.ALBUM); }
        if (Build.VERSION.SDK_INT >= 29) cols.add(MediaStore.MediaColumns.RELATIVE_PATH);
        String sel = video ? null : MediaStore.Audio.AudioColumns.IS_MUSIC + " != 0 OR " + MediaStore.MediaColumns.DATA + " LIKE '%.dts' OR "
                + MediaStore.MediaColumns.DATA + " LIKE '%.ac3'";
        try (Cursor c = cr.query(base, cols.toArray(new String[0]), sel, null, null)) {
            if (c == null) return;
            int iId = c.getColumnIndex(MediaStore.MediaColumns._ID);
            int iName = c.getColumnIndex(MediaStore.MediaColumns.DISPLAY_NAME);
            int iTitle = c.getColumnIndex(MediaStore.MediaColumns.TITLE);
            int iData = c.getColumnIndex(MediaStore.MediaColumns.DATA);
            int iDur = c.getColumnIndex("duration");
            int iArtist = c.getColumnIndex(MediaStore.Audio.AudioColumns.ARTIST);
            int iRel = Build.VERSION.SDK_INT >= 29 ? c.getColumnIndex(MediaStore.MediaColumns.RELATIVE_PATH) : -1;
            while (c.moveToNext()) {
                Item it = new Item();
                it.id = c.getLong(iId);
                it.video = video;
                it.uri = ContentUris.withAppendedId(base, it.id).toString();
                it.path = iData >= 0 ? c.getString(iData) : null;
                String key = it.path != null ? it.path : it.uri;
                if (!seen.add(key)) continue;
                String name = iName >= 0 ? c.getString(iName) : null;
                String title = iTitle >= 0 ? c.getString(iTitle) : null;
                it.title = video ? (name != null ? stripExt(name) : title) : (title != null && !title.isEmpty() ? title : stripExt(name));
                if (it.title == null) it.title = "?";
                it.duration = iDur >= 0 ? c.getLong(iDur) : 0;
                if (it.path != null) it.folder = new File(it.path).getParent();
                else if (iRel >= 0) it.folder = c.getString(iRel);
                String artist = iArtist >= 0 ? c.getString(iArtist) : null;
                if (artist != null && artist.startsWith("<")) artist = null;   // "<unknown>"
                it.sub = video ? (it.folder != null ? new File(it.folder).getName() : "") : (artist != null ? artist : "");
                (video ? videos : music).add(it);
            }
        } catch (Exception ignored) {
            // A volume that disappears while scanning (USB unplugged) must not crash the app
        }
    }

    static String stripExt(String n) {
        if (n == null) return null;
        int d = n.lastIndexOf('.');
        return d > 0 ? n.substring(0, d) : n;
    }

    @SuppressWarnings("deprecation")
    static Bitmap thumbnail(Context ctx, Item it, int w, int h) {
        String key = it.uri + "@" + w;
        Bitmap b = THUMBS.get(key);
        if (b != null) return b;
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                b = ctx.getContentResolver().loadThumbnail(Uri.parse(it.uri), new Size(w, h), null);
            } else if (it.video) {
                b = MediaStore.Video.Thumbnails.getThumbnail(ctx.getContentResolver(), it.id, MediaStore.Video.Thumbnails.MINI_KIND, null);
            } else if (it.path != null) {
                android.media.MediaMetadataRetriever r = new android.media.MediaMetadataRetriever();
                try {
                    r.setDataSource(it.path);
                    byte[] art = r.getEmbeddedPicture();
                    if (art != null) b = android.graphics.BitmapFactory.decodeByteArray(art, 0, art.length);
                } finally { r.release(); }
            }
        } catch (Exception ignored) { }
        if (b != null) THUMBS.put(key, b);
        return b;
    }

    static List<Item> filter(List<Item> src, String q) {
        if (q == null || q.trim().isEmpty()) return src;
        String s = q.trim().toLowerCase(Locale.ROOT);
        List<Item> out = new ArrayList<>();
        for (Item it : src)
            if (it.title.toLowerCase(Locale.ROOT).contains(s) || (it.sub != null && it.sub.toLowerCase(Locale.ROOT).contains(s))) out.add(it);
        return out;
    }
}
