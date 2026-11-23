#include "StoreSlides.h"
#include "core/AppSettings.h"
#include "core/I18n.h"
#include "theme/Theme.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDesktopServices>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QTextLayout>
#include <QUrl>
#include <algorithm>

// ---------------------------------------------------------------- settings you may change
// The store the slides come from.
static const char *const kStoreBase = "https://ibraprogram.netlify.app";
// ACCENT: leave empty to use the current theme's accent colour, or set e.g. "#ff1f35".
static const char *const ACCENT = "";
static constexpr int kIntervalMs = 4000;   // auto-advance
static constexpr int kMaxItems = 10;       // newest N apps
static constexpr int kRetryMs = 10 * 60 * 1000;

static QString storeBase() {
    const QString env = qEnvironmentVariable("IBRA_STORE_BASE");   // for testing against a local copy
    return env.isEmpty() ? QString::fromLatin1(kStoreBase) : env;
}

StoreSlides::StoreSlides(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
    hide();
    m_net = new QNetworkAccessManager(this);

    m_timer.setInterval(kIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, [this] { step(+1); });

    m_anim.setDuration(420);
    m_anim.setStartValue(0.0);
    m_anim.setEndValue(1.0);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this] { update(); });
    connect(&m_anim, &QVariantAnimation::finished, this, [this] { m_prev = -1; update(); });

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { update(); });
    connect(I18n::instance(), &I18n::languageChanged, this, [this] { update(); });
    connect(AppSettings::instance(), &AppSettings::changed, this, [this](const QString &k) {
        if (k == "ui/storeSlides") emit contentChanged();
    });
    fetch();
}

bool StoreSlides::hasContent() const {
    return !m_items.isEmpty() && AppSettings::instance()->boolValue("ui/storeSlides");
}

int StoreSlides::heightForWidth(int w) const {
    return 30 + qBound(170, int(w * 0.30), 230) + 24;
}

QColor StoreSlides::accent() const {
    const QColor c(QString::fromLatin1(ACCENT));
    return c.isValid() ? c : ThemeManager::instance()->current().accent;
}

QString StoreSlides::appUrl(const Item &it) const {
    return storeBase() + "/app/" + QString::fromUtf8(QUrl::toPercentEncoding(it.slug));
}

// ---------------------------------------------------------------- data
void StoreSlides::fetch() {
    QNetworkRequest req(QUrl(storeBase() + "/api/apps"));
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(15000);
    QNetworkReply *r = m_net->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        const QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
        if (r->error() != QNetworkReply::NoError || !doc.isArray()) {
            if (m_items.isEmpty()) QTimer::singleShot(kRetryMs, this, &StoreSlides::fetch);
            return;   // stay hidden, quietly
        }
        const QUrl base(storeBase() + "/");
        auto abs = [&](const QString &u) { return u.isEmpty() ? u : base.resolved(QUrl(u)).toString(); };
        QList<Item> items;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            Item it;
            it.name = o["name"].toString().trimmed();
            it.slug = o["slug"].toString().trimmed();
            if (it.name.isEmpty() || it.slug.isEmpty()) continue;
            it.tagline = o["tagline"].toString().trimmed();
            const QJsonArray cats = o["categories"].toArray();
            it.category = cats.isEmpty() ? o["category"].toString() : cats.first().toString();
            it.createdAt = o["createdAt"].toString();
            it.iconUrl = abs(o["icon"].toString());
            const QJsonArray shots = o["screenshots"].toArray();
            it.bgUrl = shots.isEmpty() ? it.iconUrl : abs(shots.first().toString());
            items << it;
        }
        std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.createdAt > b.createdAt; });
        if (items.size() > kMaxItems) items = items.mid(0, kMaxItems);
        m_items = items;
        m_cur = 0;
        m_prev = -1;
        for (int i = 0; i < qMin(2, m_items.size()); ++i) {
            loadImage(m_items[i].bgUrl);
            loadImage(m_items[i].iconUrl);
        }
        if (m_items.size() > 1 && !m_hover) m_timer.start();
        emit contentChanged();
        update();
    });
}

void StoreSlides::loadImage(const QString &url) {
    if (url.isEmpty() || m_images.contains(url)) return;
    m_images.insert(url, QPixmap());
    if (url.startsWith("data:")) {
        const int comma = url.indexOf(',');
        QPixmap px;
        px.loadFromData(QByteArray::fromBase64(url.mid(comma + 1).toLatin1()));
        m_images[url] = px;
        return;
    }
    QNetworkRequest req{QUrl(url)};
    req.setTransferTimeout(20000);
    QNetworkReply *r = m_net->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, url] {
        r->deleteLater();
        QPixmap px;
        if (r->error() == QNetworkReply::NoError) px.loadFromData(r->readAll());
        m_images[url] = px;
        update();
    });
}

