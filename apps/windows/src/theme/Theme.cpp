#include "Theme.h"
#include "core/AppSettings.h"
#include <QApplication>
#include <QSettings>
#include <QStyleHints>
#include <QGuiApplication>
#include <QPalette>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <QStyle>

QString rgba(const QColor &c, int a) {
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a < 0 ? c.alpha() : a);
}

QColor Theme::surfaceColor(int extra) const {
    QColor c = surface;
    c.setAlpha(qBound(0, surfaceOpacity * 255 / 100 + extra, 255));
    return c;
}

QColor Theme::borderColor(double f) const {
    QColor c = border;
    c.setAlpha(qBound(0, int(borderOpacity * 255 / 100 * f), 255));
    return c;
}

static QString col(const QColor &c) { return c.name(QColor::HexArgb); }

QJsonObject Theme::toJson() const {
    QJsonObject o;
    o["format"] = "ibra-player-theme";
    o["version"] = 1;
    o["id"] = id; o["nameEn"] = nameEn; o["nameAr"] = nameAr; o["dark"] = dark;
    o["bgTop"] = col(bgTop); o["bgBottom"] = col(bgBottom);
    o["blobA"] = col(blobA); o["blobB"] = col(blobB);
    o["surface"] = col(surface); o["surfaceOpacity"] = surfaceOpacity;
    o["text"] = col(text); o["textMuted"] = col(textMuted);
    o["accent"] = col(accent); o["accentText"] = col(accentText);
    o["border"] = col(border); o["borderOpacity"] = borderOpacity;
    o["glow"] = col(glow); o["glowStrength"] = glow_;
    o["shadow"] = shadow; o["radius"] = radius; o["buttonSize"] = buttonSize;
    o["blur"] = blur; o["reflection"] = reflection;
    o["videoBackground"] = col(videoBackground);
    return o;
}

Theme Theme::fromJson(const QJsonObject &o, bool *ok) {
    Theme t;
    auto c = [&](const char *k, const QColor &def) {
        const QColor v(o.value(k).toString());
        return v.isValid() ? v : def;
    };
    auto n = [&](const char *k, int def, int lo, int hi) { return qBound(lo, o.value(k).toInt(def), hi); };
    const bool valid = o.value("format").toString() == "ibra-player-theme";
    if (ok) *ok = valid;
    t.id = o.value("id").toString();
    t.nameEn = o.value("nameEn").toString("Custom Glass").left(60);
    t.nameAr = o.value("nameAr").toString().left(60);
    t.dark = o.value("dark").toBool(true);
    t.bgTop = c("bgTop", QColor("#141418")); t.bgBottom = c("bgBottom", QColor("#050507"));
    t.blobA = c("blobA", QColor("#40ffffff")); t.blobB = c("blobB", QColor("#30ff2040"));
    t.surface = c("surface", QColor("#202024")); t.surfaceOpacity = n("surfaceOpacity", 60, 0, 100);
    t.text = c("text", Qt::white); t.textMuted = c("textMuted", QColor("#a0a0a8"));
    t.accent = c("accent", QColor("#e3192b")); t.accentText = c("accentText", Qt::white);
    t.border = c("border", Qt::white); t.borderOpacity = n("borderOpacity", 40, 0, 100);
    t.glow = c("glow", QColor("#e3192b")); t.glow_ = n("glowStrength", 0, 0, 40);
    t.shadow = n("shadow", 18, 0, 40); t.radius = n("radius", 14, 0, 30);
    t.buttonSize = n("buttonSize", 38, 26, 64); t.blur = n("blur", 60, 0, 100);
    t.reflection = n("reflection", 35, 0, 100);
    t.videoBackground = c("videoBackground", Qt::black);
    return t;
}

// ---------------------------------------------------------------------------

ThemeManager *ThemeManager::instance() {
    static ThemeManager *m = new ThemeManager();
    return m;
}

ThemeManager::ThemeManager() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Follow Windows light/dark changes live when the automatic theme is chosen
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_current.id == "system") activate(find("system"));
    });
#endif
}

