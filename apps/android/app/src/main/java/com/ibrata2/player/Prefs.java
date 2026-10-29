package com.ibrata2.player;

import android.content.Context;
import android.content.SharedPreferences;
import android.view.KeyEvent;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Persistent settings, library state and steering-wheel key map. */
final class Prefs {
    private final SharedPreferences sp;

    Prefs(Context c) { sp = c.getSharedPreferences("ibra", Context.MODE_PRIVATE); }

    boolean arabic() { return sp.getBoolean("arabic", true); }
    void setArabic(boolean v) { sp.edit().putBoolean("arabic", v).apply(); }
    String theme() { return sp.getString("theme", "red-black"); }
    void setTheme(String v) { sp.edit().putString("theme", v).apply(); }
    boolean bool(String k, boolean def) { return sp.getBoolean(k, def); }
    void setBool(String k, boolean v) { sp.edit().putBoolean(k, v).apply(); }
    int integer(String k, int def) { return sp.getInt(k, def); }
    void setInt(String k, int v) { sp.edit().putInt(k, v).apply(); }

    // ---- per-file resume positions
    long position(String uri) { return sp.getLong("pos:" + uri, 0); }
    void setPosition(String uri, long ms, long duration) {
        SharedPreferences.Editor e = sp.edit();
        if (duration > 60_000 && ms > 10_000 && ms < duration - 15_000) e.putLong("pos:" + uri, ms);
        else e.remove("pos:" + uri);
        e.apply();
    }

    // ---- last session (queue) for "continue when the app opens"
    void saveSession(List<String> uris, List<String> titles, int index, long pos) {
        try {
            JSONObject o = new JSONObject();
            JSONArray a = new JSONArray(), t = new JSONArray();
            for (String u : uris) a.put(u);
            for (String s : titles) t.put(s);
            o.put("uris", a); o.put("titles", t); o.put("index", index); o.put("pos", pos);
            sp.edit().putString("session", o.toString()).apply();
        } catch (JSONException ignored) { }
    }
    JSONObject session() {
        try { return new JSONObject(sp.getString("session", "{}")); } catch (JSONException e) { return new JSONObject(); }
    }

    // ---- recent & favorites (uri|title|isVideo)
    List<String[]> list(String key) {
        List<String[]> out = new ArrayList<>();
        try {
            JSONArray a = new JSONArray(sp.getString(key, "[]"));
            for (int i = 0; i < a.length(); i++) {
                JSONArray r = a.getJSONArray(i);
                out.add(new String[]{r.getString(0), r.getString(1), r.optString(2, "0")});
            }
        } catch (JSONException ignored) { }
        return out;
    }
    private void saveList(String key, List<String[]> l) {
        JSONArray a = new JSONArray();
        for (String[] r : l) { JSONArray x = new JSONArray(); x.put(r[0]); x.put(r[1]); x.put(r[2]); a.put(x); }
        sp.edit().putString(key, a.toString()).apply();
    }
    void addRecent(String uri, String title, boolean video) {
        List<String[]> l = list("recent");
        for (int i = l.size() - 1; i >= 0; i--) if (l.get(i)[0].equals(uri)) l.remove(i);
        l.add(0, new String[]{uri, title, video ? "1" : "0"});
        while (l.size() > 40) l.remove(l.size() - 1);
        saveList("recent", l);
    }
    boolean isFavorite(String uri) {
        for (String[] r : list("fav")) if (r[0].equals(uri)) return true;
        return false;
    }
    boolean toggleFavorite(String uri, String title, boolean video) {
        List<String[]> l = list("fav");
        for (int i = 0; i < l.size(); i++) if (l.get(i)[0].equals(uri)) { l.remove(i); saveList("fav", l); return false; }
        l.add(0, new String[]{uri, title, video ? "1" : "0"});
        saveList("fav", l);
        return true;
    }

    // ---- steering wheel: keycode -> action
    static final String[] ACTIONS = {"playPause", "next", "previous", "forward", "rewind", "stop", "repeat", "shuffle", "openPlayer", "ignore"};

    static Map<Integer, String> defaultKeys() {
        Map<Integer, String> m = new LinkedHashMap<>();
        m.put(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE, "playPause");
        m.put(KeyEvent.KEYCODE_HEADSETHOOK, "playPause");
        m.put(KeyEvent.KEYCODE_MEDIA_PLAY, "playPause");
        m.put(KeyEvent.KEYCODE_MEDIA_PAUSE, "playPause");
        m.put(KeyEvent.KEYCODE_MEDIA_NEXT, "next");
        m.put(KeyEvent.KEYCODE_MEDIA_PREVIOUS, "previous");
        m.put(KeyEvent.KEYCODE_MEDIA_FAST_FORWARD, "forward");
        m.put(KeyEvent.KEYCODE_MEDIA_REWIND, "rewind");
        m.put(KeyEvent.KEYCODE_MEDIA_STOP, "stop");
        m.put(KeyEvent.KEYCODE_CHANNEL_UP, "next");
        m.put(KeyEvent.KEYCODE_CHANNEL_DOWN, "previous");
        m.put(KeyEvent.KEYCODE_MEDIA_SKIP_FORWARD, "next");
        m.put(KeyEvent.KEYCODE_MEDIA_SKIP_BACKWARD, "previous");
        m.put(KeyEvent.KEYCODE_MEDIA_STEP_FORWARD, "forward");
        m.put(KeyEvent.KEYCODE_MEDIA_STEP_BACKWARD, "rewind");
        return m;
    }

    Map<Integer, String> keyMap() {
        Map<Integer, String> m = defaultKeys();
        try {
            JSONObject o = new JSONObject(sp.getString("keys", "{}"));
            java.util.Iterator<String> it = o.keys();
            while (it.hasNext()) { String k = it.next(); m.put(Integer.parseInt(k), o.getString(k)); }
        } catch (Exception ignored) { }
        return m;
    }
    Map<Integer, String> customKeys() {
        Map<Integer, String> m = new LinkedHashMap<>();
        try {
            JSONObject o = new JSONObject(sp.getString("keys", "{}"));
            java.util.Iterator<String> it = o.keys();
            while (it.hasNext()) { String k = it.next(); m.put(Integer.parseInt(k), o.getString(k)); }
        } catch (Exception ignored) { }
        return m;
    }
    void setKey(int keyCode, String action) {
        try {
            JSONObject o = new JSONObject(sp.getString("keys", "{}"));
            o.put(String.valueOf(keyCode), action);
            sp.edit().putString("keys", o.toString()).apply();
        } catch (JSONException ignored) { }
    }
    void resetKeys() { sp.edit().remove("keys").apply(); }
}
