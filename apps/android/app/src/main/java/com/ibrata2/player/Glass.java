package com.ibrata2.player;

import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;

/** Glass themes shared with the desktop player. */
final class Glass {
    final String id;
    final boolean dark;
    final int bgTop, bgBottom, blobA, blobB, surface, text, muted, accent, onAccent, border;

    private Glass(String id, boolean dark, int bgTop, int bgBottom, int blobA, int blobB, int surface,
                  int text, int muted, int accent, int onAccent, int border) {
        this.id = id; this.dark = dark; this.bgTop = bgTop; this.bgBottom = bgBottom; this.blobA = blobA;
        this.blobB = blobB; this.surface = surface; this.text = text; this.muted = muted; this.accent = accent;
        this.onAccent = onAccent; this.border = border;
    }

    static final String[] IDS = {"system", "red-white", "white", "black", "red-black", "clear"};

    /** Automatic: follows the device's night mode, with the Android 12+ system accent colour. */
    static Glass system(android.content.Context ctx) {
        int night = ctx.getResources().getConfiguration().uiMode & android.content.res.Configuration.UI_MODE_NIGHT_MASK;
        boolean dark = night != android.content.res.Configuration.UI_MODE_NIGHT_NO;
        int accent = 0xFFE3192B;
        if (android.os.Build.VERSION.SDK_INT >= 31) {
            try { accent = ctx.getColor(dark ? android.R.color.system_accent1_300 : android.R.color.system_accent1_600); }
            catch (Exception ignored) { }
        }
        int onAccent = (Color.red(accent) * 299 + Color.green(accent) * 587 + Color.blue(accent) * 114) / 1000 > 170 ? 0xFF111111 : 0xFFFFFFFF;
        int blob = (accent & 0x00FFFFFF) | 0x46000000;
        return dark
                ? new Glass("system", true, 0xFF202124, 0xFF0B0B0D, 0x1EFFFFFF, blob, 0xA81C1C1F,
                        0xFFF3F3F3, 0xFFA0A0A6, accent, onAccent, 0x3DFFFFFF)
                : new Glass("system", false, 0xFFF5F6F8, 0xFFE1E4EA, 0xDCFFFFFF, blob, 0x99FFFFFF,
                        0xFF1B1B1F, 0xFF5F6370, accent, onAccent, 0xD8FFFFFF);
    }

    static Glass of(String id) {
        switch (id) {
            case "red-white": return new Glass(id, false, 0xFFFBF7F7, 0xFFE7DCDD, 0xE6FFFFFF, 0x46E3192B, 0x99FFFFFF,
                    0xFF1C1517, 0xFF6E5F62, 0xFFE3192B, 0xFFFFFFFF, 0xE0FFFFFF);
            case "white": return new Glass(id, false, 0xFFF4F5F8, 0xFFD9DCE3, 0xDCFFFFFF, 0x5AAAB4C8, 0x94FFFFFF,
                    0xFF18191D, 0xFF5D6170, 0xFF2B2E36, 0xFFFFFFFF, 0xD8FFFFFF);
            case "black": return new Glass(id, true, 0xFF1B1C20, 0xFF060608, 0x22FFFFFF, 0x2896A0B4, 0x9E16171B,
                    0xFFF5F6F8, 0xFF9A9CA5, 0xFFE8E9EE, 0xFF111114, 0x42FFFFFF);
            case "clear": return new Glass(id, true, 0xFF3A4150, 0xFF1B1F27, 0x5A8CC8FF, 0x46FFFFFF, 0x24FFFFFF,
                    0xFFFFFFFF, 0xFFD6DBE6, 0xFFFFFFFF, 0xFF1B1F27, 0x8CFFFFFF);
            default: return new Glass("red-black", true, 0xFF1A0B0D, 0xFF050304, 0x46FF283C, 0x5A78000A, 0xA3120708,
                    0xFFFFF4F4, 0xFFC09A9E, 0xFFFF1F35, 0xFFFFFFFF, 0x61FF5464);
        }
    }

    /** Frosted backdrop: gradient plus soft light blobs. */
    Drawable backdrop() {
        return new Drawable() {
            final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
            @Override public void draw(Canvas c) {
                int w = getBounds().width(), h = getBounds().height();
                p.setShader(new LinearGradient(0, 0, w, h, bgTop, bgBottom, Shader.TileMode.CLAMP));
                c.drawRect(getBounds(), p);
                float m = Math.max(w, h);
                blob(c, w * 0.12f, h * 0.05f, m * 0.6f, blobA);
                blob(c, w * 0.92f, h * 0.95f, m * 0.65f, blobB);
            }
            void blob(Canvas c, float x, float y, float r, int col) {
                p.setShader(new RadialGradient(x, y, r, new int[]{col, (col & 0x00FFFFFF) | ((Color.alpha(col) / 3) << 24), col & 0x00FFFFFF},
                        new float[]{0f, 0.45f, 1f}, Shader.TileMode.CLAMP));
                c.drawRect(getBounds(), p);
            }
            @Override public void setAlpha(int a) { }
            @Override public void setColorFilter(ColorFilter cf) { }
            @Override public int getOpacity() { return PixelFormat.OPAQUE; }
        };
    }

    /** Glass panel: tint + top reflection + luminous edge. */
    Drawable panel(float radiusPx, float density, int extraAlpha) {
        int tint = surface;
        int a = Math.min(255, Color.alpha(tint) + extraAlpha);
        tint = (tint & 0x00FFFFFF) | (a << 24);
        GradientDrawable body = new GradientDrawable();
        body.setCornerRadius(radiusPx);
        body.setColor(tint);
        body.setStroke(Math.max(1, Math.round(density)), border);
        GradientDrawable sheen = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[]{dark ? 0x22FFFFFF : 0x55FFFFFF, 0x00FFFFFF, 0x00FFFFFF});
        sheen.setCornerRadius(radiusPx);
        return new LayerDrawable(new Drawable[]{body, sheen});
    }

    Drawable pill(int color, float radiusPx) {
        GradientDrawable d = new GradientDrawable();
        d.setCornerRadius(radiusPx);
        d.setColor(color);
        return d;
    }

    int field() { return dark ? 0x1FFFFFFF : 0x14000000; }
    int pressed() { return (accent & 0x00FFFFFF) | 0x55000000; }
}
