#include "ControlBar.h"
#include "Icons.h"
#include "core/I18n.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>

// ---------------------------------------------------------------- SeekSlider
SeekSlider::SeekSlider(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFixedHeight(22);
    setCursor(Qt::PointingHandCursor);
    setLayoutDirection(Qt::LeftToRight);   // timelines are never mirrored
}

double SeekSlider::posFromX(int x) const {
    const double w = qMax(1, width() - 16);
    return qBound(0.0, (x - 8) / w, 1.0) * m_duration;
}

void SeekSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Theme &t = ThemeManager::instance()->current();
    const double h = (m_hover || m_dragging) ? 6 : 4;
    const QRectF groove(8, height() / 2.0 - h / 2, width() - 16, h);
    QColor track = t.dark ? QColor(255, 255, 255, 45) : QColor(0, 0, 0, 40);
    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(groove, h / 2, h / 2);

    const double frac = m_duration > 0 ? qBound(0.0, m_position / m_duration, 1.0) : 0;
    auto xOf = [&](double sec) { return groove.left() + groove.width() * qBound(0.0, sec / qMax(0.001, m_duration), 1.0); };

    if (m_duration > 0 && m_a >= 0) {   // A-B loop region
        QColor lc = t.accent; lc.setAlpha(70);
        const double xb = m_b >= 0 ? xOf(m_b) : groove.right();
        p.setBrush(lc);
        p.drawRect(QRectF(xOf(m_a), groove.top() - 3, xb - xOf(m_a), groove.height() + 6));
    }

    if (m_hover && m_hoverPos >= 0 && m_duration > 0) {   // hover preview fill
        QColor hc = t.accent; hc.setAlpha(60);
        p.setBrush(hc);
        p.drawRoundedRect(QRectF(groove.left(), groove.top(), xOf(m_hoverPos) - groove.left(), h), h / 2, h / 2);
    }

    QLinearGradient fill(groove.topLeft(), groove.topRight());
    fill.setColorAt(0, t.accent.darker(115));
    fill.setColorAt(1, t.accent.lighter(115));
    p.setBrush(fill);
    p.drawRoundedRect(QRectF(groove.left(), groove.top(), groove.width() * frac, h), h / 2, h / 2);

    p.setBrush(t.dark ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 90));
    for (double c : m_chapters) if (c > 0) p.drawRect(QRectF(xOf(c) - 1, groove.top(), 2, h));

    if (m_duration > 0) {
        const double x = groove.left() + groove.width() * frac;
        const double r = (m_hover || m_dragging) ? 8 : 6;
        QColor glow = t.accent; glow.setAlpha(70);
        p.setBrush(glow);
        p.drawEllipse(QPointF(x, height() / 2.0), r + 4, r + 4);
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(t.accent, 2.5));
        p.drawEllipse(QPointF(x, height() / 2.0), r, r);
    }
}

void SeekSlider::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || m_duration <= 0) return;
    m_dragging = true;
    m_position = posFromX(e->position().x());
    emit seekRequested(m_position, false);
    update();
}

void SeekSlider::mouseMoveEvent(QMouseEvent *e) {
    m_hoverPos = posFromX(e->position().x());
    if (m_duration > 0)
        QToolTip::showText(e->globalPosition().toPoint() - QPoint(20, 46), ControlBar::formatTime(m_hoverPos), this);
    if (m_dragging) {
        m_position = m_hoverPos;
        emit seekRequested(m_position, false);
    }
    update();
}

void SeekSlider::mouseReleaseEvent(QMouseEvent *e) {
    if (!m_dragging) return;
    m_dragging = false;
    m_position = posFromX(e->position().x());
    emit seekRequested(m_position, true);
    update();
}

void SeekSlider::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void SeekSlider::leaveEvent(QEvent *) { m_hover = false; m_hoverPos = -1; QToolTip::hideText(); update(); }

