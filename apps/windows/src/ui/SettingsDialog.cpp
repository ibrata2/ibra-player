#include "SettingsDialog.h"
#include "MainWindow.h"
#include "engine/MpvWidget.h"
#include "core/I18n.h"
#include "core/AppSettings.h"
#include "core/Library.h"
#include "core/Shortcuts.h"
#include "core/Log.h"
#include "core/Playlist.h"
#include "theme/Theme.h"
#include "platform/Platform.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QFontComboBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTextBrowser>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QDir>
#include <QSet>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    if (auto *mw = qobject_cast<MainWindow *>(parent)) m_engine = mw->engine();
    setWindowTitle(T("act.settings") + " — iBRA PLAYER");
    resize(900, 620);
    auto *root = new QVBoxLayout(this);
    auto *body = new QHBoxLayout;
    m_nav = new QListWidget(this);
    m_nav->setObjectName("SettingsNav");
    m_nav->setFixedWidth(210);
    m_stack = new QStackedWidget(this);
    body->addWidget(m_nav);
    body->addWidget(m_stack, 1);
    root->addLayout(body, 1);

    auto *buttons = new QHBoxLayout;
    auto *imp = new QPushButton(T("set.import"), this);
    auto *exp = new QPushButton(T("set.export"), this);
    auto *def = new QPushButton(T("set.restoreDefaults"), this);
    auto *close = new QPushButton(T("dlg.close"), this);
    close->setDefault(true);
    buttons->addWidget(imp);
    buttons->addWidget(exp);
    buttons->addWidget(def);
    buttons->addStretch();
    buttons->addWidget(close);
    root->addLayout(buttons);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(exp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getSaveFileName(this, T("set.export"), "iBRA-PLAYER-settings.ini", "INI (*.ini)");
        if (!f.isEmpty()) QMessageBox::information(this, "iBRA PLAYER", AppSettings::instance()->exportTo(f) ? T("set.exported") : T("dlg.saveFailed"));
    });
    connect(imp, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, T("set.import"), QString(), "INI (*.ini)");
        if (f.isEmpty()) return;
        if (AppSettings::instance()->importFrom(f)) { QMessageBox::information(this, "iBRA PLAYER", T("set.imported")); accept(); }
        else QMessageBox::warning(this, "iBRA PLAYER", T("set.importFailed"));
    });
    connect(def, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, "iBRA PLAYER", T("q.restoreDefaults")) != QMessageBox::Yes) return;
        AppSettings::instance()->restoreDefaults();
        accept();
    });
    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    buildPages();
    m_nav->setCurrentRow(0);
}

void SettingsDialog::selectPage(const QString &id) {
    if (m_pageIndex.contains(id)) m_nav->setCurrentRow(m_pageIndex[id]);
}

QFormLayout *SettingsDialog::addPage(const QString &id, const char *titleKey) {
    auto *scroll = new QScrollArea(m_stack);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:transparent;} QScrollArea>QWidget>QWidget{background:transparent;}");
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(18, 8, 18, 8);
    auto *title = new QLabel(T(titleKey), page);
    title->setProperty("heading", true);
    v->addWidget(title);
    auto *form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignLeading | Qt::AlignVCenter);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(10);
    v->addLayout(form);
    v->addStretch();
    scroll->setWidget(page);
    m_pageIndex[id] = m_stack->addWidget(scroll);
    m_nav->addItem(T(titleKey));
    return form;
}

QCheckBox *SettingsDialog::check(QFormLayout *f, const char *labelKey, const QString &key) {
    auto *c = new QCheckBox(T(labelKey));
    c->setChecked(AppSettings::instance()->boolValue(key));
    connect(c, &QCheckBox::toggled, this, [key](bool on) { AppSettings::instance()->setValue(key, on); });
    f->addRow(c);
    return c;
}