void StoreSlides::go(int index, int dir) {
    if (m_items.isEmpty() || index == m_cur) return;
    m_prev = m_cur;
    m_cur = index;
    m_dir = dir;
    // preload this slide and the next one
    for (int k : {m_cur, int((m_cur + 1) % m_items.size())}) {
        loadImage(m_items[k].bgUrl);
        loadImage(m_items[k].iconUrl);
    }
    m_anim.stop();
    m_anim.start();
    if (m_timer.isActive()) m_timer.start();   // restart the 4 s countdown
}

// ---------------------------------------------------------------- geometry
QRectF StoreSlides::cardRect() const {
    return QRectF(0.5, 30.5, width() - 1.0, height() - 30 - 24 - 1.0);
}

QRectF StoreSlides::arrowRect(int dir) const {
    const QRectF c = cardRect();
    const qreal s = 34;
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    const bool right = (dir > 0) != rtl;
    const qreal x = right ? c.right() - s - 10 : c.left() + 10;
    return QRectF(x, c.center().y() - s / 2, s, s);
}

QRectF StoreSlides::dotRect(int i) const {
    const int n = m_items.size();
    const qreal dot = 8, gap = 8, wide = 22;
    const qreal total = (n - 1) * (dot + gap) + wide;
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    qreal x = width() / 2.0 - total / 2;
    const qreal y = height() - 15;
    for (int k = 0; k < n; ++k) {
        const int idx = rtl ? n - 1 - k : k;
        const qreal w = idx == m_cur ? wide : dot;
        if (idx == i) return QRectF(x, y, w, dot);
        x += w + gap;
    }
    return {};
}

QRectF StoreSlides::buttonRect(const QRectF &card) const {
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 0.95);
    f.setBold(true);
    const QString txt = I18n::instance()->isRtl() ? QStringLiteral("عرض البرنامج ←") : QStringLiteral("View app →");
    const qreal w = QFontMetricsF(f).horizontalAdvance(txt) + 34, h = 34;
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    const qreal pad = 58;   // leave room for the arrows
    const qreal x = rtl ? card.right() - pad - w : card.left() + pad;
    return QRectF(x, card.bottom() - h - 18, w, h);
}

// ---------------------------------------------------------------- painting
static void drawCover(QPainter &p, const QRectF &r, const QPixmap &px) {
    const QSizeF s = px.size().scaled(r.size().toSize(), Qt::KeepAspectRatioByExpanding);
    const QRectF target(r.center().x() - s.width() / 2, r.center().y() - s.height() / 2, s.width(), s.height());
    p.drawPixmap(target, px, px.rect());
}

// Wraps text into at most maxLines lines, adding "…" to the last one if it does not fit.
static QStringList wrapLines(const QString &text, const QFont &f, qreal width, int maxLines) {
    QStringList out;
    QTextLayout tl(text, f);
    tl.beginLayout();
    int lastStart = 0;
    while (out.size() < maxLines) {
        QTextLine line = tl.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(width);
        lastStart = line.textStart();
        out << text.mid(line.textStart(), line.textLength()).trimmed();
    }
    tl.endLayout();
    if (!out.isEmpty() && lastStart + out.last().size() < text.trimmed().size()) {
        const QFontMetricsF fm(f);
        out.last() = fm.elidedText(text.mid(lastStart).trimmed(), Qt::ElideRight, width);
    }
    return out;
}

