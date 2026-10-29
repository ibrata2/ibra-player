#include "MainWindow.h"
#include "TitleBar.h"
#include "ControlBar.h"
#include "PlaylistPanel.h"
#include "GlassPanel.h"
#include "Icons.h"
#include "SettingsDialog.h"
#include "ThemeStudio.h"
#include "MediaInfoDialog.h"
#include "AboutDialog.h"
#include "engine/MpvWidget.h"
#include "core/I18n.h"
#include "core/AppSettings.h"
#include "core/Library.h"
#include "core/Shortcuts.h"
#include "core/Log.h"
#include "theme/Theme.h"
#include "platform/Platform.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QWindow>
#include <QScreen>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVersionNumber>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QLineEdit>

static const char *kTelegram = "https://t.me/ibraprogram";

// =================================================================== GlassRoot
GlassRoot::GlassRoot(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_StyledBackground, false);
}

void GlassRoot::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const Theme &t = ThemeManager::instance()->current();
    const bool maxed = window()->isMaximized() || window()->isFullScreen();
    const QRectF r = rect();
    if (maxed) {
        Glass::paintBackdrop(p, r, t, translucentWindow);
        return;
    }
    // Rounded glass window with a luminous edge
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    p.setClipPath(clip);
    Glass::paintBackdrop(p, r, t, translucentWindow);
    p.setClipping(false);
    p.setPen(QPen(t.borderColor(1.2), 1));
    p.drawPath(clip);
}

Qt::Edges GlassRoot::edgesAt(const QPoint &p) const {
    if (window()->isMaximized() || window()->isFullScreen()) return {};
    const int m = 7;
    Qt::Edges e;
    if (p.x() < m) e |= Qt::LeftEdge;
    if (p.x() > width() - m) e |= Qt::RightEdge;
    if (p.y() < m) e |= Qt::TopEdge;
    if (p.y() > height() - m) e |= Qt::BottomEdge;
    return e;
}

void GlassRoot::mouseMoveEvent(QMouseEvent *e) {
    const Qt::Edges ed = edgesAt(e->position().toPoint());
    if ((ed & Qt::LeftEdge && ed & Qt::TopEdge) || (ed & Qt::RightEdge && ed & Qt::BottomEdge)) setCursor(Qt::SizeFDiagCursor);
    else if ((ed & Qt::RightEdge && ed & Qt::TopEdge) || (ed & Qt::LeftEdge && ed & Qt::BottomEdge)) setCursor(Qt::SizeBDiagCursor);
    else if (ed & (Qt::LeftEdge | Qt::RightEdge)) setCursor(Qt::SizeHorCursor);
    else if (ed & (Qt::TopEdge | Qt::BottomEdge)) setCursor(Qt::SizeVerCursor);
    else unsetCursor();
}

void GlassRoot::mousePressEvent(QMouseEvent *e) {
    const Qt::Edges ed = edgesAt(e->position().toPoint());
    if (ed && e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemResize(ed);
}

// ======================================================================= Stage
void Stage::relayout() {
    const int m = frame ? 8 : 0;
    const QRect vr = rect().adjusted(m, m, -m, -m);
    if (video) video->setGeometry(vr);
    if (placeholder) placeholder->setGeometry(vr);
    if (controls) {
        const int w = qMin(vr.width() - 24, 1180);
        const int h = controls->sizeHint().height();
        controls->setGeometry(vr.left() + (vr.width() - w) / 2, vr.bottom() - h - 10, w, h);
    }
    if (osd) {
        osd->adjustSize();
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        osd->move(rtl ? vr.right() - osd->width() - 18 : vr.left() + 18, vr.top() + 18);
    }
}

void Stage::paintEvent(QPaintEvent *) {
    if (!frame) return;
    QPainter p(this);
    Glass::paintPanel(p, QRectF(rect()).adjusted(3, 3, -3, -3), ThemeManager::instance()->current(), -1, 0, true);
}

// Idle screen shown on top of the (black) video surface
class Placeholder : public QWidget {
public:
    explicit Placeholder(QWidget *parent) : QWidget(parent) { setMouseTracking(true); setAcceptDrops(false); }
    QString title;
    bool audioOnly = false;
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const Theme &t = ThemeManager::instance()->current();
        p.fillRect(rect(), t.videoBackground);
        // faint accent aura
        QRadialGradient g(rect().center(), qMax(width(), height()) * 0.55);
        QColor a = t.accent; a.setAlpha(55);
        g.setColorAt(0, a);
        g.setColorAt(1, QColor(0, 0, 0, 0));
        p.fillRect(rect(), g);
        const int s = qBound(64, qMin(width(), height()) / 3, 220);
        const QPixmap logo = Icons::logo(s * 2);
        const QRect lr(width() / 2 - s / 2, height() / 2 - s / 2 - 40, s, s);
        p.drawPixmap(lr, logo);
        QFont f = font();
        f.setPointSizeF(16);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 235));
        p.drawText(QRect(0, lr.bottom() + 14, width(), 32), Qt::AlignCenter, audioOnly ? title : QStringLiteral("iBRA PLAYER"));
        f.setPointSizeF(10);
        f.setBold(false);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255, 150));
        p.drawText(QRect(0, lr.bottom() + 48, width(), 26), Qt::AlignCenter, audioOnly ? T("ph.audio") : T("ph.drop"));
    }
};

// ================================================================== MainWindow
MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle("iBRA PLAYER");
    setWindowIcon(QIcon(":/brand/logo.png"));
    setWindowFlag(Qt::FramelessWindowHint);
    setAcceptDrops(true);
    setMinimumSize(560, 360);

    m_playlist = new Playlist(this);
    m_playlist->setShuffle(AppSettings::instance()->boolValue("play/shuffle"));
    const QString rep = AppSettings::instance()->stringValue("play/repeat");
    m_playlist->setRepeat(rep == "one" ? Playlist::RepeatOne : rep == "all" ? Playlist::RepeatAll : Playlist::RepeatOff);

    const bool backdrop = AppSettings::instance()->boolValue("ui/windowBackdrop") && Platform::backdropSupported();
    if (backdrop) setAttribute(Qt::WA_TranslucentBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_root = new GlassRoot(this);
    m_root->translucentWindow = backdrop;
    outer->addWidget(m_root);

    auto *v = new QVBoxLayout(m_root);
    v->setContentsMargins(6, 4, 6, 6);
    v->setSpacing(4);
    m_title = new TitleBar(this, m_root);
    v->addWidget(m_title);

    m_split = new QSplitter(Qt::Horizontal, m_root);
    m_split->setChildrenCollapsible(false);
    m_split->setHandleWidth(6);
    m_stage = new Stage(m_split);
    m_mpv = new MpvWidget(m_stage);
    m_mpv->setFocusPolicy(Qt::StrongFocus);
    auto *ph = new Placeholder(m_stage);
    m_stage->placeholder = ph;
    m_controls = new ControlBar(m_stage);
    m_osd = new QLabel(m_stage);
    m_osd->setObjectName("OsdLabel");
    m_osd->hide();
    m_stage->video = m_mpv;
    m_stage->controls = m_controls;
    m_stage->osd = m_osd;
    m_playlistPanel = new PlaylistPanel(m_playlist, m_split);
    m_playlistPanel->setMinimumWidth(240);
    m_split->addWidget(m_stage);
    m_split->addWidget(m_playlistPanel);
    m_split->setStretchFactor(0, 1);
    m_split->setStretchFactor(1, 0);
    m_split->setSizes({900, 320});
    m_playlistPanel->setVisible(AppSettings::instance()->boolValue("playlist/visible"));
    v->addWidget(m_split, 1);

    // Mouse tracking for auto-hide + clicks on video
    for (QWidget *w : {static_cast<QWidget *>(m_mpv), static_cast<QWidget *>(ph), static_cast<QWidget *>(m_controls)}) {
        w->setMouseTracking(true);
        w->installEventFilter(this);
    }
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, &MainWindow::hideControlsIfIdle);
    m_osdTimer.setSingleShot(true);
    connect(&m_osdTimer, &QTimer::timeout, m_osd, &QWidget::hide);
    m_resumeTimer.setInterval(5000);
    connect(&m_resumeTimer, &QTimer::timeout, this, &MainWindow::saveResume);
    m_resumeTimer.start();

    // Engine signals
    connect(m_mpv, &MpvWidget::fileLoaded, this, &MainWindow::onFileLoaded);
    connect(m_mpv, &MpvWidget::endOfFile, this, &MainWindow::onEndOfFile);
    connect(m_mpv, &MpvWidget::positionChanged, this, &MainWindow::onPosition);
    connect(m_mpv, &MpvWidget::durationChanged, this, [this](double d) {
        m_controls->setTimes(m_mpv->position(), d);
        if (m_playlist->current() >= 0) {
            m_playlist->setDuration(m_playlist->current(), d);
            Library::instance()->setDuration(m_currentPath, d);
        }
        QList<double> chapters;
        for (const QVariant &c : m_mpv->getProperty("chapter-list").toList()) chapters << c.toMap().value("time").toDouble();
        m_controls->seek()->setChapters(chapters);
    });
    connect(m_mpv, &MpvWidget::pauseChanged, this, [this](bool p) {
        m_controls->setPlaying(!p && m_mpv->hasMedia());
        if (p) showControls(); else m_hideTimer.start(AppSettings::instance()->intValue("ui/autoHideMs"));
        updateMediaActions();
    });
    connect(m_mpv, &MpvWidget::volumeChanged, this, [this](int vol) {
        m_controls->setMuted(m_mpv->muted(), vol);
        AppSettings::instance()->setValue("play/volume", vol);
    });
    connect(m_mpv, &MpvWidget::muteChanged, this, [this](bool m) {
        m_controls->setMuted(m, m_mpv->volume());
        AppSettings::instance()->setValue("play/muted", m);
    });
    connect(m_mpv, &MpvWidget::speedChanged, m_controls, &ControlBar::setSpeed);
    connect(m_mpv, &MpvWidget::tracksChanged, this, &MainWindow::onTracksChanged);
    connect(m_mpv, &MpvWidget::titleChanged, this, [this](const QString &) { updateTitle(); });
    connect(m_mpv, &MpvWidget::abLoopChanged, this, [this] {
        const QVariant a = m_mpv->getProperty("ab-loop-a"), b = m_mpv->getProperty("ab-loop-b");
        m_controls->seek()->setAbLoop(a.typeId() == QMetaType::Double ? a.toDouble() : -1,
                                      b.typeId() == QMetaType::Double ? b.toDouble() : -1);
    });
    connect(m_mpv, &MpvWidget::logMessage, this, [](const QString &lvl, const QString &txt) { Log::write("mpv/" + lvl, txt); });
    connect(m_mpv, &MpvWidget::playbackError, this, [this](const QString &err) {
        osd(T("osd.error") + ": " + err, 4000);
        Log::write("error", "playback failed: " + err);
    });

    // Control bar
    connect(m_controls, &ControlBar::action, this, &MainWindow::onControlAction);
    connect(m_controls, &ControlBar::volumeMoved, this, [this](int v) { m_mpv->setVolume(v); if (m_mpv->muted()) m_mpv->setMuted(false); });
    connect(m_controls->seek(), &SeekSlider::seekRequested, this, [this](double s, bool final) {
        if (final) m_mpv->seekAbsolute(s);
        else m_mpv->commandAsync({"seek", s, "absolute+keyframes"});
    });

    // Playlist panel
    connect(m_playlistPanel, &PlaylistPanel::playRequested, this, &MainWindow::playIndex);
    connect(m_playlistPanel, &PlaylistPanel::addFilesRequested, this, [this] { openFileDialog(true); });
    connect(m_playlistPanel, &PlaylistPanel::addFolderRequested, this, [this] { openFolderDialog(true); });
    connect(m_playlistPanel, &PlaylistPanel::addUrlRequested, this, &MainWindow::openUrlDialog);
    connect(m_playlistPanel, &PlaylistPanel::loadPlaylistRequested, this, &MainWindow::loadPlaylistDialog);
    connect(m_playlistPanel, &PlaylistPanel::savePlaylistRequested, this, &MainWindow::savePlaylistDialog);
    connect(m_playlist, &Playlist::modeChanged, this, [this] {
        m_controls->setRepeatShuffle(int(m_playlist->repeat()), m_playlist->shuffle());
        updateMediaActions();
    });

    // Global media keys
    m_mediaKeys = new MediaKeyFilter(this);
    qApp->installNativeEventFilter(m_mediaKeys);
    connect(m_mediaKeys, &MediaKeyFilter::playPause, this, [this] { trigger("play_pause"); });
    connect(m_mediaKeys, &MediaKeyFilter::stop, this, [this] { trigger("stop"); });
    connect(m_mediaKeys, &MediaKeyFilter::next, this, [this] { trigger("next"); });
    connect(m_mediaKeys, &MediaKeyFilter::previous, this, [this] { trigger("previous"); });

    createActions();
    connect(I18n::instance(), &I18n::languageChanged, this, &MainWindow::retranslate);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &MainWindow::retheme);
    connect(AppSettings::instance(), &AppSettings::changed, this, &MainWindow::applySettings);
    connect(AppSettings::instance(), &AppSettings::reloaded, this, [this] {
        ThemeManager::instance()->apply(AppSettings::instance()->stringValue("ui/theme"));
        I18n::instance()->setLanguage(AppSettings::instance()->stringValue("ui/language"));
        for (auto it = m_actions.begin(); it != m_actions.end(); ++it) it.value()->setShortcut(Shortcuts::get(it.key()));
        applySettings();
    });
    connect(Library::instance(), &Library::changed, this, &MainWindow::updateMediaActions);

    retranslate();
    retheme();
    applySettings();
    m_mpv->setVolume(AppSettings::instance()->intValue("play/volume"));
    m_mpv->setMuted(AppSettings::instance()->boolValue("play/muted"));
    m_controls->setMuted(AppSettings::instance()->boolValue("play/muted"), AppSettings::instance()->intValue("play/volume"));
    m_controls->setSpeed(1.0);
    m_controls->setPlaying(false);
    m_controls->setRepeatShuffle(int(m_playlist->repeat()), m_playlist->shuffle());
    m_controls->btn("playlist")->setChecked(m_playlistPanel->isVisible());
    restoreWindowState();
    updateMediaActions();

    if (AppSettings::instance()->boolValue("privacy/updateCheck"))
        QTimer::singleShot(4000, this, [this] { checkForUpdates(true); });
}

