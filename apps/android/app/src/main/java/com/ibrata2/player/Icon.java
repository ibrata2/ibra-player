package com.ibrata2.player;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;

/** Original vector icon set (same shapes as the desktop player), tinted at runtime. */
final class Icon extends Drawable {
    private final String name;
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);

    Icon(String name, int color) {
        this.name = name;
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(1.9f);
        stroke.setStrokeCap(Paint.Cap.ROUND);
        stroke.setStrokeJoin(Paint.Join.ROUND);
        stroke.setColor(color);
        fill.setStyle(Paint.Style.FILL);
        fill.setColor(color);
    }

    @Override public void draw(Canvas c) {
        float s = Math.min(getBounds().width(), getBounds().height()) / 24f;
        c.save();
        c.translate(getBounds().left + (getBounds().width() - 24 * s) / 2f, getBounds().top + (getBounds().height() - 24 * s) / 2f);
        c.scale(s, s);
        Path p = new Path();
        switch (name) {
            case "play":
                p.moveTo(8, 5.2f); p.cubicTo(8, 4.2f, 9, 3.7f, 9.9f, 4.2f); p.lineTo(19.2f, 10.9f);
                p.cubicTo(20, 11.4f, 20, 12.6f, 19.2f, 13.1f); p.lineTo(9.9f, 19.8f); p.cubicTo(9, 20.3f, 8, 19.8f, 8, 18.8f); p.close();
                c.drawPath(p, fill); break;
            case "pause":
                c.drawRoundRect(new RectF(6.5f, 5, 10.3f, 19), 1.4f, 1.4f, fill);
                c.drawRoundRect(new RectF(13.7f, 5, 17.5f, 19), 1.4f, 1.4f, fill); break;
            case "stop": c.drawRoundRect(new RectF(6.5f, 6.5f, 17.5f, 17.5f), 2.2f, 2.2f, fill); break;
            case "next": case "prev":
                if (name.equals("prev")) { c.translate(24, 0); c.scale(-1, 1); }
                tri(c, 6, 6, 15, 12, 6, 18); c.drawRoundRect(new RectF(15.8f, 6, 18.4f, 18), 1, 1, fill); break;
            case "forward": case "rewind":
                if (name.equals("rewind")) { c.translate(24, 0); c.scale(-1, 1); }
                c.drawArc(new RectF(4, 4, 20, 20), 200, 290, false, stroke);
                tri(c, 18.5f, 3.5f, 21, 9, 15.2f, 8.4f);
                break;
            case "repeat": case "repeat1":
                poly(c, 5, 11, 5, 8, 18, 8); tri(c, 16, 5, 20, 8, 16, 11);
                poly(c, 19, 13, 19, 16, 6, 16); tri(c, 8, 13, 4, 16, 8, 19);
                if (name.equals("repeat1")) { Paint t = new Paint(fill); t.setTextSize(7); t.setFakeBoldText(true); t.setTextAlign(Paint.Align.CENTER); c.drawText("1", 12, 14.5f, t); }
                break;
            case "shuffle":
                poly(c, 4, 7, 8, 7, 15, 17, 18, 17); poly(c, 4, 17, 8, 17, 15, 7, 18, 7);
                tri(c, 17, 4, 21, 7, 17, 10); tri(c, 17, 14, 21, 17, 17, 20); break;
            case "video":
                c.drawRoundRect(new RectF(3, 6, 16, 18), 2.5f, 2.5f, stroke); tri(c, 16, 10.5f, 21, 7.5f, 21, 16.5f); break;
            case "music":
                c.drawOval(new RectF(4.7f, 14.7f, 10.3f, 19.3f), stroke); c.drawOval(new RectF(14.2f, 12.7f, 19.8f, 17.3f), stroke);
                c.drawLine(10.3f, 17, 10.3f, 5.5f, stroke); c.drawLine(19.8f, 15, 19.8f, 3.8f, stroke); c.drawLine(10.3f, 5.5f, 19.8f, 3.8f, stroke); break;
            case "folder":
                poly(c, 3.5f, 6, 9, 6, 11, 8, 20.5f, 8, 20.5f, 18.5f, 3.5f, 18.5f, 3.5f, 6); break;
            case "recent":
                c.drawCircle(12, 12, 8.5f, stroke); poly(c, 12, 7.5f, 12, 12, 15.5f, 14); break;
            case "star": case "starFill": {
                for (int i = 0; i < 10; i++) {
                    double a = -Math.PI / 2 + i * Math.PI / 5; float r = (i % 2 == 1) ? 3.9f : 8.8f;
                    float x = (float) (12 + r * Math.cos(a)), y = (float) (12.6 + r * Math.sin(a));
                    if (i == 0) p.moveTo(x, y); else p.lineTo(x, y);
                }
                p.close(); c.drawPath(p, name.equals("starFill") ? fill : stroke); break;
            }
            case "settings": {
                for (int i = 0; i < 8; i++) {
                    c.save(); c.rotate(i * 45, 12, 12); c.drawRoundRect(new RectF(10.4f, 2.8f, 13.6f, 7.2f), 0.8f, 0.8f, fill); c.restore();
                }
                Paint ring = new Paint(stroke); ring.setStrokeWidth(3.4f);
                c.drawCircle(12, 12, 5.3f, ring); break;
            }
            case "back":
                poly(c, 15, 5, 8, 12, 15, 19); break;
            case "subtitles":
                c.drawRoundRect(new RectF(3, 5, 21, 19), 3, 3, stroke); c.drawLine(6.5f, 12.5f, 10, 12.5f, stroke);
                c.drawLine(12.5f, 12.5f, 17.5f, 12.5f, stroke); c.drawLine(6.5f, 15.5f, 14, 15.5f, stroke); break;
            case "audio":
                c.drawOval(new RectF(4.7f, 14.7f, 10.3f, 19.3f), stroke); c.drawLine(10.3f, 17, 10.3f, 4.5f, stroke);
                c.drawLine(10.3f, 4.5f, 17, 7, stroke); break;
            case "resize":
                poly(c, 4, 9, 4, 4, 9, 4); poly(c, 15, 4, 20, 4, 20, 9); poly(c, 20, 15, 20, 20, 15, 20); poly(c, 9, 20, 4, 20, 4, 15); break;
            case "search":
                c.drawCircle(10.5f, 10.5f, 6, stroke); c.drawLine(15, 15, 20, 20, stroke); break;
            case "queue":
                c.drawLine(4, 6.5f, 16, 6.5f, stroke); c.drawLine(4, 11.5f, 16, 11.5f, stroke); c.drawLine(4, 16.5f, 11, 16.5f, stroke);
                tri(c, 14, 14, 20.5f, 17.3f, 14, 20.6f); break;
            case "speed":
                c.drawArc(new RectF(3.5f, 4.5f, 20.5f, 21.5f), 180, 180, false, stroke); c.drawLine(12, 13, 16.5f, 8, stroke);
                c.drawCircle(12, 13, 1.8f, fill); break;
            case "refresh":
                c.drawArc(new RectF(4.5f, 4.5f, 19.5f, 19.5f), 40, 290, false, stroke); tri(c, 17, 3, 20.5f, 8.5f, 14.5f, 9); break;
            default:
                c.drawCircle(12, 12, 3, fill);
        }
        c.restore();
    }

    private void tri(Canvas c, float x1, float y1, float x2, float y2, float x3, float y3) {
        Path p = new Path(); p.moveTo(x1, y1); p.lineTo(x2, y2); p.lineTo(x3, y3); p.close(); c.drawPath(p, fill);
    }
    private void poly(Canvas c, float... pts) {
        Path p = new Path(); p.moveTo(pts[0], pts[1]);
        for (int i = 2; i < pts.length; i += 2) p.lineTo(pts[i], pts[i + 1]);
        c.drawPath(p, stroke);
    }

    @Override public void setAlpha(int a) { stroke.setAlpha(a); fill.setAlpha(a); }
    @Override public void setColorFilter(ColorFilter cf) { stroke.setColorFilter(cf); fill.setColorFilter(cf); }
    @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    @Override public int getIntrinsicWidth() { return 96; }
    @Override public int getIntrinsicHeight() { return 96; }
}
