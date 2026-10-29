#include "ThemeStudio.h"
#include "ControlBar.h"
#include "GlassPanel.h"
#include "core/I18n.h"
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>

namespace {
// Preview surface: frosted backdrop with a real control bar on top
class PreviewArea : public QWidget {
public:
    explicit PreviewArea(QWidget *p) : QWidget(p) {
        setMinimumHeight(170);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(18, 18, 18, 18);
        v->addStretch();
        auto *bar = new ControlBar(this);
        bar->setTimes(1234, 5400);
        bar->setPlaying(true);
        bar->setMuted(false, 80);
        bar->setSpeed(1.0);
        v->addWidget(bar);
        connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Theme &t = ThemeManager::instance()->current();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 12, 12);
        p.setClipPath(clip);
        Glass::paintBackdrop(p, rect(), t, false);
        // fake "video" frame so translucency is visible
        QRectF video = QRectF(rect()).adjusted(18, 18, -18, -18);
        QLinearGradient g(video.topLeft(), video.bottomRight());
        g.setColorAt(0, QColor("#1d3557")); g.setColorAt(0.5, QColor("#457b9d")); g.setColorAt(1, QColor("#e76f51"));
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(video, 8, 8);
    }
};
}

static const QList<QPair<QString, const char *>> kColorFields = {
    {"bgTop", "ts.bgTop"}, {"bgBottom", "ts.bgBottom"}, {"blobA", "ts.blobA"}, {"blobB", "ts.blobB"},
    {"surface", "ts.surface"}, {"text", "ts.text"}, {"textMuted", "ts.textMuted"}, {"accent", "ts.accent"},
    {"accentText", "ts.accentText"}, {"border", "ts.border"}, {"glow", "ts.glow"}};
static const QList<std::tuple<QString, const char *, int, int>> kSliderFields = {
    {"surfaceOpacity", "ts.opacity", 0, 100}, {"blur", "ts.blur", 0, 100}, {"reflection", "ts.reflection", 0, 100},
    {"borderOpacity", "ts.borderOpacity", 0, 100}, {"glowStrength", "ts.glowStrength", 0, 40},
    {"shadow", "ts.shadow", 0, 40}, {"radius", "ts.radius", 0, 30}, {"buttonSize", "ts.buttonSize", 26, 64}};

