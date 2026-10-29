#include "GlassPanel.h"
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QLinearGradient>

void Glass::paintBackdrop(QPainter &p, const QRectF &r, const Theme &t, bool translucent) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    QColor top = t.bgTop, bottom = t.bgBottom;
    if (translucent) { top.setAlpha(150); bottom.setAlpha(190); }   // let the OS acrylic show through
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    p.fillRect(r, g);

    // Light blobs: radial gradients are naturally soft, so "blur" maps to spread.
    const double spread = 0.35 + t.blur / 100.0 * 0.45;
    const double w = r.width(), h = r.height();
    auto blob = [&](QPointF c, double rad, QColor col) {
        QRadialGradient rg(c, rad);
        rg.setColorAt(0, col);
        QColor mid = col; mid.setAlpha(col.alpha() / 3);
        rg.setColorAt(0.45, mid);
        QColor end = col; end.setAlpha(0);
        rg.setColorAt(1, end);
        p.fillRect(r, rg);
    };
    const double m = qMax(w, h);
    blob(r.topLeft() + QPointF(w * 0.12, h * 0.05), m * spread, t.blobA);
    blob(r.topLeft() + QPointF(w * 0.92, h * 0.95), m * spread * 1.1, t.blobB);
    QColor faint = t.blobA; faint.setAlpha(t.blobA.alpha() / 2);
    blob(r.topLeft() + QPointF(w * 0.75, h * 0.15), m * spread * 0.6, faint);
    p.restore();
}

void Glass::paintPanel(QPainter &p, const QRectF &rIn, const Theme &t, int radius, int extraAlpha, bool glow) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double rad = radius < 0 ? t.radius : radius;
    QRectF r = rIn;

    // Soft drop shadow (stacked translucent outlines; cheap and smooth)
    if (t.shadow > 0) {
        const int steps = qMin(t.shadow, 14);
        for (int i = steps; i > 0; --i) {
            const double grow = i * t.shadow / double(steps) * 0.55;
            QColor sc(0, 0, 0, int(70.0 / steps * (1.0 - double(i) / (steps + 1)) * (t.dark ? 1.4 : 0.8)));
            p.setPen(Qt::NoPen);
            p.setBrush(sc);
            p.drawRoundedRect(r.adjusted(-grow, -grow + 2, grow, grow + 3), rad + grow, rad + grow);
        }
    }

    // Accent glow
    if (glow && t.glow_ > 0) {
        const int steps = qMin(t.glow_, 10);
        for (int i = steps; i > 0; --i) {
            const double grow = i * t.glow_ / double(steps) * 0.5;
            QColor gc = t.glow;
            gc.setAlpha(int(55.0 / steps * (1.0 - double(i) / (steps + 1))));
            p.setPen(QPen(gc, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(-grow, -grow, grow, grow), rad + grow, rad + grow);
        }
    }

    QPainterPath shape;
    shape.addRoundedRect(r, rad, rad);

    // Tinted glass body
    p.setPen(Qt::NoPen);
    p.fillPath(shape, t.surfaceColor(extraAlpha));

    // Top reflection (specular sheen)
    if (t.reflection > 0) {
        QLinearGradient refl(r.topLeft(), QPointF(r.left(), r.top() + r.height() * 0.6));
        refl.setColorAt(0, QColor(255, 255, 255, t.reflection * 120 / 100));
        refl.setColorAt(0.5, QColor(255, 255, 255, t.reflection * 25 / 100));
        refl.setColorAt(1, QColor(255, 255, 255, 0));
        p.fillPath(shape, refl);
    }

    // Luminous edge: brighter at top, fading down
    QLinearGradient edge(r.topLeft(), r.bottomLeft());
    edge.setColorAt(0, t.borderColor(1.3));
    edge.setColorAt(0.5, t.borderColor(0.55));
    edge.setColorAt(1, t.borderColor(0.9));
    p.setPen(QPen(QBrush(edge), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
    p.restore();
}

GlassPanel::GlassPanel(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
}

void GlassPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const int m = m_shadowMargin;
    Glass::paintPanel(p, QRectF(rect()).adjusted(m, m, -m, -m), ThemeManager::instance()->current(), -1, m_extra, m_glow);
}
