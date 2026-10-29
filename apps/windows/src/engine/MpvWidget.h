// iBRA PLAYER — playback engine. libmpv (FFmpeg + libass + libplacebo) rendered
// into a QOpenGLWidget through the mpv render API, so glass UI overlays are
// composed by Qt and never touch the video frames themselves.
#pragma once

#include <QOpenGLWidget>
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>

struct mpv_handle;
struct mpv_render_context;

struct MediaTrack {
    int id = 0;
    QString type;      // video | audio | sub
    QString title;
    QString lang;
    QString codec;
    bool selected = false;
    bool external = false;
    int channels = 0;
    QString label() const;
};

class MpvWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;

    // Low level access
    void command(const QVariantList &args);
    void commandAsync(const QVariantList &args);
    void setOption(const QString &name, const QVariant &value);   // before/after init
    void setProperty(const QString &name, const QVariant &value);
    QVariant getProperty(const QString &name) const;
    QString getPropertyString(const QString &name) const;

    // High level transport
    void loadFile(const QString &pathOrUrl, double startSeconds = 0);
    void stop();
    void togglePause();
    void setPaused(bool p);
    bool isPaused() const { return m_paused; }
    bool hasMedia() const { return m_hasMedia; }
    void seekAbsolute(double seconds);
    void seekRelative(double seconds);
    void seekPercent(double pct);
    void frameStep();
    void frameBackStep();
    void setVolume(int v);
    void setMuted(bool m);
    void setSpeed(double s);
    void setAudioTrack(int id);   // 0 = off
    void setSubTrack(int id);     // 0 = off
    void setSecondarySubTrack(int id);
    void addSubtitle(const QString &file);
    void addAudioFile(const QString &file);
    void screenshot(const QString &dir);
    void setAbLoopA();
    void setAbLoopB();
    void clearAbLoop();
    void setLoopFile(bool on);
    void setHardwareDecoding(const QString &mode);  // auto-safe | no | d3d11va | dxva2 | ...

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    double speed() const { return m_speed; }
    QList<MediaTrack> tracks() const { return m_tracks; }
    QString currentPath() const { return m_path; }
    QString lastError() const { return m_lastError; }
    QJsonObject mediaInfo() const;     // snapshot for info panel / self-test

signals:
    void fileLoaded();
    void endOfFile(bool eofReached, const QString &error);
    void positionChanged(double sec);
    void durationChanged(double sec);
    void pauseChanged(bool paused);
    void volumeChanged(int v);
    void muteChanged(bool m);
    void speedChanged(double s);
    void tracksChanged();
    void videoSizeChanged(int w, int h);
    void titleChanged(const QString &title);
    void idleChanged(bool idle);
    void abLoopChanged();
    void logMessage(const QString &level, const QString &text);
    void playbackError(const QString &message);
    void hwdecChanged(const QString &active);

protected:
    void initializeGL() override;
    void paintGL() override;

private slots:
    void onMpvEvents();
    void maybeUpdate();

private:
    void handleEvent(void *event);
    void parseTracks();
    static void onUpdate(void *ctx);
    static void onWakeup(void *ctx);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_gl = nullptr;
    double m_position = 0, m_duration = 0, m_speed = 1;
    int m_volume = 100;
    bool m_muted = false, m_paused = true, m_hasMedia = false;
    QList<MediaTrack> m_tracks;
    QString m_path, m_lastError;
};