ThemeStudio::ThemeStudio(QWidget *parent) : QDialog(parent) {
    setWindowTitle(T("act.themeStudio") + " — iBRA PLAYER");
    resize(1040, 700);
    m_savedId = ThemeManager::instance()->current().id;
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(90);
    connect(&m_previewTimer, &QTimer::timeout, this, [this] { ThemeManager::instance()->preview(m_edit); });

    auto *root = new QHBoxLayout(this);
    // ---- left: themes
    auto *left = new QVBoxLayout;
    auto *lt = new QLabel(T("ts.themes"));
    lt->setProperty("heading", true);
    left->addWidget(lt);
    m_list = new QListWidget;
    m_list->setFixedWidth(230);
    left->addWidget(m_list, 1);
    auto *imp = new QPushButton(T("ts.import"));
    auto *exp = new QPushButton(T("ts.export"));
    m_delete = new QPushButton(T("ts.delete"));
    left->addWidget(imp);
    left->addWidget(exp);
    left->addWidget(m_delete);
    root->addLayout(left);

    // ---- right: preview + editors
    auto *right = new QVBoxLayout;
    right->addWidget(new PreviewArea(this));
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:transparent;} QScrollArea>QWidget>QWidget{background:transparent;}");
    auto *editors = new QWidget;
    auto *grid = new QGridLayout(editors);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);

    m_nameEn = new QLineEdit;
    m_nameAr = new QLineEdit;
    m_nameAr->setLayoutDirection(Qt::RightToLeft);
    m_dark = new QCheckBox(T("ts.dark"));
    grid->addWidget(new QLabel(T("ts.nameEn")), 0, 0);
    grid->addWidget(m_nameEn, 0, 1);
    grid->addWidget(new QLabel(T("ts.nameAr")), 0, 2);
    grid->addWidget(m_nameAr, 0, 3);
    grid->addWidget(m_dark, 1, 0, 1, 2);
    connect(m_nameEn, &QLineEdit::textEdited, this, [this](const QString &s) { m_edit.nameEn = s; m_dirty = true; });
    connect(m_nameAr, &QLineEdit::textEdited, this, [this](const QString &s) { m_edit.nameAr = s; m_dirty = true; });
    connect(m_dark, &QCheckBox::toggled, this, [this](bool on) { if (m_loading) return; m_edit.dark = on; schedulePreview(); });

    int row = 2, col = 0;
    for (const auto &cf : kColorFields) {
        auto *b = new QPushButton;
        b->setMinimumWidth(110);
        m_colors[cf.first] = b;
        grid->addWidget(new QLabel(T(cf.second)), row, col);
        grid->addWidget(b, row, col + 1);
        const QString key = cf.first;
        connect(b, &QPushButton::clicked, this, [this, key] {
            QJsonObject o = m_edit.toJson();
            const QColor c = QColorDialog::getColor(QColor(o[key].toString()), this, QString(), QColorDialog::ShowAlphaChannel);
            if (!c.isValid()) return;
            o[key] = c.name(QColor::HexArgb);
            const QString id = m_edit.id; const bool bi = m_edit.builtIn;
            m_edit = Theme::fromJson(o);
            m_edit.id = id; m_edit.builtIn = bi;
            syncEditors();
            schedulePreview();
        });
        col += 2;
        if (col > 2) { col = 0; ++row; }
    }
    if (col) { col = 0; ++row; }
    for (const auto &sf : kSliderFields) {
        auto *s = new QSlider(Qt::Horizontal);
        s->setRange(std::get<2>(sf), std::get<3>(sf));
        s->setLayoutDirection(Qt::LeftToRight);
        auto *val = new QLabel;
        val->setMinimumWidth(28);
        m_sliders[std::get<0>(sf)] = s;
        auto *w = new QWidget; auto *h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(s, 1); h->addWidget(val);
        grid->addWidget(new QLabel(T(std::get<1>(sf))), row, col);
        grid->addWidget(w, row, col + 1);
        const QString key = std::get<0>(sf);
        connect(s, &QSlider::valueChanged, this, [this, key, val](int v) {
            val->setText(QString::number(v));
            if (m_loading) return;
            QJsonObject o = m_edit.toJson();
            o[key] = v;
            const QString id = m_edit.id; const bool bi = m_edit.builtIn;
            m_edit = Theme::fromJson(o);
            m_edit.id = id; m_edit.builtIn = bi;
            schedulePreview();
        });
        col += 2;
        if (col > 2) { col = 0; ++row; }
    }
    scroll->setWidget(editors);
    right->addWidget(scroll, 1);

    auto *btns = new QHBoxLayout;
    auto *reset = new QPushButton(T("ts.reset"));
    auto *saveNew = new QPushButton(T("ts.saveAs"));
    auto *apply = new QPushButton(T("ts.apply"));
    apply->setDefault(true);
    auto *close = new QPushButton(T("dlg.close"));
    btns->addWidget(reset);
    btns->addStretch();
    btns->addWidget(saveNew);
    btns->addWidget(apply);
    btns->addWidget(close);
    right->addLayout(btns);
    root->addLayout(right, 1);

    auto *fallback = new QLabel(T("ts.fallbackNote"));
    fallback->setWordWrap(true);
    fallback->setProperty("muted", true);
    right->insertWidget(2, fallback);

    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *it) {
        if (!it) return;
        loadTheme(ThemeManager::instance()->find(it->data(Qt::UserRole).toString()));
        ThemeManager::instance()->preview(m_edit);
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        loadTheme(ThemeManager::instance()->find(m_edit.id));
        ThemeManager::instance()->preview(m_edit);
    });
    connect(saveNew, &QPushButton::clicked, this, [this] {
        Theme t = m_edit;
        if (t.builtIn) {
            t.nameEn = (t.nameEn.isEmpty() ? QString("Custom") : t.nameEn) + " (Custom)";
            t.nameAr = (t.nameAr.isEmpty() ? QString("مخصص") : t.nameAr) + " (مخصص)";
        }
        t.id.clear();
        ThemeManager::instance()->saveUserTheme(t);
        m_savedId = ThemeManager::instance()->current().id;
        m_dirty = false;
        refreshList();
    });
    connect(apply, &QPushButton::clicked, this, [this] {
        if (m_edit.builtIn && m_dirty) {   // built-ins are read-only: save edits as a custom theme
            Theme t = m_edit;
            t.id.clear();
            t.nameEn = m_edit.nameEn + " (Custom)";
            t.nameAr = m_edit.nameAr + " (مخصص)";
            ThemeManager::instance()->saveUserTheme(t);
        } else if (!m_edit.builtIn) {
            ThemeManager::instance()->saveUserTheme(m_edit);
        } else {
            ThemeManager::instance()->apply(m_edit.id);
        }
        m_savedId = ThemeManager::instance()->current().id;
        m_dirty = false;
        refreshList();
    });
    connect(close, &QPushButton::clicked, this, &ThemeStudio::reject);
    connect(m_delete, &QPushButton::clicked, this, [this] {
        if (m_edit.builtIn) return;
        if (QMessageBox::question(this, "iBRA PLAYER", T("q.deleteTheme").arg(m_edit.name(I18n::instance()->isRtl()))) != QMessageBox::Yes) return;
        ThemeManager::instance()->deleteUserTheme(m_edit.id);
        m_savedId = ThemeManager::instance()->current().id;
        refreshList();
    });
    connect(exp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, T("ts.export"), m_edit.nameEn + ".ibratheme", "iBRA theme (*.ibratheme *.json)");
        if (!f.isEmpty() && !ThemeManager::instance()->exportTheme(m_edit, f)) QMessageBox::warning(this, "iBRA PLAYER", T("dlg.saveFailed"));
    });
    connect(imp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, T("ts.import"), QString(), "iBRA theme (*.ibratheme *.json)");
        if (f.isEmpty()) return;
        Theme t;
        if (ThemeManager::instance()->importTheme(f, &t)) { m_savedId = t.id; refreshList(); }
        else QMessageBox::warning(this, "iBRA PLAYER", T("ts.importFailed"));
    });

    refreshList();
}

