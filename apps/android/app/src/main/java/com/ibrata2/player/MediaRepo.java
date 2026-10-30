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
        String name, path, volume;
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
        // MediaStore on head units often indexes only part of a USB disk, so walk
        // every reachable storage ourselves and add whatever it missed.
        for (File root : storageRoots(ctx)) walk(root, seen);

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
        for (Folder f : folders) {
            String vol = volumeName(f.path);
            f.volume = vol;
        }
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

    // ---------------------------------------------------------------- filesystem walk
    static final java.util.Set<String> VIDEO_EXT = new java.util.HashSet<>(java.util.Arrays.asList(
            "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "mpeg", "mpg", "m4v", "ts", "mts", "m2ts", "vob",
            "3gp", "3g2", "ogv", "divx", "xvid", "asf", "f4v", "rm", "rmvb", "m2v", "dat", "trp", "tp", "mpe"));
    static final java.util.Set<String> AUDIO_EXT = new java.util.HashSet<>(java.util.Arrays.asList(
            "mp3", "aac", "m4a", "flac", "wav", "wma", "ogg", "oga", "opus", "ac3", "eac3", "dts", "mka", "alac",
            "ape", "aiff", "aif", "amr", "wv", "m4b", "mp2", "mid", "midi"));
    private static final int MAX_FILES = 80_000;
    static volatile int progress;      // files examined, shown while scanning
    final List<String> rootsScanned = new ArrayList<>();
    boolean unreadableRoot;

    /** Every mounted storage: internal, SD cards, USB sticks and USB hard disks. */
    static List<File> storageRoots(Context ctx) {
        java.util.LinkedHashMap<String, File> out = new java.util.LinkedHashMap<>();
        try {
            android.os.storage.StorageManager sm = (android.os.storage.StorageManager) ctx.getSystemService(Context.STORAGE_SERVICE);
            for (android.os.storage.StorageVolume v : sm.getStorageVolumes()) {
                File d = null;
                if (Build.VERSION.SDK_INT >= 30) d = v.getDirectory();
                else {
                    try { d = (File) v.getClass().getMethod("getPathFile").invoke(v); } catch (Exception ignored) { }
                    if (d == null) try { d = new File((String) v.getClass().getMethod("getPath").invoke(v)); } catch (Exception ignored) { }
                }
                addRoot(out, d);
            }
        } catch (Exception ignored) { }
        addRoot(out, android.os.Environment.getExternalStorageDirectory());
        // Chinese head units often mount USB drives in non-standard places
        for (String base : new String[]{"/storage", "/mnt", "/mnt/media_rw", "/mnt/usb_storage", "/mnt/usb", "/mnt/udisk"}) {
            File[] kids = new File(base).listFiles();
            if (kids == null) continue;
            for (File k : kids) {
                String n = k.getName();
                if (n.equals("self") || n.equals("emulated") || n.equals("runtime") || n.equals("secure") || n.equals("asec")
                        || n.equals("obb") || n.equals("user") || n.equals("pass_through") || n.equals("appfuse") || n.equals("vendor")
                        || n.equals("installer") || n.equals("expand") || n.equals("product") || n.equals("data_mirror")) continue;
                addRoot(out, k);
            }
        }
        return new ArrayList<>(out.values());
    }

    private static void addRoot(java.util.Map<String, File> out, File d) {
        if (d == null || !d.isDirectory()) return;
        String key;
        try { key = d.getCanonicalPath(); } catch (Exception e) { key = d.getAbsolutePath(); }
        for (String k : out.keySet()) if (key.startsWith(k + "/") || key.equals(k)) return;   // already covered
        out.put(key, d);
    }

    private static String volumeName(String path) {
        if (path == null) return "";
        String[] p = path.split("/");
        // /storage/XXXX-XXXX/...  or /mnt/usb_storage/USB_DISK0/...
        if (p.length > 2 && p[1].equals("storage")) return p[2].equals("emulated") ? "Internal" : p[2];
        if (p.length > 2 && p[1].equals("mnt")) return p.length > 3 ? p[3] : p[2];
        return "";
    }

    private void walk(File root, Set<String> seen) {
        File[] top = root.listFiles();
        if (top == null) { unreadableRoot = true; return; }
        rootsScanned.add(root.getAbsolutePath());
        java.util.ArrayDeque<File> stack = new java.util.ArrayDeque<>();
        stack.push(root);
        int count = 0;
        while (!stack.isEmpty() && count < MAX_FILES) {
            File dir = stack.pop();
            File[] files = dir.listFiles();
            if (files == null) continue;
            for (File f : files) {
                String n = f.getName();
                if (n.startsWith(".")) continue;
                if (f.isDirectory()) {
                    // skip app-private data and system folders
                    if (dir.equals(root) && (n.equals("Android") || n.equals("LOST.DIR") || n.equals("System Volume Information"))) continue;
                    stack.push(f);
                    continue;
                }
                count++;
                progress++;
                int dot = n.lastIndexOf('.');
                if (dot < 0) continue;
                String ext = n.substring(dot + 1).toLowerCase(Locale.ROOT);
                boolean video = VIDEO_EXT.contains(ext);
                if (!video && !AUDIO_EXT.contains(ext)) continue;
                String path = f.getAbsolutePath();
                if (seen.contains(path)) continue;
                String canon = path;
                try { canon = f.getCanonicalPath(); } catch (Exception ignored) { }
                if (seen.contains(canon)) continue;
                seen.add(path);
                seen.add(canon);
                Item it = new Item();
                it.video = video;
                it.path = path;
                it.uri = Uri.fromFile(f).toString();
                it.title = stripExt(n);
                it.folder = f.getParent();
                it.sub = video ? new File(it.folder).getName() : "";
                (video ? videos : music).add(it);
            }
        }
    }

    static boolean hasFullAccess(Context ctx) {
        if (Build.VERSION.SDK_INT >= 30) return android.os.Environment.isExternalStorageManager();
        return ctx.checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE) == android.content.pm.PackageManager.PERMISSION_GRANTED;
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
            if (it.id == 0 && it.path != null) {
                // found by our own folder scan (not in MediaStore)
                if (it.video) {
                    if (Build.VERSION.SDK_INT >= 29) b = android.media.ThumbnailUtils.createVideoThumbnail(new File(it.path), new Size(w, h), null);
                    else b = android.media.ThumbnailUtils.createVideoThumbnail(it.path, MediaStore.Video.Thumbnails.MINI_KIND);
                }
            } else if (Build.VERSION.SDK_INT >= 29) {
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
