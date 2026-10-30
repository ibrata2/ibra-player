package com.ibrata2.player;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextClock;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.OptIn;
import androidx.core.content.ContextCompat;
import androidx.media3.common.C;
import androidx.media3.common.MediaItem;
import androidx.media3.common.MediaMetadata;
import androidx.media3.common.PlaybackException;
import androidx.media3.common.Player;
import androidx.media3.common.TrackSelectionOverride;
import androidx.media3.common.Tracks;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.session.MediaController;
import androidx.media3.session.SessionToken;
import androidx.media3.ui.AspectRatioFrameLayout;
import androidx.media3.ui.CaptionStyleCompat;
import androidx.media3.ui.PlayerView;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.common.util.concurrent.ListenableFuture;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * iBRA PLAYER for car head units, phones and tablets.
 * Layout adapts to any screen: 7" 1024×600, 10" 1280×720, ultra-wide 1920×720,
 * portrait "Tesla-style" screens and phones. Touch, rotary/D-pad and steering-wheel keys.
 */
@OptIn(markerClass = UnstableApi.class)
public final class MainActivity extends Activity {

    static final String EXTRA_AUTOPLAY = "autoplay";
    private static final int REQ_PERMS = 7;

    private Prefs prefs;
    private Glass g;
    private float dp;
    private final MediaRepo repo = new MediaRepo();
    private final ExecutorService io = Executors.newFixedThreadPool(2);
    private final Handler ui = new Handler(Looper.getMainLooper());
    private ListenableFuture<MediaController> controllerFuture;
    private MediaController controller;
    private boolean scanned, scanning, pendingAutoplay, hadFullAccess;
    private Uri pendingViewUri;

    // shell
    private FrameLayout root, content;
    private LinearLayout nav;
    private String tab = "videos";
    private MediaRepo.Folder openFolder;
    private String query = "";
    // library
    private RecyclerView list;
    private RowAdapter adapter;
    private TextView header, emptyView;
    private EditText search;
    // mini player
    private LinearLayout mini;
    private ImageView miniArt;
    private TextView miniTitle, miniSub;
    private ImageButton miniPlay;
    // full player
    private FrameLayout playerPage;
    private PlayerView playerView;
    private ImageView audioArt;
    private LinearLayout pTop, pBottom;
    private TextView pTitle, pSub, pCur, pDur;
    private SeekBar pSeek;
    private ImageButton pPlay, pRepeat, pShuffle, pFav;
    private boolean playerVisible, seeking;
    private AlertDialog learnDialog;
    private byte[] lastArtData;
    private Bitmap lastArt;
    private String lastArtId;

    private TextView lastKeyView;
    private final Runnable ticker = new Runnable() {
        @Override public void run() {
            updateProgress();
            if (lastKeyView != null && lastKeyView.isAttachedToWindow() && !KeyBus.lastEvent.isEmpty())
                lastKeyView.setText(KeyBus.lastEvent);
            ui.postDelayed(this, 500);
        }
    };
    private final Runnable hideControls = () -> setControlsVisible(false);