MainWindow::~MainWindow() = default;

// ------------------------------------------------------------------ actions
QAction *MainWindow::act(const QString &id, const char *labelKey, bool checkable) {
    auto *a = new QAction(this);
    a->setProperty("labelKey", labelKey ? labelKey : (Shortcuts::find(id) ? Shortcuts::find(id)->labelKey : ""));
    a->setCheckable(checkable);
    if (Shortcuts::find(id)) {
        a->setShortcut(Shortcuts::get(id));
        a->setShortcutContext(Qt::WindowShortcut);
        addAction(a);   // keep shortcuts alive while menus are hidden (fullscreen)
    }
    connect(a, &QAction::triggered, this, [this, id] { trigger(id); });
    m_actions[id] = a;
    return a;
}

void MainWindow::createActions() {
    for (const ShortcutDef &d : Shortcuts::all()) act(d.id, d.labelKey, false);
    m_actions["always_on_top"]->setCheckable(true);
    m_actions["playlist"]->setCheckable(true);
    m_actions["shuffle"]->setCheckable(true);
    m_actions["favorite"]->setCheckable(true);
    act("open_audio_file", "act.loadAudio");
    act("load_playlist", "pl.load");
    act("search_subtitles", "act.searchSubtitles");
    act("screenshot_folder", "act.screenshotFolder");
    act("ab_set_a", "act.abSetA");
    act("ab_set_b", "act.abSetB");
    act("ab_clear", "act.abClear");
    act("audio_delay_reset", "act.audioDelayReset");
    act("sub_delay_reset", "act.subDelayReset");
    act("sub_size_up", "act.subSizeUp");
    act("sub_size_down", "act.subSizeDown");
    act("flip_h", "act.flipH", true);
    act("flip_v", "act.flipV", true);
    act("crop_fill", "act.cropFill", true);
    act("deinterlace", "act.deinterlace", true);
    act("filters_reset", "act.filtersReset");
    act("brightness_up", "act.brightnessUp");
    act("brightness_down", "act.brightnessDown");
    act("contrast_up", "act.contrastUp");
    act("contrast_down", "act.contrastDown");
    act("saturation_up", "act.saturationUp");
    act("saturation_down", "act.saturationDown");
    act("gamma_up", "act.gammaUp");
    act("gamma_down", "act.gammaDown");
    act("go_start", "act.goStart");
    act("about", "act.about");
    act("shortcuts_help", "act.shortcutsHelp");
    act("open_log", "act.openLog");
    act("check_updates", "act.checkUpdates");
    act("telegram", "act.telegram");
    act("clear_recent", "act.clearRecent");
    act("clear_favorites", "act.clearFavorites");
}

void MainWindow::retranslate() {
    const bool rtl = I18n::instance()->isRtl();
    qApp->setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it)
        it.value()->setText(T(it.value()->property("labelKey").toByteArray().constData()));
    buildMenus();
    m_stage->relayout();
    if (m_stage->placeholder) m_stage->placeholder->update();
    updateTitle();
}

void MainWindow::retheme() {
    const Theme &t = ThemeManager::instance()->current();
    // Never touch the native window before it is shown (see showEvent)
    if (m_nativeReady) Platform::setDarkTitle(this, t.dark);
    const QColor c = t.text;
    m_actions["open_file"]->setIcon(Icons::make("open", c));
    m_actions["open_folder"]->setIcon(Icons::make("folder", c));
    m_actions["open_url"]->setIcon(Icons::make("link", c));
    m_actions["play_pause"]->setIcon(Icons::make("play", c));
    m_actions["stop"]->setIcon(Icons::make("stop", c));
    m_actions["previous"]->setIcon(Icons::make("prev", c));
    m_actions["next"]->setIcon(Icons::make("next", c));
    m_actions["fullscreen"]->setIcon(Icons::make("fullscreen", c));
    m_actions["playlist"]->setIcon(Icons::make("playlist", c));
    m_actions["settings"]->setIcon(Icons::make("settings", c));
    m_actions["theme_studio"]->setIcon(Icons::make("theme", c));
    m_actions["media_info"]->setIcon(Icons::make("info", c));
    m_actions["favorite"]->setIcon(Icons::make("star", c));
    m_actions["about"]->setIcon(QIcon(":/brand/logo.png"));
    m_root->update();
    m_stage->update();
    if (m_stage->placeholder) m_stage->placeholder->update();
    updateMediaActions();
}

void MainWindow::buildMenus() {
    QMenuBar *mb = m_title->menuBar();
    mb->clear();
    const QList<QPair<QString, const char *>> menus = {
        {"file", "menu.file"}, {"view", "menu.view"}, {"play", "menu.play"},
        {"navigate", "menu.navigate"}, {"favorites", "menu.favorites"}, {"help", "menu.help"}};
    for (const auto &m : menus) {
        QMenu *menu = mb->addMenu(T(m.second));
        const QString which = m.first;
        connect(menu, &QMenu::aboutToShow, this, [this, menu, which] { menu->clear(); fillMenu(menu, which); });
        fillMenu(menu, which);   // so keyboard accelerators exist before first open
    }
}

