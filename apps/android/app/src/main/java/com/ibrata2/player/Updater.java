package com.ibrata2.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;

/**
 * In-app updates from this project's GitHub releases. The APK is downloaded,
 * checked against the SHA-256 published by GitHub, then installed over the
 * current version through Android's package installer.
 */
final class Updater {
    private static final String API = "https://api.github.com/repos/ibrata2/ibra-player/releases/latest";
    private static final String DOWNLOAD_PREFIX = "https://github.com/ibrata2/ibra-player/releases/download/";
    private static final Handler UI = new Handler(Looper.getMainLooper());
    private static volatile boolean busy;

    static void check(Activity a, boolean silent) {
        if (busy) return;
        busy = true;
        new Thread(() -> {
            try {
                JSONObject rel = new JSONObject(readText(API));
                String tag = rel.optString("tag_name", "").replaceFirst("^[vV]", "");
                String mine = a.getPackageManager().getPackageInfo(a.getPackageName(), 0).versionName;
                if (compare(tag, mine) <= 0) {
                    busy = false;
                    if (!silent) UI.post(() -> toast(a, String.format(Tr.t("updLatest"), mine)));
                    return;
                }
                JSONObject asset = null;
                JSONArray assets = rel.optJSONArray("assets");
                for (int i = 0; assets != null && i < assets.length(); i++) {
                    JSONObject o = assets.getJSONObject(i);
                    if (o.optString("name").toLowerCase(Locale.ROOT).endsWith(".apk")) asset = o;
                }
                if (asset == null) { busy = false; return; }
                final JSONObject chosen = asset;
                UI.post(() -> {
                    if (a.isFinishing()) { busy = false; return; }
                    new AlertDialog.Builder(a, android.R.style.Theme_Material_Dialog_Alert)
                            .setTitle("iBRA PLAYER " + tag)
                            .setMessage(String.format(Tr.t("updAvailable"), tag, mine))
                            .setPositiveButton(Tr.t("updNow"), (d, w) -> download(a, chosen))
                            .setNegativeButton(Tr.t("updLater"), (d, w) -> busy = false)
                            .setOnCancelListener(d -> busy = false)
                            .show();
                });
            } catch (Exception e) {
                busy = false;
                if (!silent) UI.post(() -> toast(a, Tr.t("updFailed")));
            }
        }).start();
    }