void ThemeStudio::refreshList() {
    QSignalBlocker b(m_list);
    m_list->clear();
    const bool ar = I18n::instance()->isRtl();
    const QString cur = ThemeManager::instance()->current().id;
    for (const Theme &t : ThemeManager::instance()->allThemes()) {
        auto *it = new QListWidgetItem(t.name(ar) + (t.builtIn ? QString() : "  ★"));
        it->setData(Qt::UserRole, t.id);
        m_list->addItem(it);
        if (t.id == cur) m_list->setCurrentItem(it);
    }
    loadTheme(ThemeManager::instance()->current());
}

void ThemeStudio::loadTheme(const Theme &t) {
    m_edit = t;
    m_dirty = false;
    syncEditors();
}

void ThemeStudio::syncEditors() {
    m_loading = true;
    const QJsonObject o = m_edit.toJson();
    m_nameEn->setText(m_edit.nameEn);
    m_nameAr->setText(m_edit.nameAr);
    m_dark->setChecked(m_edit.dark);
    for (auto it = m_colors.begin(); it != m_colors.end(); ++it) {
        const QColor c(o[it.key()].toString());
        it.value()->setText(ltr(c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb).toUpper()));
        it.value()->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:1px solid rgba(128,128,128,120);border-radius:6px;padding:5px;}")
                                      .arg(c.name(QColor::HexArgb), c.lightness() > 140 || c.alpha() < 100 ? "#000" : "#fff"));
    }
    for (auto it = m_sliders.begin(); it != m_sliders.end(); ++it) it.value()->setValue(o[it.key()].toInt());
    m_delete->setEnabled(!m_edit.builtIn);
    m_loading = false;
}

void ThemeStudio::schedulePreview() {
    m_dirty = true;
    m_previewTimer.start();
}

void ThemeStudio::reject() {
    // Discard unsaved live-preview changes
    m_previewTimer.stop();
    ThemeManager::instance()->apply(m_savedId);
    QDialog::reject();
}