void MainWindow::fillMenu(QMenu *menu, const QString &which) {
    auto A = [this](const char *id) { return m_actions.value(id); };
    const bool has = m_mpv->hasMedia();

    if (which == "file") {
        menu->addAction(A("open_file"));
        menu->addAction(A("open_folder"));
        menu->addAction(A("open_url"));
        QMenu *disc = menu->addMenu(T("menu.disc"));
        disc->addAction(T("disc.dvd"), this, [this] { openDisc("dvd"); });
        disc->addAction(T("disc.bluray"), this, [this] { openDisc("bd"); });
        disc->addAction(T("disc.vcd"), this, [this] { openDisc("vcd"); });
        disc->addAction(T("disc.cd"), this, [this] { openDisc("cdda"); });
        QMenu *recent = menu->addMenu(T("menu.recent"));
        updateRecentMenu(recent);
        menu->addSeparator();
        menu->addAction(A("open_subtitle"));
        menu->addAction(A("open_audio_file"));
        menu->addAction(A("search_subtitles"));
        menu->addSeparator();
        menu->addAction(A("load_playlist"));
        menu->addAction(A("save_playlist"));
        menu->addSeparator();
        menu->addAction(A("media_info"));
        menu->addAction(A("close_file"));
        menu->addSeparator();
        menu->addAction(A("exit"));
    } else if (which == "view") {
        menu->addAction(A("fullscreen"));
        menu->addAction(A("always_on_top"));
        menu->addAction(A("playlist"));
        menu->addSeparator();
        QMenu *theme = menu->addMenu(Icons::make("theme", ThemeManager::instance()->current().text), T("menu.theme"));
        auto *tg = new QActionGroup(theme);
        for (const Theme &t : ThemeManager::instance()->allThemes()) {
            QAction *a = theme->addAction(t.name(I18n::instance()->isRtl()));
            a->setCheckable(true);
            a->setChecked(t.id == ThemeManager::instance()->current().id);
            tg->addAction(a);
            const QString id = t.id;
            connect(a, &QAction::triggered, this, [id] { ThemeManager::instance()->apply(id); });
        }
        theme->addSeparator();
        theme->addAction(A("theme_studio"));
        QMenu *lang = menu->addMenu(T("menu.language"));
        for (const auto &l : QList<QPair<QString, QString>>{{"ar", "العربية"}, {"en", "English"}}) {
            QAction *a = lang->addAction(l.second);
            a->setCheckable(true);
            a->setChecked(I18n::instance()->language() == l.first);
            const QString code = l.first;
            connect(a, &QAction::triggered, this, [code] {
                AppSettings::instance()->setValue("ui/language", code);
                I18n::instance()->setLanguage(code);
            });
        }
        menu->addSeparator();
        QMenu *video = menu->addMenu(T("menu.video"));
        QMenu *aspect = video->addMenu(T("menu.aspect"));
        auto *ag = new QActionGroup(aspect);
        for (const QString &r : QStringList{"no", "16:9", "4:3", "21:9", "2.35:1", "1.85:1", "5:4", "1:1", "stretch"}) {
            QAction *a = aspect->addAction(r == "no" ? T("aspect.default") : r == "stretch" ? T("aspect.stretch") : r);
            a->setCheckable(true);
            a->setChecked(m_aspect == r);
            ag->addAction(a);
            connect(a, &QAction::triggered, this, [this, r] {
                m_aspect = r;
                applyVideoFilters();
                osd(T("menu.aspect") + ": " + (r == "no" ? T("aspect.default") : r == "stretch" ? T("aspect.stretch") : r));
            });
        }
        video->addAction(A("crop_fill"));
        video->addSeparator();
        video->addAction(A("zoom_in"));
        video->addAction(A("zoom_out"));
        video->addAction(A("zoom_reset"));
        video->addSeparator();
        video->addAction(A("rotate"));
        video->addAction(A("flip_h"));
        video->addAction(A("flip_v"));
        video->addAction(A("deinterlace"));
        QMenu *adj = video->addMenu(T("menu.adjust"));
        for (const char *id : {"brightness_up", "brightness_down", "contrast_up", "contrast_down", "saturation_up",
                               "saturation_down", "gamma_up", "gamma_down"}) adj->addAction(A(id));
        adj->addSeparator();
        adj->addAction(A("filters_reset"));
        video->addSeparator();
        video->addAction(A("screenshot"));
        video->addAction(A("screenshot_folder"));
        menu->addSeparator();
        menu->addAction(A("settings"));
    } else if (which == "play") {
        menu->addAction(A("play_pause"));
        menu->addAction(A("stop"));
        menu->addAction(A("frame_step"));
        menu->addAction(A("frame_back"));
        menu->addSeparator();
        QMenu *speed = menu->addMenu(Icons::make("speed", ThemeManager::instance()->current().text), T("menu.speed"));
        auto *sg = new QActionGroup(speed);
        for (double s : {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0}) {
            QAction *a = speed->addAction(QString::number(s) + "×");
            a->setCheckable(true);
            a->setChecked(qFuzzyCompare(m_mpv->speed(), s));
            sg->addAction(a);
            connect(a, &QAction::triggered, this, [this, s] { changeSpeed(s, true); });
        }
        speed->addSeparator();
        speed->addAction(A("speed_up"));
        speed->addAction(A("speed_down"));
        speed->addAction(A("speed_reset"));
        QMenu *ab = menu->addMenu(T("menu.abLoop"));
        ab->addAction(A("ab_set_a"));
        ab->addAction(A("ab_set_b"));
        ab->addAction(A("ab_clear"));
        QMenu *rep = menu->addMenu(T("act.repeatMode"));
        auto *rg = new QActionGroup(rep);
        const QList<QPair<int, const char *>> reps = {{0, "repeat.off"}, {1, "repeat.one"}, {2, "repeat.all"}};
        for (const auto &r : reps) {
            QAction *a = rep->addAction(T(r.second));
            a->setCheckable(true);
            a->setChecked(int(m_playlist->repeat()) == r.first);
            rg->addAction(a);
            const int mode = r.first;
            connect(a, &QAction::triggered, this, [this, mode] {
                m_playlist->setRepeat(Playlist::Repeat(mode));
                AppSettings::instance()->setValue("play/repeat", mode == 1 ? "one" : mode == 2 ? "all" : "off");
                m_mpv->setLoopFile(mode == 1);
            });
        }
        menu->addAction(A("shuffle"));
        menu->addSeparator();
        QMenu *audio = menu->addMenu(Icons::make("audio", ThemeManager::instance()->current().text), T("menu.audioTrack"));
        auto *audg = new QActionGroup(audio);
        bool anyAudio = false;
        for (const MediaTrack &t : m_mpv->tracks()) {
            if (t.type != "audio") continue;
            anyAudio = true;
            QAction *a = audio->addAction(ltr(t.label()));
            a->setCheckable(true);
            a->setChecked(t.selected);
            audg->addAction(a);
            const int id = t.id;
            connect(a, &QAction::triggered, this, [this, id] { m_mpv->setAudioTrack(id); });
        }
        if (!anyAudio) audio->addAction(T("menu.none"))->setEnabled(false);
        audio->addSeparator();
        audio->addAction(A("open_audio_file"));
        audio->addAction(A("audio_delay_plus"));
        audio->addAction(A("audio_delay_minus"));
        audio->addAction(A("audio_delay_reset"));

        QMenu *subs = menu->addMenu(Icons::make("subtitles", ThemeManager::instance()->current().text), T("menu.subtitleTrack"));
        auto *subg = new QActionGroup(subs);
        QAction *off = subs->addAction(T("menu.off"));
        off->setCheckable(true);
        subg->addAction(off);
        bool anySel = false;
        QMenu *secondary = new QMenu(T("menu.secondarySub"), subs);
        const int secId = m_mpv->getProperty("secondary-sid").toInt();
        QAction *secOff = secondary->addAction(T("menu.off"));
        connect(secOff, &QAction::triggered, this, [this] { m_mpv->setSecondarySubTrack(0); });
        for (const MediaTrack &t : m_mpv->tracks()) {
            if (t.type != "sub") continue;
            QAction *a = subs->addAction(ltr(t.label()));
            a->setCheckable(true);
            a->setChecked(t.selected && t.id != secId);
            anySel |= a->isChecked();
            subg->addAction(a);
            const int id = t.id;
            connect(a, &QAction::triggered, this, [this, id] { m_mpv->setSubTrack(id); });
            QAction *s2 = secondary->addAction(ltr(t.label()));
            s2->setCheckable(true);
            s2->setChecked(t.id == secId);
            connect(s2, &QAction::triggered, this, [this, id] { m_mpv->setSecondarySubTrack(id); });
        }
        off->setChecked(!anySel);
        connect(off, &QAction::triggered, this, [this] { m_mpv->setSubTrack(0); });
        subs->addMenu(secondary);
        subs->addSeparator();
        subs->addAction(A("open_subtitle"));
        subs->addAction(A("search_subtitles"));
        subs->addAction(A("sub_delay_plus"));
        subs->addAction(A("sub_delay_minus"));
        subs->addAction(A("sub_delay_reset"));
        subs->addAction(A("sub_size_up"));
        subs->addAction(A("sub_size_down"));
        menu->addSeparator();
        menu->addAction(A("vol_up"));
        menu->addAction(A("vol_down"));
        menu->addAction(A("mute"));
    } else if (which == "navigate") {
        menu->addAction(A("previous"));
        menu->addAction(A("next"));
        menu->addSeparator();
        menu->addAction(A("seek_back"));
        menu->addAction(A("seek_fwd"));
        menu->addAction(A("seek_back_large"));
        menu->addAction(A("seek_fwd_large"));
        menu->addAction(A("go_start"));
        menu->addAction(A("jump_to"));
        const QVariantList chapters = m_mpv->getProperty("chapter-list").toList();
        QMenu *ch = menu->addMenu(T("menu.chapters"));
        ch->setEnabled(!chapters.isEmpty());
        for (int i = 0; i < chapters.size(); ++i) {
            const QVariantMap c = chapters[i].toMap();
            const double time = c.value("time").toDouble();
            const QString title = c.value("title").toString();
            ch->addAction(QString("%1  %2").arg(ControlBar::formatTime(time), title.isEmpty() ? T("menu.chapter") + " " + QString::number(i + 1) : title),
                          this, [this, time] { m_mpv->seekAbsolute(time); });
        }
    } else if (which == "favorites") {
        menu->addAction(A("favorite"));
        menu->addSeparator();
        updateFavoritesMenu(menu);
        menu->addSeparator();
        menu->addAction(A("clear_favorites"));
    } else if (which == "help") {
        menu->addAction(A("shortcuts_help"));
        menu->addAction(A("open_log"));
        menu->addAction(A("check_updates"));
        menu->addAction(A("telegram"));
        menu->addSeparator();
        menu->addAction(A("about"));
    }
    Q_UNUSED(has);
}

void MainWindow::updateRecentMenu(QMenu *m) {
    const QStringList recent = Library::instance()->recent();
    if (recent.isEmpty()) m->addAction(T("menu.empty"))->setEnabled(false);
    for (const QString &p : recent) {
        const QString name = p.contains("://") ? p : QFileInfo(p).fileName();
        m->addAction(ltr(name), this, [this, p] { openPaths({p}, false); })->setToolTip(p);
    }
    m->addSeparator();
    m->addAction(m_actions["clear_recent"]);
}

void MainWindow::updateFavoritesMenu(QMenu *m) {
    const QStringList fav = Library::instance()->favorites();
    if (fav.isEmpty()) m->addAction(T("menu.empty"))->setEnabled(false);
    const QColor c = ThemeManager::instance()->current().accent;
    for (const QString &p : fav) {
        const QString name = p.contains("://") ? p : QFileInfo(p).fileName();
        m->addAction(Icons::make("star-filled", c), ltr(name), this, [this, p] { openPaths({p}, false); })->setToolTip(p);
    }
}