QSpinBox *SettingsDialog::spin(QFormLayout *f, const char *labelKey, const QString &key, int lo, int hi, const QString &suffix, int step) {
    auto *s = new QSpinBox;
    s->setRange(lo, hi);
    s->setSingleStep(step);
    s->setSuffix(suffix);
    s->setValue(AppSettings::instance()->intValue(key));
    connect(s, &QSpinBox::valueChanged, this, [key](int v) { AppSettings::instance()->setValue(key, v); });
    f->addRow(T(labelKey), s);
    return s;
}

QComboBox *SettingsDialog::combo(QFormLayout *f, const char *labelKey, const QString &key, const QList<QPair<QString, QString>> &items) {
    auto *c = new QComboBox;
    for (const auto &it : items) c->addItem(it.second, it.first);
    const int idx = c->findData(AppSettings::instance()->stringValue(key));
    c->setCurrentIndex(qMax(0, idx));
    connect(c, &QComboBox::currentIndexChanged, this, [c, key] { AppSettings::instance()->setValue(key, c->currentData().toString()); });
    f->addRow(T(labelKey), c);
    return c;
}

QWidget *SettingsDialog::colorButton(const QString &key, bool alpha) {
    auto *b = new QPushButton;
    b->setFixedWidth(120);
    auto paint = [b, key] {
        const QColor c(AppSettings::instance()->stringValue(key));
        b->setText(ltr(c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb).toUpper()));
        b->setStyleSheet(QString("QPushButton{background:%1;color:%2;border-radius:6px;padding:6px;}")
                             .arg(c.name(QColor::HexArgb), c.lightness() > 140 ? "#000" : "#fff"));
    };
    paint();
    connect(b, &QPushButton::clicked, this, [this, key, alpha, paint] {
        const QColor c = QColorDialog::getColor(QColor(AppSettings::instance()->stringValue(key)), this, QString(),
                                                alpha ? QColorDialog::ShowAlphaChannel : QColorDialog::ColorDialogOptions());
        if (!c.isValid()) return;
        AppSettings::instance()->setValue(key, c.name(QColor::HexArgb));
        paint();
    });
    return b;
}