    // ------------------------------------------------------------------ lifecycle
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = new Prefs(this);
        Tr.setArabic(prefs.arabic());
        pendingAutoplay = getIntent().getBooleanExtra(EXTRA_AUTOPLAY, false);
        if (Intent.ACTION_VIEW.equals(getIntent().getAction())) pendingViewUri = getIntent().getData();
        buildUi();
        requestPermissionsIfNeeded();
    }

    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null) {
            pendingViewUri = intent.getData();
            if (controller != null) playPendingUri();
        }
    }

    @Override protected void onStart() {
        super.onStart();
        SessionToken token = new SessionToken(this, new ComponentName(this, PlaybackService.class));
        controllerFuture = new MediaController.Builder(this, token).buildAsync();
        controllerFuture.addListener(() -> {
            try {
                controller = controllerFuture.get();
            } catch (Exception e) {
                return;
            }
            controller.addListener(playerListener);
            playerView.setPlayer(controller);
            onControllerReady();
        }, ContextCompat.getMainExecutor(this));
        ui.post(ticker);
    }

    @Override protected void onStop() {
        ui.removeCallbacks(ticker);
        if (playerView != null) playerView.setPlayer(null);
        if (controller != null) controller.removeListener(playerListener);
        MediaController.releaseFuture(controllerFuture);
        controller = null;
        super.onStop();
    }

    @Override protected void onDestroy() {
        io.shutdownNow();
        super.onDestroy();
    }

    @Override public void onConfigurationChanged(@NonNull Configuration c) {
        super.onConfigurationChanged(c);
        boolean showPlayer = playerVisible;
        buildUi();
        if (controller != null) { playerView.setPlayer(controller); refreshPlayerState(); }
        if (showPlayer) showPlayer(true);
    }

    @Override public void onBackPressed() {
        if (playerVisible) { showPlayer(false); return; }
        if (openFolder != null) { openFolder = null; refreshList(); return; }
        if (!"videos".equals(tab)) { selectTab("videos"); return; }
        moveTaskToBack(true);   // keep music playing, like a car player should
    }

    // ------------------------------------------------------------------ permissions & scan
    private void requestPermissionsIfNeeded() {
        List<String> need = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 33) {
            need.add(Manifest.permission.READ_MEDIA_VIDEO);
            need.add(Manifest.permission.READ_MEDIA_AUDIO);
            need.add(Manifest.permission.POST_NOTIFICATIONS);
        } else if (Build.VERSION.SDK_INT >= 23) {
            need.add(Manifest.permission.READ_EXTERNAL_STORAGE);
        }
        List<String> missing = new ArrayList<>();
        for (String p : need) if (checkSelfPermission(p) != PackageManager.PERMISSION_GRANTED) missing.add(p);
        if (missing.isEmpty()) scan();
        else requestPermissions(missing.toArray(new String[0]), REQ_PERMS);
    }

    @Override public void onRequestPermissionsResult(int code, @NonNull String[] perms, @NonNull int[] res) {
        super.onRequestPermissionsResult(code, perms, res);
        scan();
    }

    private void scan() {
        if (scanning) return;
        scanning = true;
        header.setText(Tr.t("scanning"));
        MediaRepo.progress = 0;
        Runnable tick = new Runnable() {
            @Override public void run() {
                if (!scanning) return;
                header.setText(Tr.t("scanning") + "  " + MediaRepo.progress);
                ui.postDelayed(this, 400);
            }
        };
        ui.postDelayed(tick, 400);
        io.execute(() -> {
            MediaRepo fresh = new MediaRepo();
            fresh.scan(this);
            ui.post(() -> {
                hadFullAccess = MediaRepo.hasFullAccess(this);
                repo.videos.clear(); repo.videos.addAll(fresh.videos);
                repo.music.clear(); repo.music.addAll(fresh.music);
                repo.folders.clear(); repo.folders.addAll(fresh.folders);
                scanned = true;
                scanning = false;
                refreshList();
            });
        });
    }

    // ------------------------------------------------------------------ UI construction
    private int px(float v) { return Math.round(v * dp); }

    private void buildUi() {
        g = "system".equals(prefs.theme()) ? Glass.system(this) : Glass.of(prefs.theme());
        DisplayMetrics m = getResources().getDisplayMetrics();
        dp = m.density;
        float wDp = m.widthPixels / dp, hDp = m.heightPixels / dp;
        boolean landscape = wDp >= hDp;
        boolean compact = Math.min(wDp, hDp) < 420;   // 1024×600 @ 1.5x etc.

        getWindow().setStatusBarColor(Color.BLACK);
        getWindow().getDecorView().setLayoutDirection(Tr.isArabic() ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        settingsView = null;
        lastArtId = null;
        root = new FrameLayout(this);
        root.setBackground(g.backdrop());
        LinearLayout shell = new LinearLayout(this);
        shell.setOrientation(landscape ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
        root.addView(shell, new FrameLayout.LayoutParams(-1, -1));

        // ---- navigation (rail on the side in landscape, bar at the bottom in portrait)
        nav = new LinearLayout(this);
        nav.setOrientation(landscape ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        nav.setGravity(Gravity.CENTER);
        nav.setBackground(g.panel(px(18), dp, 20));
        int pad = px(compact ? 4 : 8);
        nav.setPadding(pad, pad, pad, pad);
        if (landscape) {
            ImageView logo = new ImageView(this);
            logo.setImageResource(R.drawable.logo_big);
            int ls = px(compact ? 48 : 64);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ls, ls);
            lp.bottomMargin = px(compact ? 4 : 12);
            nav.addView(logo, lp);
        }
        String[][] tabs = {{"videos", "video"}, {"music", "music"}, {"folders", "folder"}, {"recent", "recent"},
                {"favorites", "star"}, {"settings", "settings"}};
        for (String[] t : tabs) nav.addView(navButton(t[0], t[1], landscape, compact), navParams(landscape));

        content = new FrameLayout(this);
        LinearLayout.LayoutParams navLp = landscape
                ? new LinearLayout.LayoutParams(px(compact ? 88 : 108), -1)
                : new LinearLayout.LayoutParams(-1, px(compact ? 64 : 76));
        int gap = px(compact ? 6 : 10);
        navLp.setMargins(gap, gap, landscape ? 0 : gap, gap);
        LinearLayout.LayoutParams contentLp = landscape ? new LinearLayout.LayoutParams(0, -1, 1f) : new LinearLayout.LayoutParams(-1, 0, 1f);
        if (landscape) { shell.addView(nav, navLp); shell.addView(content, contentLp); }
        else { shell.addView(content, contentLp); shell.addView(nav, navLp); }

        buildLibrary(compact, wDp);
        buildPlayer(compact);
        setContentView(root);
        selectTab(tab);
        if (scanned) refreshList();
        if (controller != null) refreshPlayerState();
    }

    private LinearLayout.LayoutParams navParams(boolean landscape) {
        return landscape ? new LinearLayout.LayoutParams(-1, 0, 1f) : new LinearLayout.LayoutParams(0, -1, 1f);
    }

    private View navButton(String id, String icon, boolean landscape, boolean compact) {
        LinearLayout b = new LinearLayout(this);
        b.setOrientation(LinearLayout.VERTICAL);
        b.setGravity(Gravity.CENTER);
        b.setTag(id);
        b.setFocusable(true);
        b.setClickable(true);
        ImageView iv = new ImageView(this);
        iv.setImageDrawable(new Icon(icon, g.text));
        int s = px(compact ? 26 : 30);
        b.addView(iv, new LinearLayout.LayoutParams(s, s));
        TextView tv = text(Tr.t(id), compact ? 11 : 12, g.text, false);
        tv.setGravity(Gravity.CENTER);
        tv.setMaxLines(1);
        b.addView(tv);
        b.setOnClickListener(v -> selectTab(id));
        return b;
    }

    private void selectTab(String id) {
        tab = id;
        openFolder = null;
        for (int i = 0; i < nav.getChildCount(); i++) {
            View v = nav.getChildAt(i);
            boolean on = id.equals(v.getTag());
            if (v.getTag() == null) continue;
            v.setBackground(on ? g.pill(g.accent, px(14)) : null);
            if (v instanceof LinearLayout) {
                LinearLayout l = (LinearLayout) v;
                ((ImageView) l.getChildAt(0)).setImageDrawable(new Icon(navIcon((String) v.getTag()), on ? g.onAccent : g.text));
                ((TextView) l.getChildAt(1)).setTextColor(on ? g.onAccent : g.text);
            }
        }
        if ("settings".equals(id)) showSettings();
        else { showLibrary(); refreshList(); }
    }

    private String navIcon(String id) {
        switch (id) {
            case "music": return "music";
            case "folders": return "folder";
            case "recent": return "recent";
            case "favorites": return "star";
            case "settings": return "settings";
            default: return "video";
        }
    }

    private LinearLayout library;
    private ScrollView settingsView;

    private void buildLibrary(boolean compact, float wDp) {
        library = new LinearLayout(this);
        library.setOrientation(LinearLayout.VERTICAL);
        int gap = px(compact ? 6 : 10);
        library.setPadding(gap, gap, gap, gap);

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        header = text("", compact ? 20 : 24, g.text, true);
        top.addView(header, new LinearLayout.LayoutParams(0, -2, 1f));
        search = new EditText(this);
        search.setHint(Tr.t("search"));
        search.setSingleLine(true);
        search.setTextColor(g.text);
        search.setHintTextColor(g.muted);
        search.setTextSize(16);
        search.setBackground(g.pill(g.field(), px(22)));
        search.setPadding(px(16), 0, px(16), 0);
        search.setCompoundDrawablePadding(px(8));
        Drawable si = new Icon("search", g.muted);
        si.setBounds(0, 0, px(20), px(20));
        search.setCompoundDrawablesRelative(si, null, null, null);
        search.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) { }
            public void onTextChanged(CharSequence s, int a, int b, int c) { }
            public void afterTextChanged(Editable e) { query = e.toString(); refreshList(); }
        });
        top.addView(search, new LinearLayout.LayoutParams(px(compact ? 200 : 280), px(compact ? 44 : 50)));
        ImageButton rescan = iconButton("refresh", g.text, compact ? 44 : 50);
        rescan.setContentDescription(Tr.t("rescan"));
        rescan.setOnClickListener(v -> scan());
        top.addView(rescan);
        library.addView(top, new LinearLayout.LayoutParams(-1, -2));

        FrameLayout listFrame = new FrameLayout(this);
        list = new RecyclerView(this);
        list.setClipToPadding(false);
        list.setPadding(0, gap, 0, gap);
        int cols = Math.max(1, (int) ((wDp - (compact ? 100 : 130)) / (compact ? 210 : 240)));
        GridLayoutManager glm = new GridLayoutManager(this, cols);
        list.setLayoutManager(glm);
        adapter = new RowAdapter(cols);
        glm.setSpanSizeLookup(new GridLayoutManager.SpanSizeLookup() {
            @Override public int getSpanSize(int pos) { return adapter.isCard(pos) ? 1 : (cols >= 4 ? cols / 2 : cols); }
        });
        list.setAdapter(adapter);
        listFrame.addView(list, new FrameLayout.LayoutParams(-1, -1));
        emptyView = text("", 16, g.muted, false);
        emptyView.setGravity(Gravity.CENTER);
        emptyView.setVisibility(View.GONE);
        listFrame.addView(emptyView, new FrameLayout.LayoutParams(-1, -1));
        library.addView(listFrame, new LinearLayout.LayoutParams(-1, 0, 1f));

        // mini player
        mini = new LinearLayout(this);
        mini.setGravity(Gravity.CENTER_VERTICAL);
        mini.setBackground(g.panel(px(18), dp, 30));
        mini.setPadding(px(8), px(6), px(8), px(6));
        mini.setClickable(true);
        mini.setFocusable(true);
        mini.setOnClickListener(v -> showPlayer(true));
        miniArt = new ImageView(this);
        miniArt.setScaleType(ImageView.ScaleType.CENTER_CROP);
        int ms = px(compact ? 48 : 56);
        mini.addView(miniArt, new LinearLayout.LayoutParams(ms, ms));
        LinearLayout mt = new LinearLayout(this);
        mt.setOrientation(LinearLayout.VERTICAL);
        mt.setPadding(px(12), 0, px(12), 0);
        miniTitle = text("", 16, g.text, true);
        miniTitle.setSingleLine(true);
        miniTitle.setEllipsize(TextUtils.TruncateAt.END);
        miniSub = text("", 13, g.muted, false);
        miniSub.setSingleLine(true);
        mt.addView(miniTitle);
        mt.addView(miniSub);
        mini.addView(mt, new LinearLayout.LayoutParams(0, -2, 1f));
        ImageButton mPrev = iconButton("prev", g.text, compact ? 52 : 60);
        miniPlay = iconButton("play", g.onAccent, compact ? 52 : 60);
        miniPlay.setBackground(g.pill(g.accent, px(30)));
        ImageButton mNext = iconButton("next", g.text, compact ? 52 : 60);
        mPrev.setOnClickListener(v -> act("previous"));
        miniPlay.setOnClickListener(v -> act("playPause"));
        mNext.setOnClickListener(v -> act("next"));
        mini.addView(mPrev); mini.addView(miniPlay); mini.addView(mNext);
        mini.setVisibility(View.GONE);
        LinearLayout.LayoutParams mlp = new LinearLayout.LayoutParams(-1, -2);
        mlp.topMargin = gap;
        library.addView(mini, mlp);
        content.addView(library, new FrameLayout.LayoutParams(-1, -1));
    }

    // ------------------------------------------------------------------ list content
    private void refreshList() {
        if (adapter == null || "settings".equals(tab)) return;
        List<Row> rows = new ArrayList<>();
        String title;
        switch (tab) {
            case "music":
                title = Tr.t("music");
                List<MediaRepo.Item> ms = MediaRepo.filter(repo.music, query);
                for (int i = 0; i < ms.size(); i++) rows.add(Row.item(ms.get(i), ms, i, false));
                break;
            case "folders":
                if (openFolder != null) {
                    title = openFolder.name;
                    rows.add(Row.back());
                    List<MediaRepo.Item> fs = MediaRepo.filter(openFolder.items, query);
                    for (int i = 0; i < fs.size(); i++) rows.add(Row.item(fs.get(i), fs, i, false));
                } else {
                    title = Tr.t("folders") + "  (" + repo.folders.size() + ")";
                    if (!MediaRepo.hasFullAccess(this)) rows.add(Row.grant());
                    for (MediaRepo.Folder f : repo.folders)
                        if (query.isEmpty() || f.name.toLowerCase(Locale.ROOT).contains(query.toLowerCase(Locale.ROOT))) rows.add(Row.folder(f));
                }
                break;
            case "recent":
            case "favorites": {
                title = Tr.t(tab);
                List<String[]> src = prefs.list("recent".equals(tab) ? "recent" : "fav");
                List<MediaRepo.Item> items = new ArrayList<>();
                for (String[] r : src) {
                    MediaRepo.Item it = find(r[0]);
                    if (it == null) { it = new MediaRepo.Item(); it.uri = r[0]; it.title = r[1]; it.sub = ""; it.video = "1".equals(r[2]); }
                    items.add(it);
                }
                List<MediaRepo.Item> f = MediaRepo.filter(items, query);
                for (int i = 0; i < f.size(); i++) rows.add(Row.item(f.get(i), f, i, false));
                break;
            }
            default:
                title = Tr.t("videos");
                List<MediaRepo.Item> vs = MediaRepo.filter(repo.videos, query);
                for (int i = 0; i < vs.size(); i++) rows.add(Row.item(vs.get(i), vs, i, true));
        }
        header.setText(title);
        adapter.set(rows);
        boolean empty = rows.isEmpty() || (rows.size() == 1 && rows.get(0).back);
        emptyView.setVisibility(empty && scanned ? View.VISIBLE : View.GONE);
        emptyView.setText(Tr.t("empty"));
    }

    private MediaRepo.Item find(String uri) {
        for (MediaRepo.Item i : repo.videos) if (i.uri.equals(uri)) return i;
        for (MediaRepo.Item i : repo.music) if (i.uri.equals(uri)) return i;
        return null;
    }

    static final class Row {
        MediaRepo.Item item;
        List<MediaRepo.Item> queue;
        int index;
        MediaRepo.Folder folder;
        boolean card, back;
        static Row item(MediaRepo.Item it, List<MediaRepo.Item> q, int i, boolean card) {
            Row r = new Row(); r.item = it; r.queue = q; r.index = i; r.card = card; return r;
        }
        static Row folder(MediaRepo.Folder f) { Row r = new Row(); r.folder = f; return r; }
        static Row back() { Row r = new Row(); r.back = true; return r; }
        boolean grant;
        static Row grant() { Row r = new Row(); r.grant = true; return r; }
    }

    private final class RowAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {
        private final List<Row> rows = new ArrayList<>();
        private final int cols;
        RowAdapter(int cols) { this.cols = cols; }
        @SuppressLint("NotifyDataSetChanged")
        void set(List<Row> r) { rows.clear(); rows.addAll(r); notifyDataSetChanged(); }
        boolean isCard(int pos) { return pos < rows.size() && rows.get(pos).card; }
        @Override public int getItemCount() { return rows.size(); }
        @Override public int getItemViewType(int pos) { return rows.get(pos).card ? 1 : 0; }

        @NonNull @Override public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int type) {
            LinearLayout v = new LinearLayout(MainActivity.this);
            v.setFocusable(true);
            v.setClickable(true);
            v.setBackground(g.panel(px(16), dp, 0));
            RecyclerView.LayoutParams lp = new RecyclerView.LayoutParams(-1, -2);
            lp.setMargins(px(5), px(5), px(5), px(5));
            v.setLayoutParams(lp);
            ImageView img = new ImageView(MainActivity.this);
            img.setScaleType(ImageView.ScaleType.CENTER_CROP);
            img.setClipToOutline(true);
            TextView t1 = text("", 15, g.text, true);
            TextView t2 = text("", 12, g.muted, false);
            t1.setMaxLines(2);
            t1.setEllipsize(TextUtils.TruncateAt.END);
            t2.setSingleLine(true);
            t2.setEllipsize(TextUtils.TruncateAt.END);
            if (type == 1) {
                v.setOrientation(LinearLayout.VERTICAL);
                v.setPadding(px(6), px(6), px(6), px(10));
                FrameLayout thumbFrame = new FrameLayout(MainActivity.this);
                thumbFrame.addView(img, new FrameLayout.LayoutParams(-1, -1));
                v.addView(thumbFrame, new LinearLayout.LayoutParams(-1, px(118)));
                t1.setPadding(px(4), px(8), px(4), 0);
                t2.setPadding(px(4), px(2), px(4), 0);
                v.addView(t1);
                v.addView(t2);
            } else {
                v.setOrientation(LinearLayout.HORIZONTAL);
                v.setGravity(Gravity.CENTER_VERTICAL);
                v.setPadding(px(8), px(8), px(12), px(8));
                v.setMinimumHeight(px(72));
                v.addView(img, new LinearLayout.LayoutParams(px(56), px(56)));
                LinearLayout tx = new LinearLayout(MainActivity.this);
                tx.setOrientation(LinearLayout.VERTICAL);
                tx.setPadding(px(12), 0, px(8), 0);
                tx.addView(t1);
                tx.addView(t2);
                v.addView(tx, new LinearLayout.LayoutParams(0, -2, 1f));
            }
            v.setTag(new View[]{img, t1, t2});
            return new RecyclerView.ViewHolder(v) { };
        }

        @Override public void onBindViewHolder(@NonNull RecyclerView.ViewHolder h, int pos) {
            Row r = rows.get(pos);
            View[] parts = (View[]) h.itemView.getTag();
            ImageView img = (ImageView) parts[0];
            TextView t1 = (TextView) parts[1], t2 = (TextView) parts[2];
            img.setTag(null);
            img.setBackground(g.pill(g.field(), px(10)));
            int pad = r.card ? px(30) : px(14);
            img.setPadding(pad, pad, pad, pad);
            if (r.grant) {
                img.setImageDrawable(new Icon("folder", g.onAccent));
                img.setBackground(g.pill(g.accent, px(10)));
                t1.setText(Tr.t("grantAll"));
                t2.setText(Tr.t("grantAllHint"));
                h.itemView.setOnClickListener(v -> requestAllFiles());
                return;
            }
            if (r.back) {
                img.setImageDrawable(new Icon("back", g.text));
                t1.setText(Tr.t("back"));
                t2.setText("");
                h.itemView.setOnClickListener(v -> { openFolder = null; refreshList(); });
                return;
            }
            if (r.folder != null) {
                img.setImageDrawable(new Icon("folder", g.accent));
                t1.setText(r.folder.name);
                String vol = TextUtils.isEmpty(r.folder.volume) ? "" : "  ·  " + r.folder.volume;
                t2.setText(String.format(Locale.getDefault(), Tr.t("items"), r.folder.items.size()) + vol);
                h.itemView.setOnClickListener(v -> { openFolder = r.folder; refreshList(); list.scrollToPosition(0); });
                return;
            }
            MediaRepo.Item it = r.item;
            img.setImageDrawable(new Icon(it.video ? "video" : "music", g.muted));
            t1.setText(it.title);
            String dur = it.duration > 0 ? time(it.duration) : "";
            t2.setText(TextUtils.isEmpty(it.sub) ? dur : (dur.isEmpty() ? it.sub : dur + "  ·  " + it.sub));
            h.itemView.setOnClickListener(v -> play(r.queue, r.index));
            if (it.id > 0 || it.path != null) {
                img.setTag(it.uri);
                io.execute(() -> {
                    Bitmap b = MediaRepo.thumbnail(MainActivity.this, it, r.card ? 384 : 160, r.card ? 216 : 160);
                    if (b == null) return;
                    ui.post(() -> {
                        if (it.uri.equals(img.getTag())) { img.setPadding(0, 0, 0, 0); img.setImageBitmap(b); }
                    });
                });
            }
        }
    }

    // ------------------------------------------------------------------ player page
    private void buildPlayer(boolean compact) {
        playerPage = new FrameLayout(this);
        playerPage.setBackgroundColor(Color.BLACK);
        playerPage.setVisibility(View.GONE);
        playerPage.setClickable(true);

        audioArt = new ImageView(this);
        audioArt.setScaleType(ImageView.ScaleType.FIT_CENTER);
        audioArt.setBackground(g.backdrop());
        int ap = px(compact ? 50 : 90);
        audioArt.setPadding(ap, ap, ap, ap + px(110));
        playerPage.addView(audioArt, new FrameLayout.LayoutParams(-1, -1));

        playerView = new PlayerView(this);
        playerView.setUseController(false);
        playerView.setShutterBackgroundColor(Color.BLACK);
        playerView.setKeepContentOnPlayerReset(true);
        applySubtitleStyle();
        playerPage.addView(playerView, new FrameLayout.LayoutParams(-1, -1));
        playerView.setOnClickListener(v -> setControlsVisible(pBottom.getVisibility() != View.VISIBLE));
        audioArt.setOnClickListener(v -> setControlsVisible(true));

        int btn = compact ? 56 : 66;
        // top bar
        pTop = new LinearLayout(this);
        pTop.setGravity(Gravity.CENTER_VERTICAL);
        pTop.setBackground(g.panel(px(20), dp, 40));
        pTop.setPadding(px(6), px(4), px(14), px(4));
        ImageButton back = iconButton("back", g.text, btn);
        back.setContentDescription(Tr.t("back"));
        back.setOnClickListener(v -> showPlayer(false));
        if (Tr.isArabic()) back.setScaleX(-1);
        pTop.addView(back);
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        pTitle = text("", compact ? 17 : 20, g.text, true);
        pTitle.setSingleLine(true);
        pTitle.setEllipsize(TextUtils.TruncateAt.MARQUEE);
        pTitle.setMarqueeRepeatLimit(-1);
        pTitle.setSelected(true);
        pSub = text("", 13, g.muted, false);
        pSub.setSingleLine(true);
        titles.addView(pTitle);
        titles.addView(pSub);
        pTop.addView(titles, new LinearLayout.LayoutParams(0, -2, 1f));
        pFav = iconButton("star", g.text, btn);
        pFav.setOnClickListener(v -> toggleFavorite());
        pTop.addView(pFav);
        TextClock clock = new TextClock(this);
        clock.setFormat12Hour("h:mm");
        clock.setFormat24Hour("HH:mm");
        clock.setTextColor(g.text);
        clock.setTextSize(compact ? 18 : 22);
        clock.setTypeface(Typeface.DEFAULT_BOLD);
        clock.setPadding(px(8), 0, 0, 0);
        pTop.addView(clock);
        FrameLayout.LayoutParams tlp = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        tlp.setMargins(px(10), px(10), px(10), 0);
        playerPage.addView(pTop, tlp);

        // bottom controls
        pBottom = new LinearLayout(this);
        pBottom.setOrientation(LinearLayout.VERTICAL);
        pBottom.setBackground(g.panel(px(24), dp, 45));
        pBottom.setPadding(px(14), px(8), px(14), px(8));
        pBottom.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);   // media transport is never mirrored
        LinearLayout seekRow = new LinearLayout(this);
        seekRow.setGravity(Gravity.CENTER_VERTICAL);
        pCur = text("0:00", 14, g.text, true);
        pDur = text("0:00", 14, g.text, true);
        pSeek = new SeekBar(this);
        pSeek.setMax(1000);
        pSeek.setMinimumHeight(px(40));
        pSeek.getProgressDrawable().setTint(g.accent);
        pSeek.getThumb().setTint(g.accent);
        pSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean user) {
                if (user && controller != null) pCur.setText(time(controller.getDuration() * p / 1000));
            }
            public void onStartTrackingTouch(SeekBar s) { seeking = true; ui.removeCallbacks(hideControls); }
            public void onStopTrackingTouch(SeekBar s) {
                seeking = false;
                if (controller != null && controller.getDuration() > 0) controller.seekTo(controller.getDuration() * s.getProgress() / 1000);
                scheduleHide();
            }
        });
        seekRow.addView(pCur);
        seekRow.addView(pSeek, new LinearLayout.LayoutParams(0, px(48), 1f));
        seekRow.addView(pDur);
        pBottom.addView(seekRow);

        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER);
        pRepeat = iconButton("repeat", g.text, btn);
        pRepeat.setOnClickListener(v -> { act("repeat"); refreshPlayerState(); });
        ImageButton prev = iconButton("prev", g.text, btn);
        prev.setOnClickListener(v -> act("previous"));
        ImageButton rew = iconButton("rewind", g.text, btn);
        rew.setOnClickListener(v -> act("rewind"));
        pPlay = iconButton("play", g.onAccent, btn + 16);
        pPlay.setBackground(g.pill(g.accent, px(btn + 16)));
        pPlay.setOnClickListener(v -> act("playPause"));
        ImageButton fwd = iconButton("forward", g.text, btn);
        fwd.setOnClickListener(v -> act("forward"));
        ImageButton next = iconButton("next", g.text, btn);
        next.setOnClickListener(v -> act("next"));
        pShuffle = iconButton("shuffle", g.text, btn);
        pShuffle.setOnClickListener(v -> { act("shuffle"); refreshPlayerState(); });
        ImageButton audio = iconButton("audio", g.text, btn);
        audio.setOnClickListener(v -> chooseTrack(C.TRACK_TYPE_AUDIO));
        ImageButton subs = iconButton("subtitles", g.text, btn);
        subs.setOnClickListener(v -> chooseTrack(C.TRACK_TYPE_TEXT));
        ImageButton resize = iconButton("resize", g.text, btn);
        resize.setOnClickListener(v -> cycleResize());
        ImageButton speed = iconButton("speed", g.text, btn);
        speed.setOnClickListener(v -> chooseSpeed());
        for (View v : new View[]{pRepeat, prev, rew, pPlay, fwd, next, pShuffle}) { space(v); row.addView(v); }
        LinearLayout extra = new LinearLayout(this);
        extra.setGravity(Gravity.CENTER);
        for (View v : new View[]{audio, subs, resize, speed}) { space(v); extra.addView(v); }
        DisplayMetrics m = getResources().getDisplayMetrics();
        boolean wide = m.widthPixels / dp >= 760;
        if (wide) {
            LinearLayout both = new LinearLayout(this);
            both.setGravity(Gravity.CENTER_VERTICAL);
            both.addView(row, new LinearLayout.LayoutParams(0, -2, 1f));
            both.addView(extra);
            pBottom.addView(both);
        } else {
            pBottom.addView(row);
            pBottom.addView(extra);
        }
        FrameLayout.LayoutParams blp = new FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM);
        blp.setMargins(px(10), 0, px(10), px(10));
        playerPage.addView(pBottom, blp);
        root.addView(playerPage, new FrameLayout.LayoutParams(-1, -1));
    }

    private void space(View v) {
        ((LinearLayout.LayoutParams) v.getLayoutParams()).setMargins(px(3), 0, px(3), 0);
    }

    private void showPlayer(boolean show) {
        playerVisible = show;
        playerPage.setVisibility(show ? View.VISIBLE : View.GONE);
        if (show) { setControlsVisible(true); refreshPlayerState(); }
        updateKeepScreen();
    }

    private void setControlsVisible(boolean v) {
        pTop.setVisibility(v ? View.VISIBLE : View.GONE);
        pBottom.setVisibility(v ? View.VISIBLE : View.GONE);
        if (v) scheduleHide();
    }

    private void scheduleHide() {
        ui.removeCallbacks(hideControls);
        if (controller != null && controller.isPlaying() && isVideo()) ui.postDelayed(hideControls, 4500);
    }

    private boolean isVideo() {
        if (controller == null) return false;
        Tracks t = controller.getCurrentTracks();
        if (t.isTypeSupported(C.TRACK_TYPE_VIDEO)) {
            for (Tracks.Group gr : t.getGroups())
                if (gr.getType() == C.TRACK_TYPE_VIDEO && gr.isSelected()) {
                    // album art in audio files is reported as an image, not video
                    String mime = gr.getMediaTrackGroup().getFormat(0).sampleMimeType;
                    if (mime == null || !mime.startsWith("image/")) return true;
                }
        }
        MediaItem cur = controller.getCurrentMediaItem();
        Bundle ex = cur == null ? null : cur.mediaMetadata.extras;
        return ex != null && ex.getBoolean(PlaybackService.EXTRA_VIDEO) && t.getGroups().isEmpty();
    }

    // ------------------------------------------------------------------ playback actions
    private void play(List<MediaRepo.Item> items, int index) {
        if (controller == null) return;
        List<MediaItem> mi = new ArrayList<>();
        int start = 0, from = 0, to = items.size();
        if (items.size() > 400) { from = Math.max(0, index - 200); to = Math.min(items.size(), from + 400); }
        for (int i = from; i < to; i++) {
            MediaRepo.Item it = items.get(i);
            mi.add(PlaybackService.build(it.uri, it.title, it.sub, it.path, it.video));
            if (i == index) start = i - from;
        }
        controller.setMediaItems(mi, start, C.TIME_UNSET);
        controller.prepare();
        controller.play();
        if (items.get(index).video) showPlayer(true);
        else mini.setVisibility(View.VISIBLE);
    }

    private void act(String action) {
        if (controller == null) return;
        if ("openPlayer".equals(action)) { if (controller.getMediaItemCount() > 0) showPlayer(true); return; }
        if ("ignore".equals(action)) return;
        PlaybackService.perform(controller, action);
        if ("repeat".equals(action)) {
            int r = controller.getRepeatMode();
            toast(Tr.t(r == Player.REPEAT_MODE_ONE ? "repeatOne" : r == Player.REPEAT_MODE_ALL ? "repeatAll" : "repeatOff"));
        } else if ("shuffle".equals(action)) {
            toast(Tr.t(controller.getShuffleModeEnabled() ? "shuffleOn" : "shuffleOff"));
        }
        if (playerVisible) setControlsVisible(true);
    }

    private void toggleFavorite() {
        if (controller == null) return;
        MediaItem cur = controller.getCurrentMediaItem();
        if (cur == null) return;
        Bundle ex = cur.mediaMetadata.extras;
        boolean added = prefs.toggleFavorite(cur.mediaId, String.valueOf(cur.mediaMetadata.title),
                ex != null && ex.getBoolean(PlaybackService.EXTRA_VIDEO));
        toast(Tr.t(added ? "addFav" : "removeFav"));
        refreshPlayerState();
        if ("favorites".equals(tab)) refreshList();
    }

    private void chooseTrack(int type) {
        if (controller == null) return;
        List<String> labels = new ArrayList<>();
        List<Object[]> choices = new ArrayList<>();
        if (type == C.TRACK_TYPE_TEXT) { labels.add(Tr.t("off")); choices.add(null); }
        int checked = type == C.TRACK_TYPE_TEXT ? 0 : -1;
        for (Tracks.Group gr : controller.getCurrentTracks().getGroups()) {
            if (gr.getType() != type) continue;
            for (int i = 0; i < gr.length; i++) {
                if (!gr.isTrackSupported(i)) continue;
                androidx.media3.common.Format f = gr.getTrackFormat(i);
                StringBuilder sb = new StringBuilder();
                if (f.label != null) sb.append(f.label).append(' ');
                if (f.language != null && !"und".equals(f.language)) sb.append('[').append(f.language).append("] ");
                if (f.channelCount > 0) sb.append(f.channelCount).append("ch ");
                if (f.sampleMimeType != null) sb.append('(').append(f.sampleMimeType.replaceAll(".*/", "")).append(')');
                labels.add(sb.length() == 0 ? "#" + (labels.size() + 1) : sb.toString().trim());
                choices.add(new Object[]{gr, i});
                if (gr.isTrackSelected(i)) checked = labels.size() - 1;
            }
        }
        if (choices.isEmpty() || (type == C.TRACK_TYPE_TEXT && choices.size() == 1)) {
            toast(Tr.t(type == C.TRACK_TYPE_AUDIO ? "audio" : "subtitles") + ": —");
            return;
        }
        new AlertDialog.Builder(this, dialogTheme())
                .setTitle(Tr.t(type == C.TRACK_TYPE_AUDIO ? "audio" : "subtitles"))
                .setSingleChoiceItems(labels.toArray(new String[0]), checked, (d, which) -> {
                    Object[] ch = choices.get(which);
                    androidx.media3.common.TrackSelectionParameters.Builder b = controller.getTrackSelectionParameters().buildUpon();
                    if (ch == null) b.setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true);
                    else {
                        Tracks.Group gr = (Tracks.Group) ch[0];
                        b.setTrackTypeDisabled(type, false);
                        b.setOverrideForType(new TrackSelectionOverride(gr.getMediaTrackGroup(), (int) ch[1]));
                    }
                    controller.setTrackSelectionParameters(b.build());
                    d.dismiss();
                }).show();
    }

    private int resizeMode = AspectRatioFrameLayout.RESIZE_MODE_FIT;

    private void cycleResize() {
        int[] modes = {AspectRatioFrameLayout.RESIZE_MODE_FIT, AspectRatioFrameLayout.RESIZE_MODE_ZOOM, AspectRatioFrameLayout.RESIZE_MODE_FILL};
        String[] names = {"fit", "fill", "stretch"};
        int idx = 0;
        for (int i = 0; i < modes.length; i++) if (modes[i] == resizeMode) idx = i;
        idx = (idx + 1) % modes.length;
        resizeMode = modes[idx];
        playerView.setResizeMode(resizeMode);
        toast(Tr.t("resize") + ": " + Tr.t(names[idx]));
    }

    private void chooseSpeed() {
        if (controller == null) return;
        final float[] speeds = {0.5f, 0.75f, 1f, 1.25f, 1.5f, 1.75f, 2f};
        String[] labels = new String[speeds.length];
        int checked = 2;
        for (int i = 0; i < speeds.length; i++) {
            labels[i] = speeds[i] + "×";
            if (Math.abs(controller.getPlaybackParameters().speed - speeds[i]) < 0.01f) checked = i;
        }
        new AlertDialog.Builder(this, dialogTheme()).setTitle(Tr.t("speed"))
                .setSingleChoiceItems(labels, checked, (d, w) -> { controller.setPlaybackSpeed(speeds[w]); d.dismiss(); }).show();
    }

    private int dialogTheme() {
        return g.dark ? android.R.style.Theme_Material_Dialog_Alert : android.R.style.Theme_Material_Light_Dialog_Alert;
    }

    // ------------------------------------------------------------------ controller state
    private final Player.Listener playerListener = new Player.Listener() {
        @Override public void onEvents(@NonNull Player player, @NonNull Player.Events events) { refreshPlayerState(); }
        @Override public void onPlayerError(@NonNull PlaybackException error) {
            toast(Tr.t("error") + " (" + error.getErrorCodeName() + ")");
            if (controller != null && controller.hasNextMediaItem()) ui.postDelayed(() -> { if (controller != null) controller.seekToNextMediaItem(); }, 1500);
        }
    };

    private void onControllerReady() {
        refreshPlayerState();
        if (pendingViewUri != null) { playPendingUri(); return; }
        if (controller.getMediaItemCount() == 0 && prefs.bool("resumeOnOpen", true)) restoreSession(pendingAutoplay || prefs.bool("resumeOnOpen", true));
        pendingAutoplay = false;
    }

    private void playPendingUri() {
        Uri u = pendingViewUri;
        pendingViewUri = null;
        if (u == null || controller == null) return;
        String name = u.getLastPathSegment() == null ? u.toString() : u.getLastPathSegment();
        String type = getContentResolver().getType(u);
        boolean video = type == null || type.startsWith("video");
        String path = "file".equals(u.getScheme()) ? u.getPath() : null;
        controller.setMediaItem(PlaybackService.build(u.toString(), MediaRepo.stripExt(name), "", path, video));
        controller.prepare();
        controller.play();
        showPlayer(true);
    }

    private void restoreSession(boolean autoplay) {
        JSONObject s = prefs.session();
        JSONArray uris = s.optJSONArray("uris"), titles = s.optJSONArray("titles");
        if (uris == null || uris.length() == 0) return;
        List<MediaItem> items = new ArrayList<>();
        for (int i = 0; i < uris.length(); i++) {
            String[] meta = (titles != null ? titles.optString(i, "") : "").split("\u0001", -1);
            String title = meta.length > 0 ? meta[0] : "";
            String path = meta.length > 1 ? meta[1] : null;
            boolean video = meta.length > 2 && "1".equals(meta[2]);
            items.add(PlaybackService.build(uris.optString(i), title, "", path, video));
        }
        int idx = Math.min(Math.max(0, s.optInt("index", 0)), items.size() - 1);
        controller.setMediaItems(items, idx, Math.max(0, s.optLong("pos", 0)));
        controller.prepare();
        if (autoplay) controller.play();
        mini.setVisibility(View.VISIBLE);
    }

    private void refreshPlayerState() {
        if (controller == null || pPlay == null) return;
        boolean has = controller.getMediaItemCount() > 0;
        mini.setVisibility(has ? View.VISIBLE : View.GONE);
        boolean playing = controller.isPlaying();
        pPlay.setImageDrawable(new Icon(playing ? "pause" : "play", g.onAccent));
        miniPlay.setImageDrawable(new Icon(playing ? "pause" : "play", g.onAccent));
        MediaItem cur = controller.getCurrentMediaItem();
        MediaMetadata md = controller.getMediaMetadata();
        String title = md.title != null ? md.title.toString() : (cur != null && cur.mediaMetadata.title != null ? cur.mediaMetadata.title.toString() : Tr.t("nothing"));
        String sub = md.artist != null ? md.artist.toString() : "";
        pTitle.setText(title);
        pSub.setText(sub);
        miniTitle.setText(title);
        miniSub.setText(sub.isEmpty() ? (controller.getMediaItemCount() > 1
                ? (controller.getCurrentMediaItemIndex() + 1) + " / " + controller.getMediaItemCount() : "") : sub);
        boolean fav = cur != null && prefs.isFavorite(cur.mediaId);
        pFav.setImageDrawable(new Icon(fav ? "starFill" : "star", fav ? g.accent : g.text));
        int rep = controller.getRepeatMode();
        pRepeat.setImageDrawable(new Icon(rep == Player.REPEAT_MODE_ONE ? "repeat1" : "repeat", rep == Player.REPEAT_MODE_OFF ? g.muted : g.accent));
        pShuffle.setImageDrawable(new Icon("shuffle", controller.getShuffleModeEnabled() ? g.accent : g.muted));

        boolean video = isVideo();
        playerView.setVisibility(video ? View.VISIBLE : View.INVISIBLE);
        audioArt.setVisibility(video ? View.GONE : View.VISIBLE);
        String curId = cur == null ? null : cur.mediaId;
        boolean artChanged = md.artworkData != lastArtData || (curId != null && !curId.equals(lastArtId));
        if (artChanged) {
            lastArtId = curId;
            lastArtData = md.artworkData;
            lastArt = md.artworkData == null ? null
                    : android.graphics.BitmapFactory.decodeByteArray(md.artworkData, 0, md.artworkData.length);
            if (lastArt != null) { audioArt.setImageBitmap(lastArt); miniArt.setImageBitmap(lastArt); }
            else {
                audioArt.setImageResource(R.drawable.logo_big);
                MediaRepo.Item it = cur == null ? null : find(cur.mediaId);
                miniArt.setImageResource(R.drawable.logo_big);
                if (it != null) io.execute(() -> {
                    Bitmap bmp = MediaRepo.thumbnail(this, it, 160, 160);
                    if (bmp != null) ui.post(() -> { if (it.uri.equals(lastArtId)) miniArt.setImageBitmap(bmp); });
                });
            }
        }
        if (!video && playerVisible) setControlsVisible(true);
        else if (playerVisible && playing) scheduleHide();
        updateKeepScreen();
        updateProgress();
    }

    private void updateProgress() {
        if (controller == null || seeking || pSeek == null) return;
        long d = controller.getDuration(), p = controller.getCurrentPosition();
        if (d > 0 && d != C.TIME_UNSET) {
            pSeek.setProgress((int) (p * 1000 / d));
            pDur.setText(time(d));
        } else {
            pSeek.setProgress(0);
            pDur.setText("--:--");
        }
        pCur.setText(time(p));
    }

    private void updateKeepScreen() {
        boolean keep = controller != null && controller.isPlaying() && isVideo() && prefs.bool("keepScreen", true);
        if (keep) getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        else getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void applySubtitleStyle() {
        if (playerView == null || playerView.getSubtitleView() == null) return;
        int size = prefs.integer("subSize", 2);
        float[] sizes = {18, 22, 28, 36};
        playerView.getSubtitleView().setFixedTextSize(TypedValue.COMPLEX_UNIT_DIP, sizes[Math.max(0, Math.min(3, size))]);
        boolean bg = prefs.bool("subBg", false);
        playerView.getSubtitleView().setStyle(new CaptionStyleCompat(Color.WHITE, bg ? 0x99000000 : Color.TRANSPARENT,
                Color.TRANSPARENT, CaptionStyleCompat.EDGE_TYPE_OUTLINE, Color.BLACK, Typeface.DEFAULT_BOLD));
        playerView.getSubtitleView().setApplyEmbeddedStyles(true);
        playerView.getSubtitleView().setBottomPaddingFraction(0.12f);
    }

    // ------------------------------------------------------------------ keys (steering wheel, remote, rotary)
    @Override protected void onResume() {
        super.onResume();
        KeyBus.uiVisible = true;
        // Rescan once the user comes back after granting "all files" access
        if (scanned && !hadFullAccess && MediaRepo.hasFullAccess(this)) scan();
    }
    @Override protected void onPause() { KeyBus.uiVisible = false; super.onPause(); }

    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        int code = e.getKeyCode();
        if (KeyBus.learn != null) {
            if (code == KeyEvent.KEYCODE_BACK) return super.dispatchKeyEvent(e);
            if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) KeyBus.report(code, "key event");
            return true;
        }
        Map<Integer, String> map = prefs.keyMap();
        String action = map.get(code);
        if (action != null && controller != null) {
            if (e.getAction() == KeyEvent.ACTION_DOWN && e.getRepeatCount() == 0) {
                KeyBus.lastEvent = KeyBus.name(code) + "  ·  key event";
                act(action);
            }
            return true;
        }
        if (e.getAction() == KeyEvent.ACTION_DOWN) KeyBus.lastEvent = KeyBus.name(code) + "  ·  key event";
        if (playerVisible && e.getAction() == KeyEvent.ACTION_DOWN) setControlsVisible(true);
        return super.dispatchKeyEvent(e);
    }

    private void requestAllFiles() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
            } catch (Exception ex) {
                try { startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)); }
                catch (Exception ex2) { toast(Tr.t("grantAllHint")); }
            }
        } else {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, REQ_PERMS);
        }
    }

    private void startLearning() {
        TextView msg = text(Tr.t("pressNow"), 20, g.text, true);
        msg.setGravity(Gravity.CENTER);
        msg.setPadding(px(24), px(40), px(24), px(40));
        learnDialog = new AlertDialog.Builder(this, dialogTheme()).setTitle(Tr.t("learn")).setView(msg)
                .setNegativeButton(Tr.t("close"), null).create();
        learnDialog.setOnDismissListener(d -> KeyBus.learn = null);
        KeyBus.learn = (code, source) -> onKeyLearned(code, source);
        learnDialog.show();
    }

    private void onKeyLearned(int code, String source) {
        KeyBus.learn = null;
        if (learnDialog != null) { learnDialog.dismiss(); learnDialog = null; }
        String[] labels = new String[Prefs.ACTIONS.length];
        for (int i = 0; i < labels.length; i++) labels[i] = Tr.t("a." + Prefs.ACTIONS[i]);
        new AlertDialog.Builder(this, dialogTheme())
                .setTitle(Tr.t("chooseAction") + "\n" + KeyBus.name(code) + "  (" + source + ")")
                .setItems(labels, (d, w) -> { prefs.setKey(code, Prefs.ACTIONS[w]); showSettings(); })
                .show();
    }

    // ------------------------------------------------------------------ settings
    private void showLibrary() {
        if (settingsView != null) settingsView.setVisibility(View.GONE);
        library.setVisibility(View.VISIBLE);
    }

    private void showSettings() {
        library.setVisibility(View.GONE);
        if (settingsView != null) content.removeView(settingsView);
        settingsView = new ScrollView(this);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(px(12), px(12), px(12), px(24));
        settingsView.addView(col);
        content.addView(settingsView, new FrameLayout.LayoutParams(-1, -1));

        col.addView(text(Tr.t("settings"), 24, g.text, true));

        LinearLayout lang = section(col, Tr.t("language"));
        lang.addView(chipRow(new String[]{"العربية", "English"}, Tr.isArabic() ? 0 : 1, i -> {
            prefs.setArabic(i == 0);
            Tr.setArabic(i == 0);
            rebuildKeepingTab();
        }));

        LinearLayout theme = section(col, Tr.t("theme"));
        String[] names = new String[Glass.IDS.length];
        int cur = 0;
        for (int i = 0; i < names.length; i++) { names[i] = Tr.t("t." + Glass.IDS[i]); if (Glass.IDS[i].equals(prefs.theme())) cur = i; }
        theme.addView(chipRow(names, cur, i -> { prefs.setTheme(Glass.IDS[i]); rebuildKeepingTab(); }));

        LinearLayout keys = section(col, Tr.t("steering"));
        TextView hint = text(Tr.t("steeringHint"), 14, g.muted, false);
        keys.addView(hint);
        for (Map.Entry<Integer, String> e : prefs.customKeys().entrySet()) {
            keys.addView(text("• " + KeyEvent.keyCodeToString(e.getKey()).replace("KEYCODE_", "") + "  →  " + Tr.t("a." + e.getValue()), 15, g.text, false));
        }
        LinearLayout kb = new LinearLayout(this);
        kb.addView(button(Tr.t("learn"), true, v -> startLearning()));
        kb.addView(button(Tr.t("resetKeys"), false, v -> { prefs.resetKeys(); showSettings(); }));
        keys.addView(kb);

        // Global capture through accessibility (works with units that only send keys to the app in front)
        boolean acc = KeyCaptureService.isEnabled(this);
        TextView accState = text(Tr.t("globalCapture") + ": " + Tr.t(acc ? "enabled" : "disabled"), 16, acc ? g.accent : g.text, true);
        accState.setPadding(0, px(14), 0, px(2));
        keys.addView(accState);
        keys.addView(text(Tr.t("globalCaptureHint"), 13, g.muted, false));
        keys.addView(button(Tr.t(acc ? "globalCaptureManage" : "globalCaptureEnable"), !acc, v -> {
            try { startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)); }
            catch (Exception ex) { toast(Tr.t("globalCaptureHint")); }
            toast(Tr.t("globalCaptureFind"));
        }));

        // Live button test: shows the last button and the channel it came through
        TextView testTitle = text(Tr.t("buttonTest"), 16, g.text, true);
        testTitle.setPadding(0, px(14), 0, px(2));
        keys.addView(testTitle);
        lastKeyView = text(KeyBus.lastEvent.isEmpty() ? Tr.t("pressAny") : KeyBus.lastEvent, 18, g.accent, true);
        keys.addView(lastKeyView);
        keys.addView(text(Tr.t("unitTip"), 13, g.muted, false));

        LinearLayout play = section(col, Tr.t("playback"));
        play.addView(toggle(Tr.t("resumeOnOpen"), "resumeOnOpen", true));
        play.addView(toggle(Tr.t("rememberPos"), "rememberPos", true));
        play.addView(toggle(Tr.t("autoStart"), "autoStart", false));
        play.addView(toggle(Tr.t("keepScreen"), "keepScreen", true));

        LinearLayout subs = section(col, Tr.t("subtitles"));
        subs.addView(text(Tr.t("subSize"), 15, g.text, false));
        subs.addView(chipRow(new String[]{Tr.t("small"), Tr.t("medium"), Tr.t("large"), Tr.t("xlarge")}, prefs.integer("subSize", 2),
                i -> { prefs.setInt("subSize", i); applySubtitleStyle(); showSettings(); }));
        subs.addView(toggle(Tr.t("subBg"), "subBg", false));

        LinearLayout st = section(col, Tr.t("storage"));
        LinearLayout sb = new LinearLayout(this);
        sb.addView(button(Tr.t("rescan"), true, v -> { scan(); selectTab("videos"); }));
        if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager()) {
            sb.addView(button(Tr.t("allFiles"), false, v -> {
                try {
                    startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
                } catch (Exception ex) {
                    try { startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)); } catch (Exception ignored) { }
                }
            }));
        }
        st.addView(sb);

        LinearLayout about = section(col, Tr.t("about"));
        LinearLayout ab = new LinearLayout(this);
        ab.setGravity(Gravity.CENTER_VERTICAL);
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.logo_big);
        ab.addView(logo, new LinearLayout.LayoutParams(px(84), px(84)));
        LinearLayout at = new LinearLayout(this);
        at.setOrientation(LinearLayout.VERTICAL);
        at.setPadding(px(14), 0, px(14), 0);
        at.addView(text("iBRA PLAYER", 22, g.text, true));
        String ver = "";
        try { ver = getPackageManager().getPackageInfo(getPackageName(), 0).versionName; } catch (Exception ignored) { }
        at.addView(text(Tr.t("version") + " " + ver, 14, g.muted, false));
        at.addView(text(Tr.t("credit"), 15, g.text, true));
        TextView tg = text("t.me/ibraprogram", 15, g.accent, true);
        tg.setOnClickListener(v -> { try { startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/ibraprogram"))); } catch (Exception ignored) { } });
        at.addView(tg);
        ab.addView(at, new LinearLayout.LayoutParams(0, -2, 1f));
        about.addView(ab);
        about.addView(text(Tr.t("codecNote"), 13, g.muted, false));
    }

    private void rebuildKeepingTab() {
        String t = tab;
        buildUi();
        tab = t;
        selectTab(t);
        if (controller != null) { playerView.setPlayer(controller); refreshPlayerState(); }
    }

    private LinearLayout section(LinearLayout parent, String title) {
        LinearLayout s = new LinearLayout(this);
        s.setOrientation(LinearLayout.VERTICAL);
        s.setBackground(g.panel(px(18), dp, 10));
        s.setPadding(px(16), px(12), px(16), px(14));
        TextView t = text(title, 18, g.text, true);
        t.setPadding(0, 0, 0, px(8));
        s.addView(t);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = px(12);
        parent.addView(s, lp);
        return s;
    }

    interface IntAction { void run(int i); }

    private View chipRow(String[] labels, int selected, IntAction onPick) {
        android.widget.HorizontalScrollView hs = new android.widget.HorizontalScrollView(this);
        hs.setHorizontalScrollBarEnabled(false);
        LinearLayout row = new LinearLayout(this);
        for (int i = 0; i < labels.length; i++) {
            final int idx = i;
            TextView c = text(labels[i], 15, i == selected ? g.onAccent : g.text, i == selected);
            c.setGravity(Gravity.CENTER);
            c.setMinHeight(px(52));
            c.setPadding(px(18), 0, px(18), 0);
            c.setBackground(g.pill(i == selected ? g.accent : g.field(), px(26)));
            c.setFocusable(true);
            c.setClickable(true);
            c.setOnClickListener(v -> onPick.run(idx));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, -2);
            lp.setMargins(0, px(4), px(8), px(4));
            row.addView(c, lp);
        }
        hs.addView(row);
        return hs;
    }

    private View toggle(String label, String key, boolean def) {
        android.widget.Switch sw = new android.widget.Switch(this);
        sw.setText(label);
        sw.setTextColor(g.text);
        sw.setTextSize(16);
        sw.setMinHeight(px(56));
        sw.setChecked(prefs.bool(key, def));
        sw.setOnCheckedChangeListener((b, on) -> prefs.setBool(key, on));
        return sw;
    }

    private View button(String label, boolean accent, View.OnClickListener l) {
        TextView b = text(label, 15, accent ? g.onAccent : g.text, true);
        b.setGravity(Gravity.CENTER);
        b.setMinHeight(px(52));
        b.setPadding(px(20), 0, px(20), 0);
        b.setBackground(g.pill(accent ? g.accent : g.field(), px(26)));
        b.setFocusable(true);
        b.setClickable(true);
        b.setOnClickListener(l);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-2, -2);
        lp.setMargins(0, px(10), px(10), 0);
        b.setLayoutParams(lp);
        return b;
    }

    // ------------------------------------------------------------------ helpers
    private TextView text(String s, float sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        return t;
    }

    private ImageButton iconButton(String icon, int color, int sizeDp) {
        ImageButton b = new ImageButton(this);
        b.setImageDrawable(new Icon(icon, color));
        b.setScaleType(ImageView.ScaleType.FIT_CENTER);
        int p = px(sizeDp * 0.24f);
        b.setPadding(p, p, p, p);
        android.graphics.drawable.StateListDrawable bg = new android.graphics.drawable.StateListDrawable();
        bg.addState(new int[]{android.R.attr.state_pressed}, g.pill(g.pressed(), px(sizeDp)));
        bg.addState(new int[]{android.R.attr.state_focused}, g.pill(g.pressed(), px(sizeDp)));
        bg.addState(new int[]{}, g.pill(Color.TRANSPARENT, px(sizeDp)));
        b.setBackground(bg);
        b.setLayoutParams(new LinearLayout.LayoutParams(px(sizeDp), px(sizeDp)));
        b.setFocusable(true);
        return b;
    }

    private void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_SHORT).show(); }

    static String time(long ms) {
        if (ms < 0) ms = 0;
        long s = ms / 1000, h = s / 3600, m = (s % 3600) / 60, ss = s % 60;
        return h > 0 ? String.format(Locale.US, "%d:%02d:%02d", h, m, ss) : String.format(Locale.US, "%d:%02d", m, ss);
    }
}