QMenu *MainWindow::buildContextMenu() {
    auto *m = new QMenu(this);
    m->setAttribute(Qt::WA_DeleteOnClose);
    m->addAction(m_actions["play_pause"]);
    m->addAction(m_actions["stop"]);
    m->addSeparator();
    for (const auto &p : QList<QPair<QString, const char *>>{{"file", "menu.file"}, {"play", "menu.play"},
                                                             {"view", "menu.view"}, {"navigate", "menu.navigate"},
                                                             {"favorites", "menu.favorites"}}) {
        QMenu *sub = m->addMenu(T(p.second));
        fillMenu(sub, p.first);
    }
    m->addSeparator();
    m->addAction(m_actions["fullscreen"]);
    m->addAction(m_actions["media_info"]);
    m->addAction(m_actions["settings"]);
    return m;
}

void MainWindow::updateMediaActions() {
    const bool has = m_mpv->hasMedia();
    m_actions["playlist"]->setChecked(m_playlistPanel->isVisible());
    m_actions["shuffle"]->setChecked(m_playlist->shuffle());
    m_actions["favorite"]->setChecked(has && Library::instance()->isFavorite(m_currentPath));
    m_actions["favorite"]->setText(has && Library::instance()->isFavorite(m_currentPath) ? T("act.removeFavorite") : T("act.addFavorite"));
    m_actions["always_on_top"]->setChecked(windowFlags() & Qt::WindowStaysOnTopHint);
    m_actions["flip_h"]->setChecked(m_hflip);
    m_actions["flip_v"]->setChecked(m_vflip);
    for (const char *id : {"media_info", "close_file", "screenshot", "frame_step", "frame_back", "jump_to", "favorite",
                           "ab_loop", "ab_set_a", "ab_set_b", "open_subtitle", "open_audio_file", "search_subtitles"})
        m_actions[id]->setEnabled(has);
}

// ------------------------------------------------------------- dispatcher
void MainWindow::onControlAction(const QString &id) {
    if (id == "speed") {
        QMenu m(this);
        fillMenu(&m, "play");
        // show just the speed submenu
        for (QAction *a : m.actions())
            if (a->menu() && a->text() == T("menu.speed")) { a->menu()->exec(QCursor::pos()); return; }
        return;
    }
    if (id == "audio" || id == "subtitles") {
        QMenu m(this);
        fillMenu(&m, "play");
        const QString want = id == "audio" ? T("menu.audioTrack") : T("menu.subtitleTrack");
        for (QAction *a : m.actions())
            if (a->menu() && a->text() == want) { a->menu()->exec(QCursor::pos()); return; }
        return;
    }
    trigger(id);
}

void MainWindow::trigger(const QString &id) {
    AppSettings *s = AppSettings::instance();
    const bool has = m_mpv->hasMedia();
    if (id == "open_file") openFileDialog();
    else if (id == "open_folder") openFolderDialog();
    else if (id == "open_url") openUrlDialog();
    else if (id == "open_disc") openDisc("dvd");
    else if (id == "open_subtitle") openSubtitleDialog();
    else if (id == "open_audio_file") openAudioFileDialog();
    else if (id == "search_subtitles") searchSubtitlesOnline();
    else if (id == "save_playlist") savePlaylistDialog();
    else if (id == "load_playlist") loadPlaylistDialog();
    else if (id == "close_file") closeFile();
    else if (id == "exit") close();
    else if (id == "play_pause") {
        if (!has) {
            if (m_playlist->count() > 0) playIndex(qMax(0, m_playlist->current()));
            else openFileDialog();
        } else {
            if (m_mpv->getProperty("eof-reached").toBool()) m_mpv->seekAbsolute(0);
            m_mpv->togglePause();
            osd(m_mpv->isPaused() ? T("osd.paused") : T("osd.playing"));
        }
    }
    else if (id == "stop") stopPlayback();
    else if (id == "previous") playPrevious();
    else if (id == "next") playNext(true);
    else if (id == "seek_back") { m_mpv->seekRelative(-s->intValue("play/seekSmall")); osd(QString("⟲ %1s").arg(s->intValue("play/seekSmall"))); }
    else if (id == "seek_fwd") { m_mpv->seekRelative(s->intValue("play/seekSmall")); osd(QString("⟳ %1s").arg(s->intValue("play/seekSmall"))); }
    else if (id == "seek_back_large") { m_mpv->seekRelative(-s->intValue("play/seekLarge")); osd(QString("⟲ %1s").arg(s->intValue("play/seekLarge"))); }
    else if (id == "seek_fwd_large") { m_mpv->seekRelative(s->intValue("play/seekLarge")); osd(QString("⟳ %1s").arg(s->intValue("play/seekLarge"))); }
    else if (id == "go_start") m_mpv->seekAbsolute(0);
    else if (id == "frame_step") { m_mpv->frameStep(); osd(T("act.frameStep")); }
    else if (id == "frame_back") { m_mpv->frameBackStep(); osd(T("act.frameBack")); }
    else if (id == "jump_to") jumpToDialog();
    else if (id == "vol_up") { m_mpv->setVolume(m_mpv->volume() + 5); osd(T("act.volume") + QString(": %1%").arg(qMin(200, m_mpv->volume() + 5))); }
    else if (id == "vol_down") { m_mpv->setVolume(m_mpv->volume() - 5); osd(T("act.volume") + QString(": %1%").arg(qMax(0, m_mpv->volume() - 5))); }
    else if (id == "mute") { m_mpv->setMuted(!m_mpv->muted()); osd(!m_mpv->muted() ? T("osd.muted") : T("osd.unmuted")); }
    else if (id == "speed_up") changeSpeed(s->doubleValue("play/speedStep"));
    else if (id == "speed_down") changeSpeed(-s->doubleValue("play/speedStep"));
    else if (id == "speed_reset") changeSpeed(1.0, true);
    else if (id == "ab_loop") cycleAbLoop();
    else if (id == "ab_set_a") { m_mpv->setAbLoopA(); osd(T("act.abSetA")); }
    else if (id == "ab_set_b") { m_mpv->setAbLoopB(); osd(T("act.abSetB")); }
    else if (id == "ab_clear") { m_mpv->clearAbLoop(); osd(T("act.abClear")); }
    else if (id == "repeat") cycleRepeat();
    else if (id == "shuffle") {
        m_playlist->setShuffle(!m_playlist->shuffle());
        s->setValue("play/shuffle", m_playlist->shuffle());
        osd(T("act.shuffle") + ": " + (m_playlist->shuffle() ? T("osd.on") : T("osd.off")));
    }
    else if (id == "cycle_audio") cycleTrack("audio");
    else if (id == "cycle_sub") cycleTrack("sub");
    else if (id == "sub_delay_minus") adjustProperty("sub-delay", -0.1, "osd.subDelay", 1000, " ms");
    else if (id == "sub_delay_plus") adjustProperty("sub-delay", 0.1, "osd.subDelay", 1000, " ms");
    else if (id == "sub_delay_reset") { m_mpv->setProperty("sub-delay", 0.0); osd(T("osd.subDelay") + ": 0 ms"); }
    else if (id == "audio_delay_minus") adjustProperty("audio-delay", -0.05, "osd.audioDelay", 1000, " ms");
    else if (id == "audio_delay_plus") adjustProperty("audio-delay", 0.05, "osd.audioDelay", 1000, " ms");
    else if (id == "audio_delay_reset") { m_mpv->setProperty("audio-delay", 0.0); osd(T("osd.audioDelay") + ": 0 ms"); }
    else if (id == "sub_size_up") adjustProperty("sub-scale", 0.1, "osd.subScale", 100, "%");
    else if (id == "sub_size_down") adjustProperty("sub-scale", -0.1, "osd.subScale", 100, "%");
    else if (id == "fullscreen" || id == "fullscreen_alt") toggleFullscreen();
    else if (id == "always_on_top") setAlwaysOnTop(!(windowFlags() & Qt::WindowStaysOnTopHint));
    else if (id == "zoom_in") { m_zoom = qMin(m_zoom + 0.1, 3.0); applyVideoFilters(); osd(T("osd.zoom") + QString(": %1%").arg(qRound(std::pow(2.0, m_zoom) * 100))); }
    else if (id == "zoom_out") { m_zoom = qMax(m_zoom - 0.1, -2.0); applyVideoFilters(); osd(T("osd.zoom") + QString(": %1%").arg(qRound(std::pow(2.0, m_zoom) * 100))); }
    else if (id == "zoom_reset") { m_zoom = 0; applyVideoFilters(); osd(T("osd.zoom") + ": 100%"); }
    else if (id == "rotate") { m_rotation = (m_rotation + 90) % 360; applyVideoFilters(); osd(T("act.rotate") + QString(": %1°").arg(m_rotation)); }
    else if (id == "flip_h") { m_hflip = !m_hflip; applyVideoFilters(); osd(T("act.flipH")); }
    else if (id == "flip_v") { m_vflip = !m_vflip; applyVideoFilters(); osd(T("act.flipV")); }
    else if (id == "crop_fill") {
        const bool on = m_mpv->getProperty("panscan").toDouble() < 0.5;
        m_mpv->setProperty("panscan", on ? 1.0 : 0.0);
        m_actions["crop_fill"]->setChecked(on);
        osd(T("act.cropFill") + ": " + (on ? T("osd.on") : T("osd.off")));
    }
    else if (id == "deinterlace") {
        const bool on = !m_mpv->getProperty("deinterlace").toBool();
        m_mpv->setProperty("deinterlace", on);
        m_actions["deinterlace"]->setChecked(on);
        osd(T("act.deinterlace") + ": " + (on ? T("osd.on") : T("osd.off")));
    }
    else if (id == "brightness_up") adjustProperty("brightness", 5, "osd.brightness");
    else if (id == "brightness_down") adjustProperty("brightness", -5, "osd.brightness");
    else if (id == "contrast_up") adjustProperty("contrast", 5, "osd.contrast");
    else if (id == "contrast_down") adjustProperty("contrast", -5, "osd.contrast");
    else if (id == "saturation_up") adjustProperty("saturation", 5, "osd.saturation");
    else if (id == "saturation_down") adjustProperty("saturation", -5, "osd.saturation");
    else if (id == "gamma_up") adjustProperty("gamma", 5, "osd.gamma");
    else if (id == "gamma_down") adjustProperty("gamma", -5, "osd.gamma");
    else if (id == "filters_reset") {
        for (const char *p : {"brightness", "contrast", "saturation", "gamma", "hue"}) m_mpv->setProperty(p, 0LL);
        m_zoom = 0; m_rotation = 0; m_hflip = m_vflip = false; m_aspect = "no";
        m_mpv->setProperty("panscan", 0.0);
        applyVideoFilters();
        osd(T("act.filtersReset"));
    }
    else if (id == "screenshot") {
        if (has) { m_mpv->screenshot(s->stringValue("video/screenshotDir")); osd(T("osd.screenshot")); }
    }
    else if (id == "screenshot_folder") {
        QDir().mkpath(s->stringValue("video/screenshotDir"));
        QDesktopServices::openUrl(QUrl::fromLocalFile(s->stringValue("video/screenshotDir")));
    }
    else if (id == "media_info") showMediaInfo();
    else if (id == "playlist") togglePlaylist();
    else if (id == "favorite") {
        if (has) {
            Library::instance()->toggleFavorite(m_currentPath);
            osd(Library::instance()->isFavorite(m_currentPath) ? T("osd.favAdded") : T("osd.favRemoved"));
        }
    }
    else if (id == "settings") showSettings();
    else if (id == "theme_studio") showThemeStudio();
    else if (id == "about") showAbout();
    else if (id == "shortcuts_help") showSettings("shortcuts");
    else if (id == "open_log") QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(Log::filePath()).absolutePath()));
    else if (id == "check_updates") checkForUpdates(false);
    else if (id == "telegram") QDesktopServices::openUrl(QUrl(kTelegram));
    else if (id == "clear_recent") Library::instance()->clearHistory();
    else if (id == "clear_favorites") {
        if (QMessageBox::question(this, "iBRA PLAYER", T("q.clearFavorites")) == QMessageBox::Yes)
            Library::instance()->clearFavorites();
    }
    updateMediaActions();
}

