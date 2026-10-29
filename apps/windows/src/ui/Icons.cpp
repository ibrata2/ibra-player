#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

namespace {

void draw(QPainter &p, const QString &n, const QColor &c) {
    // Coordinate system: 24 x 24
    QPen pen(c, 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto fill = [&](const QPainterPath &path) { p.setPen(Qt::NoPen); p.setBrush(c); p.drawPath(path); p.setPen(pen); p.setBrush(Qt::NoBrush); };
    auto tri = [&](QPointF a, QPointF b, QPointF d) {
        QPainterPath path; path.moveTo(a); path.lineTo(b); path.lineTo(d); path.closeSubpath(); return path; };

    if (n == "play") {
        QPainterPath path;
        path.moveTo(8, 5.2); path.cubicTo(8, 4.2, 9, 3.7, 9.9, 4.2);
        path.lineTo(19.2, 10.9); path.cubicTo(20, 11.4, 20, 12.6, 19.2, 13.1);
        path.lineTo(9.9, 19.8); path.cubicTo(9, 20.3, 8, 19.8, 8, 18.8); path.closeSubpath();
        fill(path);
    } else if (n == "pause") {
        QPainterPath path;
        path.addRoundedRect(QRectF(6.5, 5, 3.8, 14), 1.4, 1.4);
        path.addRoundedRect(QRectF(13.7, 5, 3.8, 14), 1.4, 1.4);
        fill(path);
    } else if (n == "stop") {
        QPainterPath path; path.addRoundedRect(QRectF(6.5, 6.5, 11, 11), 2.2, 2.2); fill(path);
    } else if (n == "next" || n == "prev") {
        p.save();
        if (n == "prev") { p.translate(24, 0); p.scale(-1, 1); }
        fill(tri({6, 6}, {15, 12}, {6, 18}));
        QPainterPath bar; bar.addRoundedRect(QRectF(15.8, 6, 2.6, 12), 1, 1); fill(bar);
        p.restore();
    } else if (n == "forward" || n == "rewind") {
        p.save();
        if (n == "rewind") { p.translate(24, 0); p.scale(-1, 1); }
        fill(tri({3.5, 6.5}, {12, 12}, {3.5, 17.5}));
        fill(tri({12, 6.5}, {20.5, 12}, {12, 17.5}));
        p.restore();
    } else if (n == "volume" || n == "volume-low" || n == "mute") {
        QPainterPath sp;
        sp.moveTo(3.5, 9.5); sp.lineTo(7, 9.5); sp.lineTo(11.5, 5.5); sp.lineTo(11.5, 18.5); sp.lineTo(7, 14.5); sp.lineTo(3.5, 14.5); sp.closeSubpath();
        fill(sp);
        if (n == "mute") {
            p.drawLine(QPointF(15, 9.5), QPointF(20, 14.5));
            p.drawLine(QPointF(20, 9.5), QPointF(15, 14.5));
        } else {
            p.drawArc(QRectF(10, 8, 7, 8), -55 * 16, 110 * 16);
            if (n == "volume") p.drawArc(QRectF(10, 4.5, 11, 15), -55 * 16, 110 * 16);
        }
    } else if (n == "fullscreen") {
        p.drawPolyline(QPolygonF({{4, 9}, {4, 4}, {9, 4}}));
        p.drawPolyline(QPolygonF({{15, 4}, {20, 4}, {20, 9}}));
        p.drawPolyline(QPolygonF({{20, 15}, {20, 20}, {15, 20}}));
        p.drawPolyline(QPolygonF({{9, 20}, {4, 20}, {4, 15}}));
    } else if (n == "exit-fullscreen") {
        p.drawPolyline(QPolygonF({{9, 4}, {9, 9}, {4, 9}}));
        p.drawPolyline(QPolygonF({{15, 4}, {15, 9}, {20, 9}}));
        p.drawPolyline(QPolygonF({{20, 15}, {15, 15}, {15, 20}}));
        p.drawPolyline(QPolygonF({{4, 15}, {9, 15}, {9, 20}}));
    } else if (n == "playlist") {
        p.drawLine(QPointF(4, 6.5), QPointF(16, 6.5));
        p.drawLine(QPointF(4, 11.5), QPointF(16, 11.5));
        p.drawLine(QPointF(4, 16.5), QPointF(11, 16.5));
        fill(tri({14, 14}, {20.5, 17.3}, {14, 20.6}));
    } else if (n == "subtitles") {
        p.drawRoundedRect(QRectF(3, 5, 18, 14), 3, 3);
        p.drawLine(QPointF(6.5, 12.5), QPointF(10, 12.5));
        p.drawLine(QPointF(12.5, 12.5), QPointF(17.5, 12.5));
        p.drawLine(QPointF(6.5, 15.5), QPointF(14, 15.5));
    } else if (n == "audio") {
        p.drawEllipse(QPointF(7.5, 17), 2.8, 2.3);
        p.drawEllipse(QPointF(17, 15), 2.8, 2.3);
        p.drawLine(QPointF(10.3, 17), QPointF(10.3, 5.5));
        p.drawLine(QPointF(19.8, 15), QPointF(19.8, 3.8));
        p.drawLine(QPointF(10.3, 5.5), QPointF(19.8, 3.8));
    } else if (n == "settings") {
        QPainterPath gear;
        const QPointF c0(12, 12);
        for (int i = 0; i < 8; ++i) {
            const double a = i * M_PI / 4;
            QPointF d(qCos(a), qSin(a));
            QPointF perp(-d.y(), d.x());
            QPointF base = c0 + d * 6.2, tip = c0 + d * 9.3;
            QPainterPath tooth;
            tooth.moveTo(base + perp * 1.9); tooth.lineTo(tip + perp * 1.4);
            tooth.lineTo(tip - perp * 1.4); tooth.lineTo(base - perp * 1.9); tooth.closeSubpath();
            gear = gear.united(tooth);
        }
        QPainterPath ring; ring.addEllipse(c0, 7, 7);
        gear = gear.united(ring);
        QPainterPath hole; hole.addEllipse(c0, 2.9, 2.9);
        fill(gear.subtracted(hole));
    } else if (n == "speed") {
        p.drawArc(QRectF(3.5, 4.5, 17, 17), 0, 180 * 16);
        p.drawLine(QPointF(12, 13), QPointF(16.5, 8));
        QPainterPath dot; dot.addEllipse(QPointF(12, 13), 1.8, 1.8); fill(dot);
    } else if (n == "open") {
        p.drawPolyline(QPolygonF({{3.5, 18.5}, {3.5, 6}, {9, 6}, {11, 8}, {19, 8}, {19, 11}}));
        p.drawPolygon(QPolygonF({{3.5, 18.5}, {6.5, 11}, {21, 11}, {18, 18.5}}));
    } else if (n == "minimize") {
        p.drawLine(QPointF(6, 12.5), QPointF(18, 12.5));
    } else if (n == "maximize") {
        p.drawRoundedRect(QRectF(6, 6, 12, 12), 2, 2);
    } else if (n == "restore") {
        p.drawRoundedRect(QRectF(5.5, 8.5, 10, 10), 2, 2);
        p.drawPolyline(QPolygonF({{8.5, 5.5}, {18.5, 5.5}, {18.5, 15.5}}));
    } else if (n == "close") {
        p.drawLine(QPointF(7, 7), QPointF(17, 17));
        p.drawLine(QPointF(17, 7), QPointF(7, 17));
    } else if (n == "repeat" || n == "repeat-one") {
        p.drawPolyline(QPolygonF({{5, 11}, {5, 8}, {18, 8}}));
        fill(tri({16, 5}, {20, 8}, {16, 11}));
        p.drawPolyline(QPolygonF({{19, 13}, {19, 16}, {6, 16}}));
        fill(tri({8, 13}, {4, 16}, {8, 19}));
        if (n == "repeat-one") { QFont f; f.setPixelSize(7); f.setBold(true); p.setFont(f); p.drawText(QRectF(9, 8.5, 6, 7), Qt::AlignCenter, "1"); }
    } else if (n == "shuffle") {
        p.drawPolyline(QPolygonF({{4, 7}, {8, 7}, {15, 17}, {18, 17}}));
        p.drawPolyline(QPolygonF({{4, 17}, {8, 17}, {15, 7}, {18, 7}}));
        fill(tri({17, 4}, {21, 7}, {17, 10}));
        fill(tri({17, 14}, {21, 17}, {17, 20}));
    } else if (n == "info") {
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        p.drawLine(QPointF(12, 11), QPointF(12, 16.5));
        QPainterPath dot; dot.addEllipse(QPointF(12, 7.8), 1.2, 1.2); fill(dot);
    } else if (n == "plus") {
        p.drawLine(QPointF(12, 5), QPointF(12, 19)); p.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (n == "minus") {
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (n == "trash") {
        p.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        p.drawPolyline(QPolygonF({{9, 7}, {9.5, 4.5}, {14.5, 4.5}, {15, 7}}));
        p.drawPolygon(QPolygonF({{6.5, 7}, {7.5, 19.5}, {16.5, 19.5}, {17.5, 7}}));
    } else if (n == "save") {
        p.drawRoundedRect(QRectF(4.5, 4.5, 15, 15), 2, 2);
        p.drawRect(QRectF(8, 4.5, 8, 5));
        p.drawRect(QRectF(7.5, 13, 9, 6.5));
    } else if (n == "search") {
        p.drawEllipse(QPointF(10.5, 10.5), 6, 6);
        p.drawLine(QPointF(15, 15), QPointF(20, 20));
    } else if (n == "star" || n == "star-filled") {
        QPainterPath s;
        for (int i = 0; i < 10; ++i) {
            const double a = -M_PI / 2 + i * M_PI / 5;
            const double r = (i % 2) ? 3.9 : 8.8;
            QPointF pt(12 + r * qCos(a), 12.6 + r * qSin(a));
            if (i == 0) s.moveTo(pt); else s.lineTo(pt);
        }
        s.closeSubpath();
        if (n == "star-filled") fill(s); else p.drawPath(s);
    } else if (n == "sort") {
        p.drawLine(QPointF(5, 7), QPointF(19, 7)); p.drawLine(QPointF(5, 12), QPointF(15, 12)); p.drawLine(QPointF(5, 17), QPointF(11, 17));
    } else if (n == "theme") {
        p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
        QPainterPath half; half.moveTo(12, 3.5); half.arcTo(QRectF(3.5, 3.5, 17, 17), 90, 180); half.closeSubpath(); fill(half);
    } else if (n == "folder") {
        p.drawPolygon(QPolygonF({{3.5, 6}, {9, 6}, {11, 8}, {20.5, 8}, {20.5, 18.5}, {3.5, 18.5}}));
    } else if (n == "link") {
        p.drawRoundedRect(QRectF(3, 9, 10, 6), 3, 3);
        p.drawRoundedRect(QRectF(11, 9, 10, 6), 3, 3);
    } else if (n == "ab") {
        QFont f; f.setPixelSize(10); f.setBold(true); p.setFont(f);
        p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, "A-B");
    }
}

} // namespace

QIcon Icons::make(const QString &name, const QColor &color, int size) {
    QIcon icon;
    for (int scale : {1, 2}) {
        QPixmap pm(size * scale, size * scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size * scale / 24.0, size * scale / 24.0);
        draw(p, name, color);
        p.end();
        pm.setDevicePixelRatio(scale);
        icon.addPixmap(pm);
    }
    return icon;
}

QPixmap Icons::logo(int size) {
    QPixmap src(":/brand/logo.png");
    return src.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