static bool systemIsDark() {
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    if (reg.contains("AppsUseLightTheme")) return reg.value("AppsUseLightTheme").toInt() == 0;
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

static QColor systemAccent() {
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM", QSettings::NativeFormat);
    if (reg.contains("AccentColor")) {
        const quint32 abgr = reg.value("AccentColor").toUInt();
        QColor c(abgr & 0xff, (abgr >> 8) & 0xff, (abgr >> 16) & 0xff);
        if (c.isValid()) return c;
    }
#endif
    return QColor("#e3192b");
}

QVector<Theme> ThemeManager::builtIns() const {
    QVector<Theme> v;
    {   // Automatic: follows the Windows light/dark mode and accent colour
        const bool dark = systemIsDark();
        const QColor acc = systemAccent();
        Theme t; t.id = "system"; t.nameEn = "Automatic (System)"; t.nameAr = "تلقائي (حسب النظام)"; t.builtIn = true; t.dark = dark;
        if (dark) {
            t.bgTop = QColor("#202124"); t.bgBottom = QColor("#0b0b0d");
            t.blobA = QColor(255, 255, 255, 30); t.blobB = QColor(acc.red(), acc.green(), acc.blue(), 60);
            t.surface = QColor("#1c1c1f"); t.surfaceOpacity = 66;
            t.text = QColor("#f3f3f3"); t.textMuted = QColor("#a0a0a6");
            t.border = QColor("#ffffff"); t.borderOpacity = 24; t.reflection = 30;
        } else {
            t.bgTop = QColor("#f5f6f8"); t.bgBottom = QColor("#e1e4ea");
            t.blobA = QColor(255, 255, 255, 220); t.blobB = QColor(acc.red(), acc.green(), acc.blue(), 50);
            t.surface = QColor("#ffffff"); t.surfaceOpacity = 60;
            t.text = QColor("#1b1b1f"); t.textMuted = QColor("#5f6370");
            t.border = QColor("#ffffff"); t.borderOpacity = 85; t.reflection = 55; t.shadow = 16;
        }
        t.accent = acc; t.accentText = acc.lightness() > 170 ? QColor("#111111") : QColor("#ffffff");
        t.glow = acc; t.glow_ = dark ? 8 : 4;
        v << t;
    }
    {   // Black Glass
        Theme t; t.id = "black"; t.nameEn = "Black Glass"; t.nameAr = "زجاجي أسود"; t.builtIn = true; t.dark = true;
        t.bgTop = QColor("#1b1c20"); t.bgBottom = QColor("#060608");
        t.blobA = QColor(255, 255, 255, 34); t.blobB = QColor(150, 160, 180, 40);
        t.surface = QColor("#16171b"); t.surfaceOpacity = 62;
        t.text = QColor("#f5f6f8"); t.textMuted = QColor("#9a9ca5");
        t.accent = QColor("#e8e9ee"); t.accentText = QColor("#111114");
        t.border = QColor("#ffffff"); t.borderOpacity = 26; t.glow = QColor("#c0c8d8"); t.glow_ = 6;
        t.reflection = 40; v << t;
    }
    {   // White Glass
        Theme t; t.id = "white"; t.nameEn = "White Glass"; t.nameAr = "زجاجي أبيض"; t.builtIn = true; t.dark = false;
        t.bgTop = QColor("#f4f5f8"); t.bgBottom = QColor("#d9dce3");
        t.blobA = QColor(255, 255, 255, 220); t.blobB = QColor(170, 180, 200, 90);
        t.surface = QColor("#ffffff"); t.surfaceOpacity = 58;
        t.text = QColor("#18191d"); t.textMuted = QColor("#5d6170");
        t.accent = QColor("#2b2e36"); t.accentText = QColor("#ffffff");
        t.border = QColor("#ffffff"); t.borderOpacity = 85; t.glow = QColor("#8890a0"); t.glow_ = 0;
        t.shadow = 16; t.reflection = 60; v << t;
    }
    {   // Red & White Glass (default)
        Theme t; t.id = "red-white"; t.nameEn = "Red & White Glass"; t.nameAr = "زجاجي أبيض وأحمر"; t.builtIn = true; t.dark = false;
        t.bgTop = QColor("#fbf7f7"); t.bgBottom = QColor("#e7dcdd");
        t.blobA = QColor(255, 255, 255, 230); t.blobB = QColor(227, 25, 43, 70);
        t.surface = QColor("#ffffff"); t.surfaceOpacity = 60;
        t.text = QColor("#1c1517"); t.textMuted = QColor("#6e5f62");
        t.accent = QColor("#e3192b"); t.accentText = QColor("#ffffff");
        t.border = QColor("#ffffff"); t.borderOpacity = 88; t.glow = QColor("#e3192b"); t.glow_ = 8;
        t.shadow = 16; t.reflection = 60; v << t;
    }
    {   // Red & Black Glass
        Theme t; t.id = "red-black"; t.nameEn = "Red & Black Glass"; t.nameAr = "زجاجي أحمر وأسود"; t.builtIn = true; t.dark = true;
        t.bgTop = QColor("#1a0b0d"); t.bgBottom = QColor("#050304");
        t.blobA = QColor(255, 40, 60, 70); t.blobB = QColor(120, 0, 10, 90);
        t.surface = QColor("#120708"); t.surfaceOpacity = 64;
        t.text = QColor("#fff4f4"); t.textMuted = QColor("#c09a9e");
        t.accent = QColor("#ff1f35"); t.accentText = QColor("#ffffff");
        t.border = QColor("#ff5464"); t.borderOpacity = 38; t.glow = QColor("#ff1f35"); t.glow_ = 16;
        t.reflection = 30; v << t;
    }
    {   // Clear Glass (transparent)
        Theme t; t.id = "clear"; t.nameEn = "Clear Glass"; t.nameAr = "زجاجي شفاف"; t.builtIn = true; t.dark = true;
        t.bgTop = QColor("#3a4150"); t.bgBottom = QColor("#1b1f27");
        t.blobA = QColor(140, 200, 255, 90); t.blobB = QColor(255, 255, 255, 70);
        t.surface = QColor("#ffffff"); t.surfaceOpacity = 14;
        t.text = QColor("#ffffff"); t.textMuted = QColor("#d6dbe6");
        t.accent = QColor("#ffffff"); t.accentText = QColor("#1b1f27");
        t.border = QColor("#ffffff"); t.borderOpacity = 55; t.glow = QColor("#bfe0ff"); t.glow_ = 6;
        t.shadow = 12; t.reflection = 55; v << t;
    }
    return v;
}

QVector<Theme> ThemeManager::userThemes() const {
    QVector<Theme> v;
    QSettings *s = AppSettings::instance()->raw();
    s->beginGroup("themes");
    const QStringList keys = s->childKeys();
    for (const QString &k : keys) {
        bool ok = false;
        Theme t = Theme::fromJson(QJsonDocument::fromJson(s->value(k).toByteArray()).object(), &ok);
        if (ok) { t.id = k; t.builtIn = false; v << t; }
    }
    s->endGroup();
    return v;
}

Theme ThemeManager::find(const QString &id) const {
    for (const Theme &t : allThemes()) if (t.id == id) return t;
    for (const Theme &t : builtIns()) if (t.id == "red-white") return t;
    return builtIns().first();
}

void ThemeManager::apply(const QString &id) {
    AppSettings::instance()->setValue("ui/theme", id);
    activate(find(id));
}

void ThemeManager::preview(const Theme &t) { activate(t); }

void ThemeManager::saveUserTheme(const Theme &in) {
    Theme t = in;
    if (t.id.isEmpty() || t.builtIn || !t.id.startsWith("user-"))
        t.id = "user-" + QUuid::createUuid().toString(QUuid::Id128).left(8);
    t.builtIn = false;
    AppSettings::instance()->raw()->setValue("themes/" + t.id, QJsonDocument(t.toJson()).toJson(QJsonDocument::Compact));
    apply(t.id);
}

void ThemeManager::deleteUserTheme(const QString &id) {
    AppSettings::instance()->raw()->remove("themes/" + id);
    if (m_current.id == id) apply("red-white");
}

bool ThemeManager::exportTheme(const Theme &t, const QString &file) const {
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(t.toJson()).toJson(QJsonDocument::Indented));
    return f.commit();
}