// --------------------------------------------------------------- playback
void MainWindow::openPaths(const QStringList &paths, bool append) {
    // A lone subtitle dropped on a playing video is attached, not queued
    if (paths.size() == 1 && Playlist::isSubtitleFile(paths.first()) && m_mpv->hasMedia()) {
        m_mpv->addSubtitle(paths.first());
        osd(T("osd.subLoaded") + ": " + ltr(QFileInfo(paths.first()).fileName()));
        return;
    }
    const QVector<PlaylistItem> items = Playlist::expand(paths);
    if (items.isEmpty()) {
        osd(T("osd.nothingToPlay"), 2500);
        return;
    }
    if (append && m_playlist->count() > 0) {
        m_playlist->add(items);
        osd(T("osd.added").arg(items.size()));
        if (!m_mpv->hasMedia()) playIndex(m_playlist->count() - items.size());
        return;
    }
    // Single file opened: optionally queue the rest of its folder (MPC-HC style)
    QVector<PlaylistItem> list = items;
    int start = 0;
    if (items.size() == 1 && !items.first().path.contains("://") && AppSettings::instance()->boolValue("playlist/addFolderSiblings")) {
        const QString file = items.first().path;
        list.clear();
        const QStringList sib = Playlist::scanFolder(QFileInfo(file).absolutePath(), false);
        for (const QString &f : sib) list.append({f, {}, 0});
        start = qMax(0, sib.indexOf(file));
        if (list.isEmpty()) { list = items; start = 0; }
    }
    m_playlist->replace(list);
    if (m_playlist->shuffle() && items.size() > 1) start = m_playlist->nextIndex(true);
    playIndex(start);
}

void MainWindow::playIndex(int index) {
    if (index < 0 || index >= m_playlist->count()) return;
    saveResume();
    m_playlist->setCurrent(index);
    const PlaylistItem &it = m_playlist->items()[index];
    m_currentPath = it.path;
    double start = 0;
    if (AppSettings::instance()->boolValue("play/resume")) start = Library::instance()->resumePosition(it.path);
    m_mpv->loadFile(it.path, start);
    if (start > 0) osd(T("osd.resumed").arg(ltr(ControlBar::formatTime(start))), 2500);
    Library::instance()->addRecent(it.path);
    Log::write("info", "open " + it.path);
    if (auto *ph = static_cast<Placeholder *>(m_stage->placeholder)) { ph->audioOnly = false; ph->title = it.displayName(); }
    updateTitle();
}

void MainWindow::playNext(bool user) {
    const int n = m_playlist->nextIndex(user);
    if (n >= 0) playIndex(n);
    else if (user) osd(T("osd.endOfPlaylist"));
}

void MainWindow::playPrevious() {
    if (m_mpv->hasMedia() && m_mpv->position() > 5) { m_mpv->seekAbsolute(0); return; }
    const int p = m_playlist->previousIndex();
    if (p >= 0) playIndex(p);
}

void MainWindow::stopPlayback() {
    saveResume();
    m_mpv->stop();
    m_controls->setPlaying(false);
    m_controls->setTimes(0, 0);
    m_stage->placeholder->show();
    static_cast<Placeholder *>(m_stage->placeholder)->audioOnly = false;
    m_stage->placeholder->update();
    showControls();
    updateMediaActions();
    osd(T("act.stop"));
}

void MainWindow::closeFile() {
    stopPlayback();
    m_playlist->setCurrent(-1);
    m_currentPath.clear();
    updateTitle();
}

void MainWindow::saveResume() {
    if (m_mpv->hasMedia() && !m_currentPath.isEmpty() && !m_currentPath.contains("://"))
        Library::instance()->setResume(m_currentPath, m_mpv->position(), m_mpv->duration());
}

void MainWindow::onFileLoaded() {
    m_controls->setPlaying(!m_mpv->isPaused());
    m_hideTimer.start(AppSettings::instance()->intValue("ui/autoHideMs"));
    onTracksChanged();
    updateTitle();
    updateMediaActions();
    m_mpv->setLoopFile(m_playlist->repeat() == Playlist::RepeatOne);
    if (!AppSettings::instance()->boolValue("play/autoPlayOnOpen")) m_mpv->setPaused(true);
    // Remember subtitle/audio delays per session only; reset per file like MPC-HC
    const QString hw = m_mpv->getPropertyString("hwdec-current");
    Log::write("info", QString("loaded: %1 / %2, hwdec=%3").arg(m_mpv->getPropertyString("video-codec"),
                                                             m_mpv->getPropertyString("audio-codec-name"),
                                                             hw.isEmpty() ? "no" : hw));
}

void MainWindow::onEndOfFile(bool eof, const QString &error) {
    if (!error.isEmpty()) {
        // Skip unplayable entries instead of stalling the playlist
        if (m_playlist->count() > 1 && AppSettings::instance()->boolValue("play/autoNext"))
            QTimer::singleShot(1200, this, [this] { playNext(false); });
        m_stage->placeholder->show();
        return;
    }
    if (!eof) return;
    Library::instance()->setResume(m_currentPath, 0, m_mpv->duration());   // finished: drop resume point
    if (AppSettings::instance()->boolValue("play/autoNext") || m_playlist->repeat() != Playlist::RepeatOff) {
        const int n = m_playlist->nextIndex(false);
        if (n >= 0) { QTimer::singleShot(0, this, [this, n] { playIndex(n); }); return; }
    }
    m_controls->setPlaying(false);
    showControls();
}

void MainWindow::onPosition(double pos) {
    if (!m_controls->seek()->isDragging()) m_controls->setTimes(pos, m_mpv->duration());
}

void MainWindow::onTracksChanged() {
    bool video = false;
    for (const MediaTrack &t : m_mpv->tracks()) if (t.type == "video") video = true;
    auto *ph = static_cast<Placeholder *>(m_stage->placeholder);
    if (m_mpv->hasMedia() && !video) {
        ph->audioOnly = true;
        ph->title = m_playlist->currentItem() ? m_playlist->currentItem()->displayName() : m_mpv->getPropertyString("media-title");
        ph->show();
        ph->update();
    } else if (m_mpv->hasMedia()) {
        ph->hide();
    }
    m_controls->raise();
    m_osd->raise();
}

void MainWindow::updateTitle() {
    QString t;
    if (m_mpv->hasMedia() || m_playlist->currentItem()) {
        const QString mt = m_mpv->getPropertyString("media-title");
        t = !mt.isEmpty() ? mt : (m_playlist->currentItem() ? m_playlist->currentItem()->displayName() : QString());
    }
    m_title->setMediaTitle(t);
    setWindowTitle(t.isEmpty() ? QString("iBRA PLAYER") : t + " — iBRA PLAYER");
}

// ---------------------------------------------------------------- dialogs
static QString mediaFilter() {
    QStringList globs;
    for (const QString &e : Playlist::mediaExtensions()) globs << "*." + e;
    QStringList pl;
    for (const QString &e : Playlist::playlistExtensions()) pl << "*." + e;
    return QString("%1 (%2 %3);;%4 (%3);;%5 (*)").arg(T("dlg.mediaFiles"), globs.join(' '), pl.join(' '),
                                                      T("dlg.playlists"), T("dlg.allFiles"));
}