    private static void download(Activity a, JSONObject asset) {
        // Android must allow this app to install updates (one-time switch)
        if (Build.VERSION.SDK_INT >= 26 && !a.getPackageManager().canRequestPackageInstalls()) {
            busy = false;
            toast(a, Tr.t("updAllowInstall"));
            try {
                a.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + a.getPackageName())));
            } catch (Exception ignored) { }
            return;
        }
        String url = asset.optString("browser_download_url");
        if (!url.startsWith(DOWNLOAD_PREFIX)) { busy = false; toast(a, Tr.t("updVerifyFailed")); return; }
        long size = asset.optLong("size", -1);
        String digest = asset.optString("digest", "");
        String sha = digest.startsWith("sha256:") ? digest.substring(7).toLowerCase(Locale.ROOT) : "";

        LinearLayout box = new LinearLayout(a);
        box.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (24 * a.getResources().getDisplayMetrics().density);
        box.setPadding(pad, pad, pad, pad);
        TextView label = new TextView(a);
        label.setText(Tr.t("updDownloading"));
        label.setTextSize(16);
        ProgressBar bar = new ProgressBar(a, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(100);
        box.addView(label);
        box.addView(bar);
        AlertDialog dlg = new AlertDialog.Builder(a, android.R.style.Theme_Material_Dialog_Alert)
                .setTitle("iBRA PLAYER").setView(box).setCancelable(false).show();

        new Thread(() -> {
            File out = new File(a.getCacheDir(), "update.apk");
            try {
                HttpURLConnection c = open(url);
                long total = c.getContentLengthLong();
                MessageDigest md = MessageDigest.getInstance("SHA-256");
                try (InputStream in = c.getInputStream(); OutputStream os = new FileOutputStream(out)) {
                    byte[] buf = new byte[64 * 1024];
                    long got = 0;
                    int n, last = -1;
                    while ((n = in.read(buf)) > 0) {
                        os.write(buf, 0, n);
                        md.update(buf, 0, n);
                        got += n;
                        int pct = total > 0 ? (int) (got * 100 / total) : 0;
                        if (pct != last) { last = pct; final int p = pct; UI.post(() -> bar.setProgress(p)); }
                    }
                }
                String hex = toHex(md.digest());
                if ((size > 0 && out.length() != size) || (!sha.isEmpty() && !sha.equals(hex))) {
                    out.delete();
                    throw new SecurityException("verification failed");
                }
                UI.post(() -> label.setText(Tr.t("updInstalling")));
                install(a, out);
                UI.post(dlg::dismiss);
            } catch (Exception e) {
                out.delete();
                busy = false;
                UI.post(() -> {
                    dlg.dismiss();
                    toast(a, e instanceof SecurityException ? Tr.t("updVerifyFailed") : Tr.t("updFailed"));
                });
            }
        }).start();
    }

    private static void install(Context ctx, File apk) throws Exception {
        PackageInstaller pi = ctx.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(ctx.getPackageName());
        if (Build.VERSION.SDK_INT >= 31) params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED);
        int id = pi.createSession(params);
        try (PackageInstaller.Session s = pi.openSession(id)) {
            try (OutputStream os = s.openWrite("base.apk", 0, apk.length()); InputStream in = new FileInputStream(apk)) {
                byte[] buf = new byte[64 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                s.fsync(os);
            }
            Intent cb = new Intent(ctx, Result.class);
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
            PendingIntent pi2 = PendingIntent.getBroadcast(ctx, id, cb, flags);
            s.commit(pi2.getIntentSender());
        }
    }

    /** Receives the installer's status; shows the system confirmation when needed. */
    public static final class Result extends BroadcastReceiver {
        @Override public void onReceive(Context ctx, Intent intent) {
            int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE);
            if (status == PackageInstaller.STATUS_PENDING_USER_ACTION) {
                Intent confirm = Build.VERSION.SDK_INT >= 33
                        ? intent.getParcelableExtra(Intent.EXTRA_INTENT, Intent.class)
                        : intent.getParcelableExtra(Intent.EXTRA_INTENT);
                if (confirm != null) {
                    confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    try { ctx.startActivity(confirm); } catch (Exception ignored) { }
                }
                return;
            }
            busy = false;
            if (status != PackageInstaller.STATUS_SUCCESS) {
                String msg = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE);
                Toast.makeText(ctx, Tr.t("updInstallFailed") + (msg != null ? "\n" + msg : ""), Toast.LENGTH_LONG).show();
            }
        }
    }

    // ---------------------------------------------------------------- helpers
    private static HttpURLConnection open(String url) throws Exception {
        String u = url;
        for (int i = 0; i < 6; i++) {   // follow GitHub's redirect to its download host (HTTPS only)
            HttpURLConnection c = (HttpURLConnection) new URL(u).openConnection();
            c.setInstanceFollowRedirects(false);
            c.setConnectTimeout(15000);
            c.setReadTimeout(30000);
            c.setRequestProperty("User-Agent", "iBRA-PLAYER");
            c.setRequestProperty("Accept", u.startsWith("https://api.github.com") ? "application/vnd.github+json" : "application/octet-stream");
            int code = c.getResponseCode();
            if (code >= 300 && code < 400) {
                String loc = c.getHeaderField("Location");
                c.disconnect();
                if (loc == null || !loc.startsWith("https://")) throw new SecurityException("bad redirect");
                u = loc;
                continue;
            }
            if (code != 200) throw new java.io.IOException("HTTP " + code);
            return c;
        }
        throw new java.io.IOException("too many redirects");
    }

    private static String readText(String url) throws Exception {
        HttpURLConnection c = open(url);
        try (InputStream in = c.getInputStream(); ByteArrayOutputStream bo = new ByteArrayOutputStream()) {
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            return bo.toString("UTF-8");
        }
    }

    static int compare(String a, String b) {
        String[] x = a.split("\\."), y = b.split("\\.");
        for (int i = 0; i < Math.max(x.length, y.length); i++) {
            int p = i < x.length ? parse(x[i]) : 0, q = i < y.length ? parse(y[i]) : 0;
            if (p != q) return Integer.compare(p, q);
        }
        return 0;
    }

    private static int parse(String s) {
        try { return Integer.parseInt(s.replaceAll("[^0-9].*$", "")); } catch (Exception e) { return 0; }
    }

    private static String toHex(byte[] b) {
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format(Locale.ROOT, "%02x", x));
        return sb.toString();
    }

    private static void toast(Context c, String s) { Toast.makeText(c, s, Toast.LENGTH_LONG).show(); }

    private Updater() {}
}