// ---------------------------------------------------------------- ControlBar
ControlBar::ControlBar(QWidget *parent) : GlassPanel(parent) {
    setLayoutDirection(Qt::LeftToRight);     // media transport is never mirrored
    setExtraAlpha(25);                      // a bit more opaque over moving video
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(14, 8, 14, 8);
    v->setSpacing(2);

    auto *seekRow = new QHBoxLayout;
    m_cur = new QLabel("00:00", this);
    m_total = new QLabel("00:00", this);
    m_cur->setObjectName("TimeLabel");
    m_total->setObjectName("TimeLabel");
    m_total->setCursor(Qt::PointingHandCursor);
    m_total->installEventFilter(this);
    m_seek = new SeekSlider(this);
    seekRow->addWidget(m_cur);
    seekRow->addWidget(m_seek, 1);
    seekRow->addWidget(m_total);
    v->addLayout(seekRow);

    auto *row = new QHBoxLayout;
    row->setSpacing(2);
    row->addWidget(makeButton("stop", "stop"));
    row->addWidget(makeButton("previous", "prev"));
    row->addWidget(makeButton("seek_back", "rewind"));
    row->addWidget(makeButton("play_pause", "play", 1));
    row->addWidget(makeButton("seek_fwd", "forward"));
    row->addWidget(makeButton("next", "next"));
    row->addSpacing(10);
    row->addWidget(makeButton("repeat", "repeat"));
    row->addWidget(makeButton("shuffle", "shuffle"));
    row->addStretch(1);
    auto *speed = makeButton("speed", "speed");
    speed->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    speed->setMinimumWidth(70);
    row->addWidget(speed);
    row->addWidget(makeButton("audio", "audio"));
    row->addWidget(makeButton("subtitles", "subtitles"));
    row->addWidget(makeButton("playlist", "playlist"));
    row->addWidget(makeButton("settings", "settings"));
    row->addSpacing(6);
    row->addWidget(makeButton("mute", "volume"));
    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setRange(0, 200);
    m_volume->setValue(100);
    m_volume->setFixedWidth(110);
    m_volume->setFocusPolicy(Qt::NoFocus);
    connect(m_volume, &QSlider::valueChanged, this, &ControlBar::volumeMoved);
    row->addWidget(m_volume);
    row->addSpacing(6);
    row->addWidget(makeButton("fullscreen", "fullscreen"));
    v->addLayout(row);

    m_buttons["repeat"]->setCheckable(true);
    m_buttons["shuffle"]->setCheckable(true);
    m_buttons["playlist"]->setCheckable(true);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &ControlBar::retheme);
    connect(I18n::instance(), &I18n::languageChanged, this, &ControlBar::retranslate);
    retheme();
    retranslate();
}

QToolButton *ControlBar::makeButton(const QString &name, const QString &icon, int big) {
    auto *b = new QToolButton(this);
    b->setObjectName("GlassToolButton");
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    b->setProperty("big", big);
    m_buttons[name] = b;
    m_iconOf[name] = icon;
    connect(b, &QToolButton::clicked, this, [this, name] { emit action(name); });
    return b;
}

void ControlBar::retheme() {
    const Theme &t = ThemeManager::instance()->current();
    const int s = t.buttonSize;
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
        QToolButton *b = it.value();
        const bool big = b->property("big").toInt();
        const int size = big ? s + 10 : s;
        if (it.key() != "speed") b->setFixedSize(size, size);
        else b->setFixedHeight(size);
        b->setIconSize(QSize(size * 0.55, size * 0.55));
        if (big) {
            b->setIcon(Icons::make(m_iconOf[it.key()], t.accentText, 28));
            b->setStyleSheet(QString("QToolButton{background:%1;border-radius:%2px;border:1px solid %3;}"
                                     "QToolButton:hover{background:%4;}")
                                 .arg(t.accent.name()).arg(size / 2).arg(rgba(t.accent.lighter(130)))
                                 .arg(t.accent.lighter(112).name()));
        } else {
            b->setIcon(Icons::make(m_iconOf[it.key()], t.text, 24));
        }
    }
    update();
}