static QString lastDir() {
    return AppSettings::instance()->raw()->value("window/lastDir",
                                                 QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString();
}
static void rememberDir(const QString &file) {
    AppSettings::instance()->raw()->setValue("window/lastDir", QFileInfo(file).absolutePath());
}

void MainWindow::openFileDialog(bool append) {
    const QStringList files = QFileDialog::getOpenFileNames(this, T("act.openFile"), lastDir(), mediaFilter());
    if (files.isEmpty()) return;
    rememberDir(files.first());
    openPaths(files, append || (QApplication::keyboardModifiers() & Qt::ControlModifier));
}

void MainWindow::openFolderDialog(bool append) {
    const QString dir = QFileDialog::getExistingDirectory(this, T("act.openFolder"), lastDir());
    if (dir.isEmpty()) return;
    AppSettings::instance()->raw()->setValue("window/lastDir", dir);
    openPaths({dir}, append);
}

void MainWindow::openUrlDialog() {
    bool ok = false;
    const QString url = QInputDialog::getText(this, T("act.openUrl"), T("dlg.urlPrompt"), QLineEdit::Normal,
                                              QString(), &ok).trimmed();
    if (!ok || url.isEmpty()) return;
    static const QRegularExpression scheme("^(https?|rtsp|rtsps|rtmp|rtmps|mms|mmsh|srt|udp|rtp|ftp|smb)://", QRegularExpression::CaseInsensitiveOption);
    if (!scheme.match(url).hasMatch()) {
        QMessageBox::warning(this, "iBRA PLAYER", T("dlg.badUrl"));
        return;
    }
    openPaths({url}, m_playlist->count() > 0 && (QApplication::keyboardModifiers() & Qt::ControlModifier));
}

void MainWindow::openDisc(const QString &kind) {
    QString device;
    if (kind == "dvd" || kind == "bd") {
        QMessageBox box(this);
        box.setWindowTitle("iBRA PLAYER");
        box.setText(kind == "dvd" ? T("disc.dvdPrompt") : T("disc.bdPrompt"));
        QPushButton *folder = box.addButton(T("disc.folder"), QMessageBox::AcceptRole);
        QPushButton *iso = box.addButton(T("disc.iso"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == folder) device = QFileDialog::getExistingDirectory(this, T("disc.folder"), lastDir());
        else if (box.clickedButton() == iso) device = QFileDialog::getOpenFileName(this, T("disc.iso"), lastDir(), "ISO (*.iso)");
        else return;
    } else {
        device = QFileDialog::getExistingDirectory(this, T("disc.drive"), QDir::rootPath());
    }
    if (device.isEmpty()) return;
    // For a DVD folder, mpv wants the folder that contains VIDEO_TS
    if (kind == "dvd" && QFileInfo(device).fileName().compare("VIDEO_TS", Qt::CaseInsensitive) == 0)
        device = QFileInfo(device).absolutePath();
    if (kind == "bd" && QFileInfo(device).fileName().compare("BDMV", Qt::CaseInsensitive) == 0)
        device = QFileInfo(device).absolutePath();
    const QString opt = kind == "dvd" ? "dvd-device" : kind == "bd" ? "bluray-device" : "cdrom-device";
    m_mpv->setProperty(opt, device);
    const QString url = kind + "://";
    m_playlist->replace({{url, kind == "dvd" ? "DVD" : kind == "bd" ? "Blu-ray" : kind.toUpper(), 0}});
    playIndex(0);
}

void MainWindow::openSubtitleDialog() {
    QStringList globs;
    for (const QString &e : Playlist::subtitleExtensions()) globs << "*." + e;
    const QString start = m_currentPath.contains("://") ? lastDir() : QFileInfo(m_currentPath).absolutePath();
    const QStringList files = QFileDialog::getOpenFileNames(this, T("act.loadSubtitle"), start,
                                                            QString("%1 (%2);;%3 (*)").arg(T("dlg.subtitles"), globs.join(' '), T("dlg.allFiles")));
    for (const QString &f : files) m_mpv->addSubtitle(f);
    if (!files.isEmpty()) osd(T("osd.subLoaded") + ": " + ltr(QFileInfo(files.last()).fileName()));
}

void MainWindow::openAudioFileDialog() {
    const QString start = m_currentPath.contains("://") ? lastDir() : QFileInfo(m_currentPath).absolutePath();
    const QString f = QFileDialog::getOpenFileName(this, T("act.loadAudio"), start, mediaFilter());
    if (f.isEmpty()) return;
    m_mpv->addAudioFile(f);
    osd(T("act.loadAudio") + ": " + ltr(QFileInfo(f).fileName()));
}

void MainWindow::loadPlaylistDialog() {
    const QString f = QFileDialog::getOpenFileName(this, T("pl.load"), lastDir(),
                                                   T("dlg.playlists") + " (*.m3u *.m3u8 *.pls)");
    if (f.isEmpty()) return;
    rememberDir(f);
    openPaths({f}, false);
}

void MainWindow::savePlaylistDialog() {
    if (m_playlist->count() == 0) { osd(T("osd.playlistEmpty")); return; }
    QString selected;
    const QString f = QFileDialog::getSaveFileName(this, T("act.savePlaylist"), lastDir() + "/playlist.m3u8",
                                                   "M3U8 (*.m3u8);;M3U (*.m3u);;PLS (*.pls)", &selected);
    if (f.isEmpty()) return;
    QString file = f;
    if (QFileInfo(file).suffix().isEmpty()) file += selected.startsWith("PLS") ? ".pls" : selected.startsWith("M3U8") ? ".m3u8" : ".m3u";
    if (m_playlist->saveTo(file)) osd(T("osd.playlistSaved"));
    else QMessageBox::warning(this, "iBRA PLAYER", T("dlg.saveFailed"));
}

void MainWindow::jumpToDialog() {
    bool ok = false;
    const QString txt = QInputDialog::getText(this, T("act.jumpTo"), T("dlg.jumpPrompt"), QLineEdit::Normal,
                                              ControlBar::formatTime(m_mpv->position(), true), &ok);
    if (!ok) return;
    const QStringList parts = txt.trimmed().split(':');
    double sec = 0;
    for (const QString &p : parts) sec = sec * 60 + p.toDouble();
    if (txt.endsWith('%')) m_mpv->seekPercent(txt.chopped(1).toDouble());
    else m_mpv->seekAbsolute(sec);
}

void MainWindow::showMediaInfo() {
    if (!m_mpv->hasMedia()) return;
    MediaInfoDialog d(m_mpv->mediaInfo(), this);
    d.exec();
}

void MainWindow::showSettings(const QString &page) {
    SettingsDialog d(this);
    connect(&d, &SettingsDialog::openThemeStudio, this, [this] { QTimer::singleShot(0, this, &MainWindow::showThemeStudio); });
    if (!page.isEmpty()) d.selectPage(page);
    d.exec();
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it)
        if (Shortcuts::find(it.key())) it.value()->setShortcut(Shortcuts::get(it.key()));
}

void MainWindow::showThemeStudio() {
    ThemeStudio d(this);
    d.exec();
}

void MainWindow::showAbout() {
    AboutDialog d(this);
    d.exec();
}

void MainWindow::searchSubtitlesOnline() {
    if (!m_mpv->hasMedia()) return;
    QString name = QFileInfo(m_currentPath).completeBaseName();
    if (m_currentPath.contains("://")) name = m_mpv->getPropertyString("media-title");
    if (QMessageBox::question(this, T("act.searchSubtitles"), T("q.subtitleSearch").arg(name)) != QMessageBox::Yes) return;
    // Clean typical release tags so the search is useful
    QString q = name;
    q.replace(QRegularExpression("[._]"), " ");
    q.remove(QRegularExpression("\\b(1080p|720p|2160p|480p|x264|x265|h264|h265|hevc|web-?dl|webrip|bluray|brrip|hdtv|aac|ac3|dts|10bit)\\b.*",
                                QRegularExpression::CaseInsensitiveOption));
    const QString url = "https://www.opensubtitles.org/en/search2/sublanguageid-ara,eng/moviename-" +
                        QString::fromUtf8(QUrl::toPercentEncoding(q.simplified()));
    QDesktopServices::openUrl(QUrl(url));
}

void MainWindow::checkForUpdates(bool silent) {
    if (!m_net) m_net = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.github.com/repos/ibrata2/ibra-player/releases/latest"));
    req.setHeader(QNetworkRequest::UserAgentHeader, "iBRA-PLAYER");   // no identifiers, no history, no file names
    req.setTransferTimeout(10000);
    QNetworkReply *r = m_net->get(req);
    connect(r, &QNetworkReply::finished, this, [this, r, silent] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            if (!silent) QMessageBox::information(this, "iBRA PLAYER", T("upd.failed"));
            return;
        }
        const QString tag = QJsonDocument::fromJson(r->readAll()).object().value("tag_name").toString();
        const QVersionNumber latest = QVersionNumber::fromString(QString(tag).remove(QRegularExpression("^[vV]")));
        const QVersionNumber mine = QVersionNumber::fromString(IBRA_VERSION);
        if (!latest.isNull() && latest > mine) {
            if (QMessageBox::question(this, "iBRA PLAYER", T("upd.available").arg(latest.toString(), mine.toString())) == QMessageBox::Yes)
                QDesktopServices::openUrl(QUrl(kTelegram));
        } else if (!silent) {
            QMessageBox::information(this, "iBRA PLAYER", T("upd.latest").arg(mine.toString()));
        }
    });
}

// ---------------------------------------------------------------- helpers
void MainWindow::changeSpeed(double d, bool absolute) {
    double s = absolute ? d : m_mpv->speed() + d;
    s = qBound(0.25, std::round(s * 100) / 100, 4.0);
    m_mpv->setSpeed(s);
    osd(T("menu.speed") + ": " + ltr(QString("%1×").arg(s)));
}

void MainWindow::adjustProperty(const QString &prop, double delta, const QString &osdKey, double scale, const QString &unit) {
    const double cur = m_mpv->getProperty(prop).toDouble();
    double v = cur + delta;
    if (prop == "sub-scale") v = qBound(0.3, v, 3.0);
    else if (prop == "brightness" || prop == "contrast" || prop == "saturation" || prop == "gamma") v = qBound(-100.0, v, 100.0);
    m_mpv->setProperty(prop, (prop == "sub-delay" || prop == "audio-delay" || prop == "sub-scale") ? QVariant(v) : QVariant(qlonglong(v)));
    osd(T(osdKey.toLatin1().constData()) + ": " + ltr(QString("%1%2").arg(qRound(v * scale)).arg(unit)));
}

void MainWindow::cycleTrack(const QString &type) {
    QList<MediaTrack> list;
    int cur = -1;
    for (const MediaTrack &t : m_mpv->tracks()) if (t.type == type) { if (t.selected) cur = list.size(); list << t; }
    if (list.isEmpty()) { osd(type == "audio" ? T("osd.noAudio") : T("osd.noSubs")); return; }
    const int next = cur + 1;
    if (type == "sub" && next >= list.size()) { m_mpv->setSubTrack(0); osd(T("menu.subtitleTrack") + ": " + T("menu.off")); return; }
    const MediaTrack &t = list[next % list.size()];
    if (type == "audio") m_mpv->setAudioTrack(t.id); else m_mpv->setSubTrack(t.id);
    osd((type == "audio" ? T("menu.audioTrack") : T("menu.subtitleTrack")) + ": " + ltr(t.label()));
}

void MainWindow::cycleAbLoop() {
    const QVariant a = m_mpv->getProperty("ab-loop-a"), b = m_mpv->getProperty("ab-loop-b");
    const bool hasA = a.typeId() == QMetaType::Double, hasB = b.typeId() == QMetaType::Double;
    if (!hasA) { m_mpv->setAbLoopA(); osd(T("act.abSetA") + " " + ControlBar::formatTime(m_mpv->position())); }
    else if (!hasB) { m_mpv->setAbLoopB(); osd(T("act.abSetB") + " " + ControlBar::formatTime(m_mpv->position())); }
    else { m_mpv->clearAbLoop(); osd(T("act.abClear")); }
}

void MainWindow::cycleRepeat() {
    const int mode = (int(m_playlist->repeat()) + 1) % 3;
    m_playlist->setRepeat(Playlist::Repeat(mode));
    m_mpv->setLoopFile(mode == 1);
    AppSettings::instance()->setValue("play/repeat", mode == 1 ? "one" : mode == 2 ? "all" : "off");
    osd(T("act.repeatMode") + ": " + T(mode == 1 ? "repeat.one" : mode == 2 ? "repeat.all" : "repeat.off"));
}