bool ThemeManager::importTheme(const QString &file, Theme *out) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 256 * 1024) return false;
    bool ok = false;
    Theme t = Theme::fromJson(QJsonDocument::fromJson(f.readAll()).object(), &ok);
    if (!ok) return false;
    t.id.clear();
    saveUserTheme(t);
    if (out) *out = m_current;
    return true;
}

void ThemeManager::activate(const Theme &t) {
    m_current = t;
    QPalette p;
    p.setColor(QPalette::Window, t.bgBottom);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.dark ? t.bgBottom.lighter(140) : QColor("#ffffff"));
    p.setColor(QPalette::AlternateBase, t.dark ? t.bgTop : t.bgBottom);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::Button, t.surface);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::Highlight, t.accent);
    p.setColor(QPalette::HighlightedText, t.accentText);
    p.setColor(QPalette::ToolTipBase, t.surface);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::PlaceholderText, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::Text, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::WindowText, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.textMuted);
    qApp->setPalette(p);
    qApp->setStyleSheet(styleSheet(t));
    emit themeChanged();
}

QString ThemeManager::styleSheet(const Theme &t) const {
    const int r = t.radius;
    const int rs = qMax(4, r * 2 / 3);
    const QColor panel = t.surfaceColor();
    // Menus and dialogs need a more opaque glass for readability
    QColor solid = t.dark ? t.bgTop.darker(110) : QColor(255, 255, 255);
    solid.setAlpha(245);
    QColor field = t.dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 10);
    QColor fieldHover = t.dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 18);
    QColor sep = t.dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 30);
    QColor hoverBg = t.accent; hoverBg.setAlpha(t.dark ? 60 : 40);
    QColor accentSoft = t.accent; accentSoft.setAlpha(150);

    QString s;
    s += QString(R"(
* { font-family: "Segoe UI", "Segoe UI Variable", "Noto Sans Arabic", "Tahoma", sans-serif; font-size: 10pt; color: %1; }
QToolTip { background: %2; color: %1; border: 1px solid %3; border-radius: 6px; padding: 5px 8px; }
QDialog, QMessageBox, QInputDialog { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %4, stop:1 %5); }
QLabel { background: transparent; }
QLabel[muted="true"] { color: %6; }
QLabel[heading="true"] { font-size: 13pt; font-weight: 600; }
)").arg(t.text.name(), rgba(solid), rgba(t.borderColor()), t.bgTop.name(), t.bgBottom.name(), t.textMuted.name());

    s += QString(R"(
QMenuBar { background: transparent; border: none; padding: 0; }
QMenuBar::item { background: transparent; padding: 5px 10px; border-radius: %1px; color: %2; }
QMenuBar::item:selected, QMenuBar::item:pressed { background: %3; }
QMenu { background: %4; border: 1px solid %5; border-radius: %6px; padding: 6px; }
QMenu::item { padding: 6px 26px 6px 26px; border-radius: %1px; color: %2; }
QMenu::item:selected { background: %3; }
QMenu::item:disabled { color: %7; }
QMenu::separator { height: 1px; background: %8; margin: 5px 8px; }
QMenu::indicator { width: 14px; height: 14px; margin-left: 6px; margin-right: 6px; }
QMenu::right-arrow { width: 10px; height: 10px; }
)").arg(rs).arg(t.text.name(), rgba(hoverBg), rgba(solid), rgba(t.borderColor())).arg(r)
       .arg(t.textMuted.name(), rgba(sep));

    s += QString(R"(
QPushButton { background: %1; border: 1px solid %2; border-radius: %3px; padding: 7px 16px; min-height: 18px; }
QPushButton:hover { background: %4; }
QPushButton:pressed { background: %5; }
QPushButton:default, QPushButton[accent="true"] { background: %6; color: %7; border: 1px solid %6; font-weight: 600; }
QPushButton:default:hover, QPushButton[accent="true"]:hover { background: %8; }
QPushButton:disabled { color: %9; }
)").arg(rgba(field), rgba(t.borderColor(0.8))).arg(rs)
       .arg(rgba(fieldHover), rgba(hoverBg), t.accent.name(), t.accentText.name(), t.accent.lighter(112).name(), t.textMuted.name());

    s += QString(R"(
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTimeEdit, QKeySequenceEdit QLineEdit {
  background: %1; border: 1px solid %2; border-radius: %3px; padding: 5px 8px; selection-background-color: %4; selection-color: %5; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border: 1px solid %4; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: %6; border: 1px solid %2; border-radius: %3px; selection-background-color: %7; outline: 0; padding: 4px; }
QCheckBox, QRadioButton { spacing: 8px; background: transparent; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; border: 1px solid %8; background: %1; }
QCheckBox::indicator { border-radius: 4px; }
QRadioButton::indicator { border-radius: 9px; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked { background: %4; border: 1px solid %4; }
QGroupBox { border: 1px solid %2; border-radius: %3px; margin-top: 14px; padding: 10px 8px 8px 8px; background: %9; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 6px; left: 10px; color: %10; font-weight: 600; }
)").arg(rgba(field), rgba(t.borderColor(0.7))).arg(rs)
       .arg(t.accent.name(), t.accentText.name(), rgba(solid), rgba(hoverBg), rgba(accentSoft), rgba(field, field.alpha() / 2), t.text.name());

    s += QString(R"(
QListWidget, QTableWidget, QTreeWidget, QListView, QTableView, QTextBrowser, QPlainTextEdit {
  background: %1; border: 1px solid %2; border-radius: %3px; padding: 4px; outline: 0; alternate-background-color: %7; }
QListWidget::item, QTableWidget::item { padding: 6px 6px; border-radius: %4px; }
QListWidget::item:hover, QTableWidget::item:hover { background: %5; }
QListWidget::item:selected, QTableWidget::item:selected { background: %6; color: %8; }
QHeaderView::section { background: transparent; border: none; border-bottom: 1px solid %2; padding: 6px; font-weight: 600; color: %9; }
QTableCornerButton::section { background: transparent; border: none; }
)").arg(rgba(field), rgba(t.borderColor(0.6))).arg(r).arg(rs)
       .arg(rgba(field, field.alpha() * 2), rgba(accentSoft), rgba(field, field.alpha() / 2), t.accentText.name(), t.textMuted.name());

    s += QString(R"(
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: %1; border-radius: 4px; min-height: 30px; min-width: 30px; }
QScrollBar::handle:hover { background: %2; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QSlider { background: transparent; }
QSlider::groove:horizontal { height: 4px; background: %3; border-radius: 2px; }
QSlider::sub-page:horizontal { background: %4; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: %5; border: 2px solid %4; }
QTabWidget::pane { border: 1px solid %6; border-radius: %7px; top: -1px; background: %8; }
QTabBar::tab { background: transparent; padding: 8px 14px; border-radius: %9px; margin: 2px; }
QTabBar::tab:selected { background: %4; color: %10; }
QTabBar::tab:hover:!selected { background: %11; }
QSplitter::handle { background: transparent; }
)").arg(rgba(sep, 90), rgba(t.accent, 160), rgba(sep, 110), t.accent.name(),
         t.dark ? QString("#ffffff") : QString("#ffffff"), rgba(t.borderColor(0.6)))
       .arg(r).arg(rgba(field, field.alpha() / 2)).arg(rs)
       .arg(t.accentText.name(), rgba(hoverBg));

    s += QString(R"(
#GlassToolButton { background: transparent; border: none; border-radius: %1px; padding: 0; }
#GlassToolButton:hover { background: %2; }
#GlassToolButton:pressed { background: %3; }
#GlassToolButton:checked { background: %3; }
#TitleText { font-weight: 700; letter-spacing: 1px; font-size: 10.5pt; }
#TimeLabel { font-family: "Segoe UI", "Consolas", monospace; font-size: 9.5pt; color: %4; }
#OsdLabel { background: %5; border: 1px solid %6; border-radius: %7px; padding: 8px 14px; font-size: 12pt; font-weight: 600; }
#SettingsNav { background: transparent; border: none; }
#SettingsNav::item { padding: 9px 12px; margin: 1px 0; border-radius: %1px; }
#SettingsNav::item:selected { background: %8; color: %9; }
)").arg(rs).arg(rgba(hoverBg), rgba(accentSoft), t.textMuted.name(),
            rgba(t.surfaceColor(60)), rgba(t.borderColor()))
       .arg(r).arg(t.accent.name(), t.accentText.name());
    Q_UNUSED(panel);
    return s;
}