void SettingsDialog::buildPages() {
    AppSettings *s = AppSettings::instance();

    // ---------------- Interface
    {
        QFormLayout *f = addPage("interface", "set.interface");
        spin(f, "set.autoHide", "ui/autoHideMs", 500, 20000, " ms", 250);
        check(f, "set.osd", "ui/osd");
        check(f, "set.splash", "ui/showSplash");
        check(f, "set.rememberGeometry", "ui/rememberGeometry");
        check(f, "set.singleInstance", "ui/singleInstance");
        check(f, "act.alwaysOnTop", "ui/alwaysOnTop");
        QCheckBox *bd = check(f, "set.backdrop", "ui/windowBackdrop");
        bd->setEnabled(Platform::backdropSupported());
        auto *note = new QLabel(Platform::backdropSupported() ? T("set.backdropNote") : T("set.backdropUnsupported"));
        note->setProperty("muted", true);
        note->setWordWrap(true);
        f->addRow(note);
    }
    // ---------------- Themes
    {
        QFormLayout *f = addPage("themes", "set.themes");
        auto *list = new QListWidget;
        list->setMinimumHeight(220);
        const bool ar = I18n::instance()->isRtl();
        for (const Theme &t : ThemeManager::instance()->allThemes()) {
            auto *it = new QListWidgetItem(t.name(ar) + (t.builtIn ? QString() : "  ★"));
            it->setData(Qt::UserRole, t.id);
            QPixmap sw(36, 20);
            sw.fill(Qt::transparent);
            QPainter p(&sw);
            p.setRenderHint(QPainter::Antialiasing);
            QLinearGradient g(0, 0, 36, 0);
            g.setColorAt(0, t.bgTop); g.setColorAt(1, t.bgBottom);
            p.setBrush(g); p.setPen(QPen(t.border, 1));
            p.drawRoundedRect(QRectF(0.5, 0.5, 35, 19), 5, 5);
            p.setBrush(t.accent); p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(26, 10), 5, 5);
            p.end();
            it->setIcon(QIcon(sw));
            list->addItem(it);
            if (t.id == ThemeManager::instance()->current().id) list->setCurrentItem(it);
        }
        list->setIconSize(QSize(36, 20));
        connect(list, &QListWidget::currentItemChanged, this, [](QListWidgetItem *it) {
            if (it) ThemeManager::instance()->apply(it->data(Qt::UserRole).toString());
        });
        f->addRow(list);
        auto *studio = new QPushButton(T("act.themeStudio"));
        studio->setProperty("accent", true);
        connect(studio, &QPushButton::clicked, this, [this] { emit openThemeStudio(); accept(); });
        f->addRow(studio);
    }
    // ---------------- Language
    {
        QFormLayout *f = addPage("language", "set.language");
        auto *g = new QButtonGroup(this);
        for (const auto &l : QList<QPair<QString, QString>>{{"ar", "العربية"}, {"en", "English"}}) {
            auto *r = new QRadioButton(l.second);
            r->setChecked(s->stringValue("ui/language") == l.first);
            g->addButton(r);
            const QString code = l.first;
            connect(r, &QRadioButton::toggled, this, [code](bool on) {
                if (!on) return;
                AppSettings::instance()->setValue("ui/language", code);
                I18n::instance()->setLanguage(code);
            });
            f->addRow(r);
        }
        auto *note = new QLabel(T("set.languageNote"));
        note->setProperty("muted", true);
        note->setWordWrap(true);
        f->addRow(note);
    }
    // ---------------- Playback
    {
        QFormLayout *f = addPage("playback", "set.playback");
        spin(f, "set.seekSmall", "play/seekSmall", 1, 120, " s");
        spin(f, "set.seekLarge", "play/seekLarge", 5, 600, " s");
        auto *step = new QDoubleSpinBox;
        step->setRange(0.05, 1.0);
        step->setSingleStep(0.05);
        step->setValue(s->doubleValue("play/speedStep"));
        connect(step, &QDoubleSpinBox::valueChanged, this, [](double v) { AppSettings::instance()->setValue("play/speedStep", v); });
        f->addRow(T("set.speedStep"), step);
        check(f, "set.resume", "play/resume");
        check(f, "set.autoNext", "play/autoNext");
        check(f, "set.autoPlay", "play/autoPlayOnOpen");
        check(f, "set.pitch", "play/pitchCorrection");
    }
    // ---------------- Video
    {
        QFormLayout *f = addPage("video", "set.video");
        combo(f, "set.scaling", "video/scale", {{"bilinear", T("scale.fast")}, {"spline36", T("scale.balanced")},
                                               {"ewa_lanczossharp", T("scale.quality")}});
        combo(f, "set.toneMapping", "video/toneMapping", {{"auto", T("tm.auto")}, {"bt.2390", "BT.2390"}, {"hable", "Hable"},
                                                         {"mobius", "Möbius"}, {"reinhard", "Reinhard"}, {"clip", T("tm.clip")}});
        check(f, "set.deband", "video/debandFilter");
        auto *dirRow = new QWidget;
        auto *h = new QHBoxLayout(dirRow);
        h->setContentsMargins(0, 0, 0, 0);
        auto *dir = new QLabel(QDir::toNativeSeparators(s->stringValue("video/screenshotDir")));
        dir->setWordWrap(true);
        auto *browse = new QPushButton(T("dlg.browse"));
        h->addWidget(dir, 1);
        h->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, [this, dir] {
            const QString d = QFileDialog::getExistingDirectory(this, T("set.screenshotDir"), AppSettings::instance()->stringValue("video/screenshotDir"));
            if (d.isEmpty()) return;
            AppSettings::instance()->setValue("video/screenshotDir", d);
            dir->setText(QDir::toNativeSeparators(d));
        });
        f->addRow(T("set.screenshotDir"), dirRow);
    }
    // ---------------- Hardware acceleration
    {
        QFormLayout *f = addPage("hwaccel", "set.hwaccel");
        combo(f, "set.decoder", "video/hwdec", {
            {"auto-safe", T("hw.auto")}, {"no", T("hw.software")},
            {"d3d11va", "D3D11VA (Direct3D 11)"}, {"d3d11va-copy", "D3D11VA copy-back"},
            {"dxva2-copy", "DXVA2 copy-back"}, {"qsv-copy", "Intel Quick Sync (QSV)"},
            {"nvdec-copy", "NVIDIA NVDEC"}, {"vulkan-copy", "Vulkan"}});
        auto *note = new QLabel(T("hw.note"));
        note->setWordWrap(true);
        note->setProperty("muted", true);
        f->addRow(note);
        if (m_engine) {
            const QString cur = m_engine->getPropertyString("hwdec-current");
            auto *active = new QLabel(m_engine->hasMedia() ? (cur.isEmpty() || cur == "no" ? T("hw.activeSoftware") : cur) : T("hw.noMedia"));
            f->addRow(T("hw.active"), active);
        }
    }
    // ---------------- Audio
    {
        QFormLayout *f = addPage("audio", "set.audio");
        check(f, "set.normalize", "audio/normalize");
        auto *bal = new QSlider(Qt::Horizontal);
        bal->setRange(-100, 100);
        bal->setValue(s->intValue("audio/balance"));
        bal->setLayoutDirection(Qt::LeftToRight);
        auto *balLabel = new QLabel(QString::number(bal->value()));
        connect(bal, &QSlider::valueChanged, this, [balLabel](int v) { balLabel->setText(QString::number(v)); AppSettings::instance()->setValue("audio/balance", v); });
        auto *br = new QWidget; auto *bh = new QHBoxLayout(br); bh->setContentsMargins(0, 0, 0, 0);
        bh->addWidget(new QLabel(T("bal.left"))); bh->addWidget(bal, 1); bh->addWidget(new QLabel(T("bal.right"))); bh->addWidget(balLabel);
        f->addRow(T("set.balance"), br);
        spin(f, "set.audioDelay", "audio/delayMs", -10000, 10000, " ms", 10);
        auto *langs = new QLineEdit(s->stringValue("audio/preferredLang"));
        connect(langs, &QLineEdit::editingFinished, this, [langs] { AppSettings::instance()->setValue("audio/preferredLang", langs->text().trimmed()); });
        f->addRow(T("set.audioLang"), langs);
    }
    // ---------------- Audio output
    {
        QFormLayout *f = addPage("audioOutput", "set.audioOutput");
        combo(f, "set.aoDriver", "audio/output", {{"auto", T("ao.auto")}, {"wasapi", "WASAPI"}, {"openal", "OpenAL"}, {"sdl", "SDL"}});
        if (m_engine) {
            auto *dev = new QComboBox;
            const QString cur = m_engine->getPropertyString("audio-device");
            for (const QVariant &d : m_engine->getProperty("audio-device-list").toList()) {
                const QVariantMap m = d.toMap();
                dev->addItem(m.value("description").toString(), m.value("name").toString());
                if (m.value("name").toString() == cur) dev->setCurrentIndex(dev->count() - 1);
            }
            connect(dev, &QComboBox::currentIndexChanged, this, [this, dev] {
                m_engine->setProperty("audio-device", dev->currentData().toString());
                AppSettings::instance()->raw()->setValue("audio/device", dev->currentData().toString());
            });
            f->addRow(T("set.audioDevice"), dev);
        }
        combo(f, "set.channels", "audio/channels", {{"auto-safe", T("ch.auto")}, {"stereo", T("ch.stereo")},
                                                     {"5.1", "5.1"}, {"7.1", "7.1"}, {"auto", T("ch.all")}});
        check(f, "set.exclusive", "audio/exclusive");
        check(f, "set.passthrough", "audio/passthrough");
        auto *note = new QLabel(T("set.passthroughNote"));
        note->setWordWrap(true);
        note->setProperty("muted", true);
        f->addRow(note);
    }
    // ---------------- Subtitles
    {
        QFormLayout *f = addPage("subtitles", "set.subtitles");
        check(f, "set.subAuto", "sub/autoLoad");
        auto *font = new QFontComboBox;
        font->setCurrentFont(QFont(s->stringValue("sub/font")));
        connect(font, &QFontComboBox::currentFontChanged, this, [](const QFont &fn) { AppSettings::instance()->setValue("sub/font", fn.family()); });
        f->addRow(T("set.subFont"), font);
        spin(f, "set.subSize", "sub/size", 12, 140);
        check(f, "set.subBold", "sub/bold");
        f->addRow(T("set.subColor"), colorButton("sub/color", false));
        f->addRow(T("set.subBorderColor"), colorButton("sub/borderColor", false));
        auto *bs = new QDoubleSpinBox;
        bs->setRange(0, 10); bs->setSingleStep(0.5); bs->setValue(s->doubleValue("sub/borderSize"));
        connect(bs, &QDoubleSpinBox::valueChanged, this, [](double v) { AppSettings::instance()->setValue("sub/borderSize", v); });
        f->addRow(T("set.subBorder"), bs);
        check(f, "set.subBackground", "sub/background");
        f->addRow(T("set.subBackgroundColor"), colorButton("sub/backgroundColor", true));
        auto *pos = new QSlider(Qt::Horizontal);
        pos->setRange(0, 150);
        pos->setValue(s->intValue("sub/position"));
        pos->setLayoutDirection(Qt::LeftToRight);
        connect(pos, &QSlider::valueChanged, this, [](int v) { AppSettings::instance()->setValue("sub/position", v); });
        f->addRow(T("set.subPosition"), pos);
        spin(f, "set.subDelay", "sub/delayMs", -60000, 60000, " ms", 50);
        combo(f, "set.subEncoding", "sub/codepage", {{"auto", T("enc.auto")}, {"utf-8", "UTF-8"}, {"cp1256", "Windows-1256 (العربية)"},
                                                     {"iso-8859-6", "ISO-8859-6 (العربية)"}, {"cp1252", "Windows-1252"},
                                                     {"cp1251", "Windows-1251"}, {"cp1254", "Windows-1254"}, {"utf-16", "UTF-16"}});
        check(f, "set.subOverride", "sub/overrideAss");
        auto *langs = new QLineEdit(s->stringValue("sub/preferredLang"));
        connect(langs, &QLineEdit::editingFinished, this, [langs] { AppSettings::instance()->setValue("sub/preferredLang", langs->text().trimmed()); });
        f->addRow(T("set.subLang"), langs);
    }
    // ---------------- Codecs (capability report from the real engine)
    buildCodecsPage();
    // ---------------- Playlists
    {
        QFormLayout *f = addPage("playlists", "set.playlists");
        check(f, "set.folderSiblings", "playlist/addFolderSiblings");
        auto *note = new QLabel(T("set.playlistFormats"));
        note->setWordWrap(true);
        note->setProperty("muted", true);
        f->addRow(note);
    }
    // ---------------- Shortcuts + mouse
    buildShortcutsPage();
    // ---------------- Privacy
    {
        QFormLayout *f = addPage("privacy", "set.privacy");
        check(f, "set.history", "privacy/history");
        auto *clear = new QPushButton(T("act.clearRecent"));
        connect(clear, &QPushButton::clicked, this, [this] {
            Library::instance()->clearHistory();
            QMessageBox::information(this, "iBRA PLAYER", T("set.historyCleared"));
        });
        f->addRow(clear);
        check(f, "set.redactLogs", "privacy/redactLogs");
        check(f, "set.updateCheck", "privacy/updateCheck");
        auto *logs = new QPushButton(T("act.openLog"));
        connect(logs, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(Log::filePath()).absolutePath())); });
        f->addRow(logs);
        auto *note = new QLabel(T("set.privacyNote"));
        note->setWordWrap(true);
        note->setProperty("muted", true);
        f->addRow(note);
    }
    // ---------------- Windows integration
    {
        QFormLayout *f = addPage("integration", "set.integration");
        auto *mk = new QCheckBox(T("set.globalMediaKeys"));
        mk->setChecked(s->raw()->value("ui/globalMediaKeys", true).toBool());
        connect(mk, &QCheckBox::toggled, this, [](bool on) {
            AppSettings::instance()->raw()->setValue("ui/globalMediaKeys", on);
            emit AppSettings::instance()->changed("ui/globalMediaKeys");
        });
        f->addRow(mk);
#ifdef Q_OS_WIN
        auto *ctx = new QPushButton(Platform::isContextMenuRegistered() ? T("int.ctxRemove") : T("int.ctxAdd"));
        connect(ctx, &QPushButton::clicked, this, [this, ctx] {
            if (Platform::isContextMenuRegistered()) Platform::unregisterAll();
            else Platform::registerContextMenu();
            ctx->setText(Platform::isContextMenuRegistered() ? T("int.ctxRemove") : T("int.ctxAdd"));
        });
        f->addRow(T("int.ctx"), ctx);
        auto *assoc = new QPushButton(T("int.assocButton"));
        connect(assoc, &QPushButton::clicked, this, [this] {
            if (QMessageBox::question(this, "iBRA PLAYER", T("q.assoc")) != QMessageBox::Yes) return;
            QStringList ext = Playlist::mediaExtensions();
            ext.removeAll("jpg"); ext.removeAll("jpeg"); ext.removeAll("png"); ext.removeAll("webp"); ext.removeAll("bmp");
            ext.removeAll("gif"); ext.removeAll("tif"); ext.removeAll("tiff"); ext.removeAll("avif"); ext.removeAll("jxl");
            ext << Playlist::playlistExtensions();
            Platform::registerFileAssociations(ext);
            Platform::openDefaultAppsSettings();
        });
        f->addRow(T("int.assoc"), assoc);
        auto *note = new QLabel(T("int.note"));
        note->setWordWrap(true);
        note->setProperty("muted", true);
        f->addRow(note);
#else
        f->addRow(new QLabel(T("int.windowsOnly")));
#endif
    }
}

