#pragma once
#include <QWidget>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QJsonObject>
#include "core/Playlist.h"

class MpvWidget;
class TitleBar;
class ControlBar;
class PlaylistPanel;
class QAction;
class QMenu;
class QLabel;
class QSplitter;
class MediaKeyFilter;
class QNetworkAccessManager;

// Container that paints the frosted glass backdrop and hosts edge resizing.
class GlassRoot : public QWidget {
    Q_OBJECT
public:
    explicit GlassRoot(QWidget *parent = nullptr);
    bool translucentWindow = false;
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
private:
    Qt::Edges edgesAt(const QPoint &p) const;
};

// Holds the video and the overlays (control bar, OSD).
class Stage : public QWidget {
    Q_OBJECT
public:
    explicit Stage(QWidget *parent = nullptr) : QWidget(parent) {}
    MpvWidget *video = nullptr;
    QWidget *controls = nullptr;
    QLabel *osd = nullptr;
    QWidget *placeholder = nullptr;
    bool frame = true;
    void relayout();
protected:
    void resizeEvent(QResizeEvent *) override { relayout(); }
    void paintEvent(QPaintEvent *) override;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void openPaths(const QStringList &paths, bool append);
    void handleInstanceMessage(const QStringList &args);
    void runSelfTest(const QStringList &files, const QString &outDir);
    void captureThemeShots(const QString &outDir);
    void captureUiShots(const QString &outDir, const QStringList &files);
    MpvWidget *engine() const { return m_mpv; }

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void changeEvent(QEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

private slots:
    void onFileLoaded();
    void onEndOfFile(bool eof, const QString &error);
    void onPosition(double pos);
    void onTracksChanged();
    void onControlAction(const QString &id);
    void retranslate();
    void retheme();
    void applySettings(const QString &key = QString());

private:
    // construction
    void createActions();
    QAction *act(const QString &id, const char *labelKey = nullptr, bool checkable = false);
    void buildMenus();
    void fillMenu(QMenu *menu, const QString &which);
    QMenu *buildContextMenu();

    // operations
    void trigger(const QString &id);
    void playIndex(int index);
    void playNext(bool userAction);
    void playPrevious();
    void stopPlayback();
    void closeFile();
    void saveResume();
    void openFileDialog(bool append = false);
    void openFolderDialog(bool append = false);
    void openUrlDialog();
    void openDisc(const QString &kind);
    void openSubtitleDialog();
    void openAudioFileDialog();
    void loadPlaylistDialog();
    void savePlaylistDialog();
    void jumpToDialog();
    void showMediaInfo();
    void showSettings(const QString &page = QString());
    void showThemeStudio();
    void showAbout();
    void searchSubtitlesOnline();
    void checkForUpdates(bool silent);
    void downloadAndInstall(const QJsonObject &asset);
    void toggleFullscreen();
    void setFullscreen(bool fs);
    void togglePlaylist();
    void setAlwaysOnTop(bool on);
    void cycleTrack(const QString &type);
    void cycleAbLoop();
    void cycleRepeat();
    void changeSpeed(double delta, bool absolute = false);
    void adjustProperty(const QString &prop, double delta, const QString &osdKey, double scale = 1, const QString &unit = QString());
    void osd(const QString &text, int ms = 1600);
    void showControls();
    void hideControlsIfIdle();
    void updateTitle();
    void updateMediaActions();
    void updateRecentMenu(QMenu *m);
    void updateFavoritesMenu(QMenu *m);
    void applyEngineSettings();
    void applySubtitleStyle();
    void applyAudioFilters();
    void applyVideoFilters();
    void restoreWindowState();

    GlassRoot *m_root = nullptr;
    TitleBar *m_title = nullptr;
    Stage *m_stage = nullptr;
    MpvWidget *m_mpv = nullptr;
    ControlBar *m_controls = nullptr;
    PlaylistPanel *m_playlistPanel = nullptr;
    QSplitter *m_split = nullptr;
    QLabel *m_osd = nullptr;
    Playlist *m_playlist = nullptr;
    MediaKeyFilter *m_mediaKeys = nullptr;
    QNetworkAccessManager *m_net = nullptr;

    QHash<QString, QAction *> m_actions;
    QTimer m_hideTimer, m_osdTimer, m_resumeTimer;
    bool m_fullscreen = false, m_wasMaximized = false;
    bool m_pendingPlay = false;
    bool m_nativeReady = false;
    bool m_hflip = false, m_vflip = false;
    int m_rotation = 0;
    double m_zoom = 0;
    QString m_aspect = "no";
    int m_brightness = 0, m_contrast = 0, m_saturation = 0, m_gamma = 0, m_hue = 0;
    QString m_currentPath;
};