void StoreSlides::paintSlide(QPainter &p, const QRectF &r, const Item &it, qreal opacity) {
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    const Theme &t = ThemeManager::instance()->current();
    const QColor acc = accent();
    p.save();
    p.setOpacity(opacity);

    // background picture, darkened towards the text side
    const QPixmap bg = m_images.value(it.bgUrl);
    p.fillRect(r, t.videoBackground.lighter(130));
    if (!bg.isNull()) {
        // soft, blurred copy behind the text so the slide stays readable on any picture
        QPixmap &soft = m_soft[it.bgUrl];
        if (soft.isNull()) soft = bg.scaledToWidth(qMax(24, bg.width() / 14), Qt::SmoothTransformation);
        drawCover(p, r, soft);
    }
    QLinearGradient shade(rtl ? r.topRight() : r.topLeft(), rtl ? r.topLeft() : r.topRight());
    shade.setColorAt(0.0, QColor(0, 0, 0, 200));
    shade.setColorAt(0.6, QColor(0, 0, 0, 150));
    shade.setColorAt(1.0, QColor(0, 0, 0, 90));
    p.fillRect(r, shade);
    QColor tint = acc; tint.setAlpha(38);
    p.fillRect(r, tint);

    // icon
    const qreal pad = 58;
    const qreal is = qBound<qreal>(56, r.height() - 76, 104);
    const QRectF ir(rtl ? r.right() - pad - is : r.left() + pad, r.top() + 22, is, is);
    QPainterPath ip; ip.addRoundedRect(ir, is * 0.22, is * 0.22);
    const QPixmap icon = m_images.value(it.iconUrl);
    p.save();
    p.setClipPath(ip, Qt::IntersectClip);
    if (!icon.isNull()) {
        p.fillRect(ir, QColor(255, 255, 255, 18));
        drawCover(p, ir, icon);
    } else {
        p.fillRect(ir, acc);
        QFont lf = font(); lf.setBold(true); lf.setPointSizeF(is * 0.32);
        p.setFont(lf); p.setPen(t.accentText);
        p.drawText(ir, Qt::AlignCenter, it.name.left(1).toUpper());
    }
    p.restore();
    p.setPen(QPen(QColor(255, 255, 255, 50), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ip);

    // text column
    const qreal gap = 18;
    const qreal tx = rtl ? r.left() + pad : ir.right() + gap;
    const qreal tw = (rtl ? ir.left() - gap : r.right() - pad) - tx;
    const Qt::Alignment al = Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
    qreal y = r.top() + 20;

    QFont base = font();
    const qreal bp = base.pointSizeF() > 0 ? base.pointSizeF() : 10;
    if (!it.category.isEmpty()) {
        QFont cf = base; cf.setPointSizeF(bp * 0.85); cf.setBold(true);
        const QFontMetricsF fm(cf);
        const QString cat = fm.elidedText(it.category, Qt::ElideRight, tw - 24);
        const qreal cw = fm.horizontalAdvance(cat) + 22, ch = fm.height() + 6;
        const QRectF cr(rtl ? tx + tw - cw : tx, y, cw, ch);
        p.setPen(Qt::NoPen);
        p.setBrush(acc);
        p.drawRoundedRect(cr, ch / 2, ch / 2);
        p.setFont(cf); p.setPen(t.accentText);
        p.drawText(cr, Qt::AlignCenter, cat);
        y += ch + 6;
    }
    QFont nf = base; nf.setPointSizeF(bp * 1.75); nf.setBold(true);
    p.setFont(nf); p.setPen(Qt::white);
    const QFontMetricsF nfm(nf);
    p.drawText(QRectF(tx, y, tw, nfm.height()), al, nfm.elidedText(it.name, Qt::ElideRight, tw));
    y += nfm.height() + 2;

    const QRectF btn = buttonRect(r);
    if (!it.tagline.isEmpty()) {
        QFont gf = base; gf.setPointSizeF(bp * 1.0);
        const QFontMetricsF gfm(gf);
        const int maxLines = qMax(1, int((btn.top() - 8 - y) / gfm.lineSpacing()));
        p.setFont(gf); p.setPen(QColor(255, 255, 255, 205));
        for (const QString &line : wrapLines(it.tagline, gf, tw, qMin(2, maxLines))) {
            p.drawText(QRectF(tx, y, tw, gfm.lineSpacing()), al, line);
            y += gfm.lineSpacing();
        }
    }

    // "View app" button
    QFont bf = base; bf.setPointSizeF(bp * 0.95); bf.setBold(true);
    const bool hot = m_hot == "btn";
    p.setPen(Qt::NoPen);
    p.setBrush(hot ? acc.lighter(115) : acc);
    p.drawRoundedRect(btn, btn.height() / 2, btn.height() / 2);
    p.setFont(bf); p.setPen(t.accentText);
    p.drawText(btn, Qt::AlignCenter, I18n::instance()->isRtl() ? QStringLiteral("عرض البرنامج ←") : QStringLiteral("View app →"));
    p.restore();
}

void StoreSlides::paintEvent(QPaintEvent *) {
    if (m_items.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    const QColor acc = accent();

    // header
    QFont hf = font(); hf.setBold(true);
    p.setFont(hf);
    p.setPen(QColor(255, 255, 255, 215));
    const QString head = I18n::instance()->isRtl() ? QStringLiteral("أحدث البرامج والألعاب") : QStringLiteral("Latest programs & games");
    QRectF hr(4, 0, width() - 8, 24);
    p.drawText(hr, Qt::AlignAbsolute | (rtl ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, head);
    QFont sf = font(); sf.setPointSizeF(sf.pointSizeF() * 0.85); sf.setBold(true);
    p.setFont(sf); p.setPen(acc);
    p.drawText(hr, Qt::AlignAbsolute | (rtl ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, QStringLiteral("IBRA STORE"));

    // card
    const QRectF c = cardRect();
    QPainterPath clip; clip.addRoundedRect(c, 18, 18);
    p.save();
    p.setClipPath(clip);
    const qreal k = m_anim.state() == QAbstractAnimation::Running ? m_anim.currentValue().toReal() : 1.0;
    const qreal sign = (m_dir > 0) != rtl ? 1 : -1;   // "next" slides in from the reading-forward side
    if (m_prev >= 0 && k < 1.0) {
        paintSlide(p, c.translated(-sign * c.width() * 0.25 * k, 0), m_items[m_prev], 1.0 - k);
        paintSlide(p, c.translated(sign * c.width() * 0.25 * (1 - k), 0), m_items[m_cur], k);
    } else {
        paintSlide(p, c, m_items[m_cur], 1.0);
    }
    p.restore();
    p.setPen(QPen(QColor(255, 255, 255, m_hover ? 70 : 40), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(clip);

    if (m_items.size() > 1) {
        // arrows
        for (int dir : {-1, +1}) {
            const QRectF ar = arrowRect(dir);
            const bool hot = m_hot == (dir < 0 ? "prev" : "next");
            p.setPen(QPen(QColor(255, 255, 255, 60), 1));
            p.setBrush(hot ? acc : QColor(0, 0, 0, 120));
            p.drawEllipse(ar);
            const bool pointRight = ar.center().x() > c.center().x();
            const qreal cx = ar.center().x() + (pointRight ? 1.5 : -1.5), cy = ar.center().y(), d = 5;
            QPainterPath chev;
            chev.moveTo(cx + (pointRight ? -d : d), cy - d * 1.5);
            chev.lineTo(cx + (pointRight ? d : -d), cy);
            chev.lineTo(cx + (pointRight ? -d : d), cy + d * 1.5);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Qt::white, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(chev);
        }
        // dots
        p.setPen(Qt::NoPen);
        for (int i = 0; i < m_items.size(); ++i) {
            const QRectF d = dotRect(i);
            p.setBrush(i == m_cur ? acc : QColor(255, 255, 255, m_hot == QString("dot%1").arg(i) ? 170 : 85));
            p.drawRoundedRect(d, 4, 4);
        }
    }
}

// ---------------------------------------------------------------- interaction
void StoreSlides::mouseMoveEvent(QMouseEvent *e) {
    const QPointF pos = e->position();
    QString hot;
    if (m_items.size() > 1 && arrowRect(-1).adjusted(-4, -4, 4, 4).contains(pos)) hot = "prev";
    else if (m_items.size() > 1 && arrowRect(+1).adjusted(-4, -4, 4, 4).contains(pos)) hot = "next";
    else if (buttonRect(cardRect()).contains(pos)) hot = "btn";
    else if (cardRect().contains(pos)) hot = "card";
    else for (int i = 0; i < m_items.size(); ++i)
        if (dotRect(i).adjusted(-4, -6, 4, 6).contains(pos)) { hot = QString("dot%1").arg(i); break; }
    setCursor(hot.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    if (hot != m_hot) { m_hot = hot; update(); }
}

void StoreSlides::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || m_items.isEmpty()) return;
    mouseMoveEvent(e);
    if (m_hot == "prev") step(-1);
    else if (m_hot == "next") step(+1);
    else if (m_hot == "btn" || m_hot == "card") QDesktopServices::openUrl(QUrl(appUrl(m_items[m_cur])));
    else if (m_hot.startsWith("dot")) {
        const int i = m_hot.mid(3).toInt();
        go(i, i > m_cur ? +1 : -1);
    }
    e->accept();
}

void StoreSlides::enterEvent(QEnterEvent *) {
    m_hover = true;
    m_timer.stop();
    update();
}

void StoreSlides::leaveEvent(QEvent *) {
    m_hover = false;
    m_hot.clear();
    if (m_items.size() > 1) m_timer.start();
    update();
}
