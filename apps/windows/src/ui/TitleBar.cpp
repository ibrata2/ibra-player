#include "TitleBar.h"
#include "Icons.h"
#include "theme/Theme.h"
#include "core/I18n.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenuBar>
#include <QMouseEvent>
#include <QWindow>
#include <QPainter>

TitleBar::TitleBar(QWidget *window, QWidget *parent) : QWidget(parent), m_window(window) {
    setFixedHeight(40);
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(12, 0, 6, 0);
    l->setSpacing(6);

    m_logo = new QLabel(this);
    m_logo->setPixmap(Icons::logo(44));
    m_logo->setFixedSize(24, 24);
    m_logo->setScaledContents(true);
    m_title = new QLabel("iBRA PLAYER", this);
    m_title->setObjectName("TitleText");
    m_menu = new QMenuBar(this);
    m_menu->setNativeMenuBar(false);
    m_media = new QLabel(this);
    m_media->setProperty("muted", true);
    m_media->setTextInteractionFlags(Qt::NoTextInteraction);
    m_media->setMinimumWidth(0);
    m_media->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_media->setAlignment(Qt::AlignCenter);

    auto mk = [this](const char *tip) {
        auto *b = new QToolButton(this);
        b->setObjectName("GlassToolButton");
        b->setFixedSize(40, 30);
        b->setIconSize(QSize(16, 16));
        b->setFocusPolicy(Qt::NoFocus);
        b->setProperty("tipKey", tip);
        return b;
    };
    m_min = mk("win.minimize");
    m_max = mk("win.maximize");
    m_close = mk("win.close");
    connect(m_min, &QToolButton::clicked, m_window, &QWidget::showMinimized);
    connect(m_max, &QToolButton::clicked, this, [this] {
        m_window->isMaximized() ? m_window->showNormal() : m_window->showMaximized();
    });
    connect(m_close, &QToolButton::clicked, m_window, &QWidget::close);

    l->addWidget(m_logo);
    l->addWidget(m_title);
    l->addSpacing(8);
    l->addWidget(m_menu);
    l->addWidget(m_media, 1);
    l->addWidget(m_min);
    l->addWidget(m_max);
    l->addWidget(m_close);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &TitleBar::updateIcons);
    connect(I18n::instance(), &I18n::languageChanged, this, &TitleBar::updateIcons);
    updateIcons();
}

void TitleBar::setMediaTitle(const QString &t) {
    m_media->setText(ltr(fontMetrics().elidedText(t, Qt::ElideMiddle, 600)));
    m_media->setToolTip(t);
}

void TitleBar::updateIcons() {
    const QColor c = ThemeManager::instance()->current().text;
    m_min->setIcon(Icons::make("minimize", c, 16));
    m_close->setIcon(Icons::make("close", c, 16));
    m_min->setToolTip(T("win.minimize"));
    m_close->setToolTip(T("win.close"));
    updateMaximized();
    // The red hover on close is a Windows convention users expect
    m_close->setStyleSheet("QToolButton:hover{background:#e81123;border-radius:6px;}");
}

void TitleBar::updateMaximized() {
    const QColor c = ThemeManager::instance()->current().text;
    const bool max = m_window->isMaximized();
    m_max->setIcon(Icons::make(max ? "restore" : "maximize", c, 16));
    m_max->setToolTip(max ? T("win.restore") : T("win.maximize"));
}

void TitleBar::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton && m_window->windowHandle()) {
        m_window->windowHandle()->startSystemMove();
        e->accept();
    }
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() == Qt::LeftButton) m_max->click();
}

void TitleBar::paintEvent(QPaintEvent *) {
    // Subtle separator line under the title bar
    QPainter p(this);
    const Theme &t = ThemeManager::instance()->current();
    p.setPen(t.borderColor(0.6));
    p.drawLine(12, height() - 1, width() - 12, height() - 1);
}
