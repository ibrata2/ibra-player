#pragma once
#include <QWidget>
#include "theme/Theme.h"

namespace Glass {
// Frosted backdrop: gradient + soft light blobs (cheap, no per-frame blur).
void paintBackdrop(QPainter &p, const QRectF &r, const Theme &t, bool translucentWindow);
// Glass surface: shadow, tint, top reflection, luminous edge, optional glow.
void paintPanel(QPainter &p, const QRectF &r, const Theme &t, int radius = -1, int extraAlpha = 0, bool glow = true);
}

class GlassPanel : public QWidget {
    Q_OBJECT
public:
    explicit GlassPanel(QWidget *parent = nullptr);
    void setExtraAlpha(int a) { m_extra = a; update(); }
    void setGlow(bool g) { m_glow = g; update(); }
    void setMarginForShadow(int m) { m_shadowMargin = m; update(); }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int m_extra = 0;
    int m_shadowMargin = 0;
    bool m_glow = true;
};