void MainWindow::osd(const QString &text, int ms) {
    if (!AppSettings::instance()->boolValue("ui/osd")) return;
    m_osd->setText(text);
    m_osd->show();
    m_osd->raise();
    m_stage->relayout();
    m_osdTimer.start(ms);
}

static void liftSubtitles(MpvWidget *mpv, QWidget *controls, bool visible) {
    // Keep subtitles above the glass control bar while it is shown, relative
    // to the user's chosen vertical position (100 = bottom).
    const int userPos = AppSettings::instance()->intValue("sub/position");
    int pos = userPos;
    if (visible && userPos > 50) {
        const double frac = (controls->height() + 14.0) / qMax(1, mpv->height());
        pos = qMin(userPos, int(100.0 * (1.0 - frac)));
    }
    mpv->setProperty("sub-pos", qlonglong(pos));
}

void MainWindow::showControls() {
    m_controls->show();
    liftSubtitles(m_mpv, m_controls, true);
    m_controls->raise();
    m_osd->raise();
    m_mpv->unsetCursor();
    m_stage->placeholder->unsetCursor();
    if (m_mpv->hasMedia() && !m_mpv->isPaused()) m_hideTimer.start(AppSettings::instance()->intValue("ui/autoHideMs"));
}

void MainWindow::hideControlsIfIdle() {
    if (!m_mpv->hasMedia() || m_mpv->isPaused()) return;
    if (m_controls->underMouse() || m_controls->seek()->isDragging() || QApplication::activePopupWidget()) {
        m_hideTimer.start(AppSettings::instance()->intValue("ui/autoHideMs"));
        return;
    }
    m_controls->hide();
    liftSubtitles(m_mpv, m_controls, false);
    if (m_mpv->underMouse()) m_mpv->setCursor(Qt::BlankCursor);
}

void MainWindow::toggleFullscreen() { setFullscreen(!m_fullscreen); }

void MainWindow::setFullscreen(bool fs) {
    if (fs == m_fullscreen) return;
    m_fullscreen = fs;
    m_title->setVisible(!fs);
    static bool playlistWasVisible = false;
    if (fs) {
        playlistWasVisible = m_playlistPanel->isVisible();
        m_playlistPanel->hide();
        m_wasMaximized = isMaximized();
        m_root->layout()->setContentsMargins(0, 0, 0, 0);
        m_stage->frame = false;
        showFullScreen();
    } else {
        m_playlistPanel->setVisible(playlistWasVisible);
        m_root->layout()->setContentsMargins(6, 4, 6, 6);
        m_stage->frame = true;
        m_wasMaximized ? showMaximized() : showNormal();
    }
    m_stage->relayout();
    liftSubtitles(m_mpv, m_controls, m_controls->isVisible());
    m_controls->setFullscreen(fs);
    showControls();
}

void MainWindow::togglePlaylist() {
    const bool v = !m_playlistPanel->isVisible();
    if (m_fullscreen && v) setFullscreen(false);
    m_playlistPanel->setVisible(v);
    m_controls->btn("playlist")->setChecked(v);
    AppSettings::instance()->setValue("playlist/visible", v);
    updateMediaActions();
}

void MainWindow::setAlwaysOnTop(bool on) {
    const bool visible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (visible) show();
    AppSettings::instance()->setValue("ui/alwaysOnTop", on);
    osd(T("act.alwaysOnTop") + ": " + (on ? T("osd.on") : T("osd.off")));
    updateMediaActions();
}

// --------------------------------------------------------------- settings -> engine
void MainWindow::applySettings(const QString &key) {
    AppSettings *s = AppSettings::instance();
    if (key.isEmpty() || key == "ui/language") I18n::instance()->setLanguage(s->stringValue("ui/language"));
    if (key.isEmpty() || key == "ui/theme")
        if (ThemeManager::instance()->current().id != s->stringValue("ui/theme")) ThemeManager::instance()->apply(s->stringValue("ui/theme"));
    if (key.isEmpty() || key.startsWith("video/") || key.startsWith("audio/") || key.startsWith("sub/") || key.startsWith("play/"))
        applyEngineSettings();
    if (m_nativeReady && (key.isEmpty() || key == "ui/mediaKeys" || key == "ui/globalMediaKeys"))
        m_mediaKeys->setEnabled(s->raw()->value("ui/globalMediaKeys", true).toBool(), this);
    if (key == "ui/alwaysOnTop" && bool(windowFlags() & Qt::WindowStaysOnTopHint) != s->boolValue("ui/alwaysOnTop"))
        setAlwaysOnTop(s->boolValue("ui/alwaysOnTop"));
}

void MainWindow::applyEngineSettings() {
    AppSettings *s = AppSettings::instance();
    m_mpv->setHardwareDecoding(s->stringValue("video/hwdec"));
    m_mpv->setProperty("tone-mapping", s->stringValue("video/toneMapping"));
    m_mpv->setProperty("scale", s->stringValue("video/scale"));
    m_mpv->setProperty("cscale", s->stringValue("video/scale") == "bilinear" ? "bilinear" : "spline36");
    m_mpv->setProperty("deband", s->boolValue("video/debandFilter"));
    m_mpv->setProperty("screenshot-directory", s->stringValue("video/screenshotDir"));

    const QString ao = s->stringValue("audio/output");
    m_mpv->setProperty("ao", ao == "auto" ? QString() : ao);
    m_mpv->setProperty("audio-exclusive", s->boolValue("audio/exclusive"));
    m_mpv->setProperty("audio-spdif", s->boolValue("audio/passthrough") ? "ac3,eac3,dts,dts-hd,truehd" : "");
    m_mpv->setProperty("audio-channels", s->stringValue("audio/channels"));
    m_mpv->setProperty("audio-pitch-correction", s->boolValue("play/pitchCorrection"));
    m_mpv->setProperty("alang", s->stringValue("audio/preferredLang"));
    const QString dev = s->raw()->value("audio/device").toString();
    if (!dev.isEmpty()) m_mpv->setProperty("audio-device", dev);
    m_mpv->setProperty("slang", s->stringValue("sub/preferredLang"));
    if (!m_mpv->hasMedia()) {
        m_mpv->setProperty("audio-delay", s->intValue("audio/delayMs") / 1000.0);
        m_mpv->setProperty("sub-delay", s->intValue("sub/delayMs") / 1000.0);
    }
    applyAudioFilters();
    applySubtitleStyle();
}

void MainWindow::applyAudioFilters() {
    AppSettings *s = AppSettings::instance();
    QStringList af;
    if (s->boolValue("audio/normalize")) af << "lavfi=[dynaudnorm=f=250:g=15:p=0.9]";
    const int bal = s->intValue("audio/balance");
    if (bal != 0) af << QString("lavfi=[stereotools=balance_out=%1]").arg(bal / 100.0, 0, 'f', 2);
    m_mpv->setProperty("af", af.join(','));
}

static QString mpvColor(const QString &argbOrRgb) {
    // Qt "#AARRGGBB" -> mpv "#AARRGGBB" (same order), plain "#RRGGBB" works too
    return QColor(argbOrRgb).name(QColor::HexArgb).toUpper();
}

void MainWindow::applySubtitleStyle() {
    AppSettings *s = AppSettings::instance();
    m_mpv->setProperty("sub-auto", s->boolValue("sub/autoLoad") ? "fuzzy" : "no");
    m_mpv->setProperty("sub-font", s->stringValue("sub/font"));
    m_mpv->setProperty("sub-font-size", qlonglong(s->intValue("sub/size")));
    m_mpv->setProperty("sub-color", mpvColor(s->stringValue("sub/color")));
    m_mpv->setProperty("sub-border-color", mpvColor(s->stringValue("sub/borderColor")));
    m_mpv->setProperty("sub-border-size", s->doubleValue("sub/borderSize"));
    m_mpv->setProperty("sub-outline-size", s->doubleValue("sub/borderSize"));   // mpv >= 0.38 name
    m_mpv->setProperty("sub-bold", s->boolValue("sub/bold"));
    liftSubtitles(m_mpv, m_controls, m_controls->isVisible());
    m_mpv->setProperty("sub-codepage", s->stringValue("sub/codepage"));
    m_mpv->setProperty("sub-ass-override", s->boolValue("sub/overrideAss") ? "force" : "scale");
    if (s->boolValue("sub/background")) {
        m_mpv->setProperty("sub-back-color", mpvColor(s->stringValue("sub/backgroundColor")));
        m_mpv->setProperty("sub-border-style", "opaque-box");
    } else {
        m_mpv->setProperty("sub-back-color", "#00000000");
        m_mpv->setProperty("sub-border-style", "outline-and-shadow");
    }
}

void MainWindow::applyVideoFilters() {
    QStringList vf;
    if (m_hflip) vf << "hflip";
    if (m_vflip) vf << "vflip";
    m_mpv->setProperty("vf", vf.join(','));
    m_mpv->setProperty("video-rotate", qlonglong(m_rotation));
    m_mpv->setProperty("video-zoom", m_zoom);
    if (m_aspect == "stretch") {
        m_mpv->setProperty("keepaspect", false);
        m_mpv->setProperty("video-aspect-override", "no");
    } else {
        m_mpv->setProperty("keepaspect", true);
        m_mpv->setProperty("video-aspect-override", m_aspect);
    }
    updateMediaActions();
}

void MainWindow::restoreWindowState() {
    QSettings *s = AppSettings::instance()->raw();
    if (AppSettings::instance()->boolValue("ui/rememberGeometry") && s->contains("window/geometry")) {
        restoreGeometry(s->value("window/geometry").toByteArray());
        // Never restore off-screen (monitor unplugged since last run)
        bool visible = false;
        for (QScreen *sc : QGuiApplication::screens()) if (sc->availableGeometry().intersects(frameGeometry())) visible = true;
        if (!visible) resize(1180, 720);
    } else {
        resize(1180, 720);
        if (QScreen *sc = QGuiApplication::primaryScreen()) move(sc->availableGeometry().center() - rect().center());
    }
    if (s->contains("window/split")) m_split->restoreState(s->value("window/split").toByteArray());
    if (AppSettings::instance()->boolValue("ui/alwaysOnTop")) setWindowFlag(Qt::WindowStaysOnTopHint, true);
}