void SettingsDialog::buildCodecsPage() {
    QFormLayout *f = addPage("codecs", "set.codecs");
    if (!m_engine) return;
    f->addRow(T("codec.engine"), new QLabel(m_engine->getPropertyString("mpv-version")));
    f->addRow("FFmpeg", new QLabel(m_engine->getPropertyString("ffmpeg-version")));
    QSet<QString> decoders;
    for (const QVariant &d : m_engine->getProperty("decoder-list").toList()) decoders.insert(d.toMap().value("codec").toString());
    QSet<QString> demuxers;
    const QStringList checks = {
        "h264|H.264 / AVC", "hevc|H.265 / HEVC", "av1|AV1", "vp8|VP8", "vp9|VP9", "mpeg1video|MPEG-1",
        "mpeg2video|MPEG-2", "mpeg4|MPEG-4 Part 2 (DivX/Xvid)", "vc1|VC-1", "wmv3|WMV9", "vvc|H.266 / VVC",
        "prores|ProRes", "mjpeg|Motion JPEG", "theora|Theora",
        "aac|AAC", "mp3float|MP3", "flac|FLAC", "opus|Opus", "vorbis|Vorbis", "ac3|AC-3", "eac3|E-AC-3",
        "dca|DTS", "truehd|Dolby TrueHD", "alac|ALAC", "ape|Monkey's Audio (APE)", "wmav2|WMA", "wmapro|WMA Pro",
        "amrnb|AMR-NB", "amrwb|AMR-WB", "pcm_s16le|PCM (WAV/AIFF)",
        "ass|ASS/SSA", "subrip|SRT", "webvtt|WebVTT", "hdmv_pgs_subtitle|PGS (Blu-ray)", "dvd_subtitle|VobSub (IDX/SUB)",
        "dvb_subtitle|DVB"};
    auto *table = new QTableWidget(checks.size(), 2);
    table->setHorizontalHeaderLabels({T("codec.format"), T("codec.status")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->verticalHeader()->hide();
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setMinimumHeight(380);
    int row = 0;
    for (const QString &c : checks) {
        const QString codec = c.section('|', 0, 0), label = c.section('|', 1);
        bool ok = decoders.contains(codec);
        if (codec == "mp3float") ok = ok || decoders.contains("mp3");
        table->setItem(row, 0, new QTableWidgetItem(label));
        auto *st = new QTableWidgetItem(ok ? "✓ " + T("codec.available") : "✗ " + T("codec.missing"));
        st->setForeground(ok ? QColor("#2fb344") : QColor("#e3192b"));
        table->setItem(row++, 1, st);
    }
    f->addRow(table);
    auto *note = new QLabel(T("codec.note").arg(decoders.size()));
    note->setWordWrap(true);
    note->setProperty("muted", true);
    f->addRow(note);
}

void SettingsDialog::buildShortcutsPage() {
    QFormLayout *f = addPage("shortcuts", "set.shortcuts");
    const auto &defs = Shortcuts::all();
    auto *table = new QTableWidget(defs.size(), 2);
    table->setHorizontalHeaderLabels({T("sc.action"), T("sc.keys")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->setColumnWidth(1, 200);
    table->verticalHeader()->hide();
    // Rows tall enough for the key editor, so shortcuts are never cut off
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->verticalHeader()->setDefaultSectionSize(44);
    table->setMinimumHeight(420);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    QList<QKeySequenceEdit *> edits;
    for (int i = 0; i < defs.size(); ++i) {
        const ShortcutDef &d = defs[i];
        auto *item = new QTableWidgetItem(T(d.labelKey));
        item->setFlags(Qt::ItemIsEnabled);
        table->setItem(i, 0, item);
        auto *edit = new QKeySequenceEdit(Shortcuts::get(d.id));
        edit->setLayoutDirection(Qt::LeftToRight);     // "Ctrl+O" always reads left to right
        auto *cell = new QWidget;
        auto *cl = new QHBoxLayout(cell);
        cl->setContentsMargins(8, 0, 8, 0);
        cl->addWidget(edit, 0, Qt::AlignVCenter);
        const QString id = d.id;
        connect(edit, &QKeySequenceEdit::editingFinished, this, [edit, id] {
            QKeySequence seq = edit->keySequence();
            if (seq.count() > 1) { seq = QKeySequence(seq[0]); edit->setKeySequence(seq); }
            Shortcuts::set(id, seq);
        });
        table->setCellWidget(i, 1, cell);
        edits << edit;
    }
    f->addRow(table);
    auto *reset = new QPushButton(T("sc.reset"));
    connect(reset, &QPushButton::clicked, this, [edits] {
        Shortcuts::resetAll();
        for (int i = 0; i < edits.size(); ++i) edits[i]->setKeySequence(Shortcuts::get(Shortcuts::all()[i].id));
    });
    f->addRow(reset);

    auto *mouseTitle = new QLabel(T("set.mouse"));
    mouseTitle->setProperty("heading", true);
    f->addRow(mouseTitle);
    combo(f, "mouse.wheel", "mouse/wheel", {{"volume", T("act.volume")}, {"seek", T("mouse.seek")}, {"none", T("menu.none")}});
    combo(f, "mouse.left", "mouse/leftClick", {{"pause", T("act.playPause")}, {"none", T("menu.none")}});
    combo(f, "mouse.double", "mouse/doubleClick", {{"fullscreen", T("act.fullscreen")}, {"none", T("menu.none")}});
    combo(f, "mouse.middle", "mouse/middleClick", {{"pause", T("act.playPause")}, {"none", T("menu.none")}});
}