void ControlBar::retranslate() {
    static const QHash<QString, const char *> tips = {
        {"stop", "act.stop"}, {"previous", "act.previous"}, {"seek_back", "act.seekBack"},
        {"play_pause", "act.playPause"}, {"seek_fwd", "act.seekFwd"}, {"next", "act.next"},
        {"repeat", "act.repeatMode"}, {"shuffle", "act.shuffle"}, {"speed", "menu.speed"},
        {"audio", "menu.audioTrack"}, {"subtitles", "menu.subtitleTrack"}, {"playlist", "act.playlist"},
        {"settings", "act.settings"}, {"mute", "act.mute"}, {"fullscreen", "act.fullscreen"}};
    for (auto it = tips.begin(); it != tips.end(); ++it)
        if (m_buttons.contains(it.key())) m_buttons[it.key()]->setToolTip(T(it.value()));
    m_total->setToolTip(T("tip.toggleRemaining"));
    m_volume->setToolTip(T("act.volume"));
}

QString ControlBar::formatTime(double sec, bool forceHours) {
    if (sec < 0 || qIsNaN(sec)) sec = 0;
    const qint64 s = qint64(sec);
    const int h = s / 3600, m = (s % 3600) / 60, ss = s % 60;
    if (h > 0 || forceHours) return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(ss, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(ss, 2, 10, QChar('0'));
}

void ControlBar::setTimes(double pos, double dur) {
    m_lastPos = pos;
    m_lastDur = dur;
    const bool hours = dur >= 3600;
    m_cur->setText(formatTime(pos, hours));
    m_total->setText(m_showRemaining && dur > 0 ? "-" + formatTime(dur - pos, hours) : formatTime(dur, hours));
    m_seek->setDuration(dur);
    m_seek->setPosition(pos);
}

void ControlBar::setPlaying(bool playing) {
    m_iconOf["play_pause"] = playing ? "pause" : "play";
    m_buttons["play_pause"]->setIcon(Icons::make(m_iconOf["play_pause"], ThemeManager::instance()->current().accentText, 28));
}

void ControlBar::setMuted(bool muted, int volume) {
    m_iconOf["mute"] = muted ? "mute" : (volume < 50 ? "volume-low" : "volume");
    m_buttons["mute"]->setIcon(Icons::make(m_iconOf["mute"], ThemeManager::instance()->current().text, 24));
    if (m_volume->value() != volume) {
        QSignalBlocker b(m_volume);
        m_volume->setValue(volume);
    }
}

void ControlBar::setFullscreen(bool fs) {
    m_iconOf["fullscreen"] = fs ? "exit-fullscreen" : "fullscreen";
    m_buttons["fullscreen"]->setIcon(Icons::make(m_iconOf["fullscreen"], ThemeManager::instance()->current().text, 24));
}

void ControlBar::setSpeed(double s) {
    m_buttons["speed"]->setText(ltr(QString::number(s, 'g', 3) + "×"));
}

void ControlBar::setRepeatShuffle(int repeatMode, bool shuffle) {
    m_iconOf["repeat"] = repeatMode == 1 ? "repeat-one" : "repeat";
    m_buttons["repeat"]->setIcon(Icons::make(m_iconOf["repeat"], ThemeManager::instance()->current().text, 24));
    m_buttons["repeat"]->setChecked(repeatMode != 0);
    m_buttons["shuffle"]->setChecked(shuffle);
}

bool ControlBar::eventFilter(QObject *o, QEvent *e) {
    if (o == m_total && e->type() == QEvent::MouseButtonPress) {
        m_showRemaining = !m_showRemaining;
        setTimes(m_lastPos, m_lastDur);
        return true;
    }
    return GlassPanel::eventFilter(o, e);
}