// ------------------------------------------------------------------ events
bool MainWindow::eventFilter(QObject *o, QEvent *e) {
    const bool onVideo = o == m_mpv || o == m_stage->placeholder;
    switch (e->type()) {
    case QEvent::MouseMove:
        if (onVideo) {
            showControls();
        }
        break;
    case QEvent::MouseButtonPress:
        if (onVideo) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::RightButton) {
                buildContextMenu()->popup(me->globalPosition().toPoint());
                return true;
            }
            if (me->button() == Qt::MiddleButton && AppSettings::instance()->stringValue("mouse/middleClick") == "pause") {
                trigger("play_pause");
                return true;
            }
            if (me->button() == Qt::BackButton) { trigger("previous"); return true; }
            if (me->button() == Qt::ForwardButton) { trigger("next"); return true; }
            m_mpv->setFocus();
        }
        break;
    case QEvent::MouseButtonRelease:
        if (onVideo) {
            auto *me = static_cast<QMouseEvent *>(e);
            if (me->button() == Qt::LeftButton && AppSettings::instance()->stringValue("mouse/leftClick") == "pause" && m_mpv->hasMedia()) {
                // Delay so that a double-click (fullscreen) does not also pause
                static QTimer *clickTimer = nullptr;
                if (!clickTimer) {
                    clickTimer = new QTimer(this);
                    clickTimer->setSingleShot(true);
                    connect(clickTimer, &QTimer::timeout, this, [this] { trigger("play_pause"); });
                }
                clickTimer->setProperty("cancel", false);
                clickTimer->start(QApplication::doubleClickInterval());
                o->setProperty("clickTimer", QVariant::fromValue<QObject *>(clickTimer));
            }
        }
        break;
    case QEvent::MouseButtonDblClick:
        if (onVideo && static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton) {
            if (auto *t = qobject_cast<QTimer *>(o->property("clickTimer").value<QObject *>())) t->stop();
            if (AppSettings::instance()->stringValue("mouse/doubleClick") == "fullscreen") toggleFullscreen();
            return true;
        }
        break;
    case QEvent::Wheel:
        if (onVideo) {
            auto *we = static_cast<QWheelEvent *>(e);
            const int steps = we->angleDelta().y() / 120;
            if (steps == 0) break;
            const QString mode = AppSettings::instance()->stringValue("mouse/wheel");
            if (mode == "volume") { for (int i = 0; i < qAbs(steps); ++i) trigger(steps > 0 ? "vol_up" : "vol_down"); }
            else if (mode == "seek") trigger(steps > 0 ? "seek_fwd" : "seek_back");
            return true;
        }
        break;
    case QEvent::Leave:
        if (o == m_controls) m_hideTimer.start(AppSettings::instance()->intValue("ui/autoHideMs"));
        break;
    default: break;
    }
    return QWidget::eventFilter(o, e);
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    switch (e->key()) {
    case Qt::Key_Escape: if (m_fullscreen) setFullscreen(false); return;
    case Qt::Key_MediaPlay: case Qt::Key_MediaPause: case Qt::Key_MediaTogglePlayPause: trigger("play_pause"); return;
    case Qt::Key_MediaStop: trigger("stop"); return;
    case Qt::Key_MediaNext: trigger("next"); return;
    case Qt::Key_MediaPrevious: trigger("previous"); return;
    case Qt::Key_VolumeUp: trigger("vol_up"); return;
    case Qt::Key_VolumeDown: trigger("vol_down"); return;
    case Qt::Key_VolumeMute: trigger("mute"); return;
    default: QWidget::keyPressEvent(e);
    }
}

void MainWindow::changeEvent(QEvent *e) {
    if (e->type() == QEvent::WindowStateChange) {
        m_title->updateMaximized();
        const bool flat = isMaximized() || isFullScreen();
        if (!m_fullscreen) m_root->layout()->setContentsMargins(flat ? 0 : 6, 4, flat ? 0 : 6, flat ? 0 : 6);
        m_root->update();
    }
    QWidget::changeEvent(e);
}

void MainWindow::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    if (m_nativeReady) return;
    // The native window exists (with its OpenGL surface) only now. Calling
    // winId() earlier forces Qt to recreate the window when the video widget
    // attaches, which can leave it hidden on Windows.
    m_nativeReady = true;
    QTimer::singleShot(0, this, [this] {
        AppSettings *s = AppSettings::instance();
        Platform::setRoundedCorners(this);
        Platform::setDarkTitle(this, ThemeManager::instance()->current().dark);
        if (s->boolValue("ui/windowBackdrop")) Platform::setWindowBackdrop(this, true, ThemeManager::instance()->current().dark);
        m_mediaKeys->setEnabled(s->raw()->value("ui/globalMediaKeys", true).toBool(), this);
        Log::write("info", QString("main window shown %1x%2, video surface %3")
                               .arg(width()).arg(height()).arg(m_mpv->isRenderReady() ? "ready" : "NOT ready"));
    });
}

void MainWindow::closeEvent(QCloseEvent *e) {
    saveResume();
    QSettings *s = AppSettings::instance()->raw();
    if (m_fullscreen) setFullscreen(false);
    s->setValue("window/geometry", saveGeometry());
    s->setValue("window/split", m_split->saveState());
    AppSettings::instance()->sync();
    Library::instance()->save();
    m_mpv->stop();
    e->accept();
    qApp->quit();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e) {
    QStringList paths;
    for (const QUrl &u : e->mimeData()->urls()) paths << (u.isLocalFile() ? u.toLocalFile() : u.toString());
    const bool append = (e->modifiers() & Qt::ControlModifier) ||
                        (m_playlistPanel->isVisible() && m_playlistPanel->geometry().contains(m_split->mapFrom(this, e->position().toPoint())));
    openPaths(paths, append && m_playlist->count() > 0);
    activateWindow();
}

void MainWindow::handleInstanceMessage(const QStringList &argsIn) {
    QStringList args = argsIn;
    const bool enqueue = args.removeAll("--enqueue") > 0;
    QStringList files;
    for (const QString &a : args) if (!a.startsWith("--")) files << a;
    if (!files.isEmpty()) openPaths(files, enqueue && m_playlist->count() > 0);
    if (isMinimized()) showNormal();
    raise();
    activateWindow();
}

// ------------------------------------------------------------- self tests
static void waitMs(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void MainWindow::runSelfTest(const QStringList &files, const QString &outDir) {
    QDir().mkpath(outDir);
    // Headless CI machines have no sound card: allow a null audio output
    if (qEnvironmentVariableIsSet("IBRA_TEST_AO")) m_mpv->setProperty("ao", qEnvironmentVariable("IBRA_TEST_AO"));
    QJsonArray results;
    int idx = 0;
    for (const QString &file : files) {
        QJsonObject r;
        r["file"] = QFileInfo(file).fileName();
        bool loaded = false, failed = false;
        QString error;
        auto c1 = connect(m_mpv, &MpvWidget::fileLoaded, this, [&] { loaded = true; });
        auto c2 = connect(m_mpv, &MpvWidget::playbackError, this, [&](const QString &e) { failed = true; error = e; });
        openPaths({file}, false);
        QElapsedTimer t;
        t.start();
        while (!loaded && !failed && t.elapsed() < 15000) waitMs(50);
        disconnect(c1);
        disconnect(c2);
        r["loaded"] = loaded;
        r["windowVisible"] = isVisible() && windowHandle() && windowHandle()->isExposed();
        r["videoSurface"] = m_mpv->isRenderReady();
        if (!loaded) {
            r["error"] = failed ? error : QString("timeout");
            results.append(r);
            continue;
        }
        const double startPos = m_mpv->position();
        waitMs(2500);
        const double played = m_mpv->position() - startPos;
        r["advancedSeconds"] = played;
        r["playing"] = played > 1.0;
        // seek test
        if (m_mpv->duration() > 4) {
            const double target = m_mpv->duration() / 2;
            m_mpv->seekAbsolute(target);
            waitMs(900);
            r["seekOk"] = qAbs(m_mpv->position() - target) < 2.5;
        }
        r["info"] = m_mpv->mediaInfo();
        showControls();
        waitMs(200);
        const QString shot = QDir(outDir).filePath(QString("shot_%1_%2.png").arg(idx++, 2, 10, QChar('0')).arg(QFileInfo(file).completeBaseName()));
        grab().save(shot);
        r["screenshot"] = QFileInfo(shot).fileName();
        results.append(r);
    }
    QFile f(QDir(outDir).filePath("report.json"));
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(results).toJson(QJsonDocument::Indented));
}

void MainWindow::captureThemeShots(const QString &outDir) {
    QDir().mkpath(outDir);
    for (const Theme &t : ThemeManager::instance()->builtIns()) {
        for (const QString &lang : {QString("en"), QString("ar")}) {
            I18n::instance()->setLanguage(lang);
            ThemeManager::instance()->preview(t);
            showControls();
            waitMs(400);
            grab().save(QDir(outDir).filePath(QString("theme_%1_%2.png").arg(t.id, lang)));
        }
    }
}

void MainWindow::captureUiShots(const QString &outDir, const QStringList &files) {
    QDir().mkpath(outDir);
    for (const QString &lang : {QString("en"), QString("ar")}) {
        I18n::instance()->setLanguage(lang);
        ThemeManager::instance()->apply(lang == "ar" ? "red-black" : "red-white");
        closeFile();
        m_playlist->clear();
        if (m_playlistPanel->isVisible()) togglePlaylist();
        waitMs(300);
        grab().save(QDir(outDir).filePath("idle_" + lang + ".png"));
        openPaths(files, false);
        togglePlaylist();
        waitMs(2500);
        showControls();
        waitMs(200);
        grab().save(QDir(outDir).filePath("playlist_" + lang + ".png"));
        {
            SettingsDialog d(this);
            for (const QString &page : {QString("interface"), QString("hwaccel"), QString("subtitles"), QString("codecs"), QString("shortcuts")}) {
                d.selectPage(page);
                d.show();
                waitMs(250);
                d.grab().save(QDir(outDir).filePath("settings_" + page + "_" + lang + ".png"));
            }
        }
        {
            ThemeStudio d(this);
            d.show();
            waitMs(300);
            d.grab().save(QDir(outDir).filePath("themestudio_" + lang + ".png"));
            static_cast<QDialog &>(d).reject();
        }
        {
            MediaInfoDialog d(m_mpv->mediaInfo(), this);
            d.show();
            waitMs(200);
            d.grab().save(QDir(outDir).filePath("mediainfo_" + lang + ".png"));
        }
        {
            AboutDialog d(this);
            d.show();
            waitMs(200);
            d.grab().save(QDir(outDir).filePath("about_" + lang + ".png"));
        }
        {
            QMenu *m = buildContextMenu();
            m->popup(mapToGlobal(QPoint(300, 200)));
            waitMs(250);
            m->grab().save(QDir(outDir).filePath("menu_" + lang + ".png"));
            m->close();
        }
    }
}
