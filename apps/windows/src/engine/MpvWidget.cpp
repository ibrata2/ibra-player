#include "MpvWidget.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QMetaObject>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <clocale>
#include <cstring>

#include <mpv/client.h>
#include <mpv/render_gl.h>

// ---------------------------------------------------------------------------
// QVariant <-> mpv_node helpers (replacement for the removed qthelper.hpp)
// ---------------------------------------------------------------------------
namespace {

struct NodeBuilder {
    mpv_node node{};
    explicit NodeBuilder(const QVariant &v) { set(&node, v); }
    ~NodeBuilder() { free_node(&node); }

    static char *dup(const QString &s) {
        QByteArray b = s.toUtf8();
        char *r = new char[b.size() + 1];
        std::memcpy(r, b.constData(), b.size() + 1);
        return r;
    }
    static void set(mpv_node *dst, const QVariant &src) {
        switch (src.typeId()) {
        case QMetaType::QString:
        case QMetaType::QByteArray:
            dst->format = MPV_FORMAT_STRING;
            dst->u.string = dup(src.toString());
            break;
        case QMetaType::Bool:
            dst->format = MPV_FORMAT_FLAG;
            dst->u.flag = src.toBool() ? 1 : 0;
            break;
        case QMetaType::Int: case QMetaType::LongLong:
        case QMetaType::UInt: case QMetaType::ULongLong:
            dst->format = MPV_FORMAT_INT64;
            dst->u.int64 = src.toLongLong();
            break;
        case QMetaType::Double: case QMetaType::Float:
            dst->format = MPV_FORMAT_DOUBLE;
            dst->u.double_ = src.toDouble();
            break;
        case QMetaType::QVariantList:
        case QMetaType::QStringList: {
            QVariantList list = src.toList();
            dst->format = MPV_FORMAT_NODE_ARRAY;
            auto *l = new mpv_node_list{};
            l->num = list.size();
            l->values = new mpv_node[list.size()]{};
            for (int i = 0; i < list.size(); ++i) set(&l->values[i], list[i]);
            dst->u.list = l;
            break;
        }
        case QMetaType::QVariantMap: {
            QVariantMap map = src.toMap();
            dst->format = MPV_FORMAT_NODE_MAP;
            auto *l = new mpv_node_list{};
            l->num = map.size();
            l->values = new mpv_node[map.size()]{};
            l->keys = new char *[map.size()];
            int i = 0;
            for (auto it = map.cbegin(); it != map.cend(); ++it, ++i) {
                l->keys[i] = dup(it.key());
                set(&l->values[i], it.value());
            }
            dst->u.list = l;
            break;
        }
        default:
            dst->format = MPV_FORMAT_NONE;
        }
    }
    static void free_node(mpv_node *n) {
        switch (n->format) {
        case MPV_FORMAT_STRING: delete[] n->u.string; break;
        case MPV_FORMAT_NODE_ARRAY:
        case MPV_FORMAT_NODE_MAP: {
            mpv_node_list *l = n->u.list;
            for (int i = 0; i < l->num; ++i) {
                free_node(&l->values[i]);
                if (l->keys) delete[] l->keys[i];
            }
            delete[] l->values;
            delete[] l->keys;
            delete l;
            break;
        }
        default: break;
        }
        n->format = MPV_FORMAT_NONE;
    }
};

QVariant nodeToVariant(const mpv_node *n) {
    switch (n->format) {
    case MPV_FORMAT_STRING: return QString::fromUtf8(n->u.string);
    case MPV_FORMAT_FLAG: return bool(n->u.flag);
    case MPV_FORMAT_INT64: return qlonglong(n->u.int64);
    case MPV_FORMAT_DOUBLE: return n->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList l;
        for (int i = 0; i < n->u.list->num; ++i) l.append(nodeToVariant(&n->u.list->values[i]));
        return l;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap m;
        for (int i = 0; i < n->u.list->num; ++i)
            m.insert(QString::fromUtf8(n->u.list->keys[i]), nodeToVariant(&n->u.list->values[i]));
        return m;
    }
    default: return {};
    }
}

void *getProcAddress(void *, const char *name) {
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    return ctx ? reinterpret_cast<void *>(ctx->getProcAddress(QByteArray(name))) : nullptr;
}

} // namespace

QString MediaTrack::label() const {
    QStringList parts;
    parts << QString("#%1").arg(id);
    if (!title.isEmpty()) parts << title;
    if (!lang.isEmpty()) parts << QString("[%1]").arg(lang);
    if (!codec.isEmpty()) parts << QString("(%1%2)").arg(codec, channels > 0 ? QString(" %1ch").arg(channels) : QString());
    if (external) parts << "ext";
    return parts.join(' ');
}

// ---------------------------------------------------------------------------

MpvWidget::MpvWidget(QWidget *parent) : QOpenGLWidget(parent) {
    // mpv requires the C numeric locale
    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (!m_mpv) qFatal("iBRA PLAYER: could not create the mpv playback engine");

    // Engine defaults. Everything user-facing is overridden later from settings.
    mpv_set_option_string(m_mpv, "vo", "libmpv");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "keep-open", "yes");          // we decide what happens at EOF
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "ytdl", "no");                // never launches external downloaders
    mpv_set_option_string(m_mpv, "load-scripts", "no");        // never executes user scripts
    mpv_set_option_string(m_mpv, "config", "no");              // ignore stray mpv.conf files
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");
    mpv_set_option_string(m_mpv, "sub-codepage", "auto");
    mpv_set_option_string(m_mpv, "audio-file-auto", "no");
    mpv_set_option_string(m_mpv, "screenshot-format", "png");
    mpv_set_option_string(m_mpv, "terminal", "no");
    mpv_set_option_string(m_mpv, "msg-level", "all=warn");
    mpv_set_option_string(m_mpv, "volume-max", "200");
    mpv_set_option_string(m_mpv, "audio-pitch-correction", "yes");
    mpv_set_option_string(m_mpv, "demuxer-max-bytes", "150MiB");
    mpv_set_option_string(m_mpv, "cache", "auto");
    mpv_set_option_string(m_mpv, "image-display-duration", "inf");

    if (mpv_initialize(m_mpv) < 0) qFatal("iBRA PLAYER: could not initialise mpv");
    mpv_request_log_messages(m_mpv, "warn");

    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "dwidth", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "dheight", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, 0, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 0, "ab-loop-a", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "ab-loop-b", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, 0, "eof-reached", MPV_FORMAT_FLAG);

    mpv_set_wakeup_callback(m_mpv, &MpvWidget::onWakeup, this);

    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

MpvWidget::~MpvWidget() {
    makeCurrent();
    if (m_gl) mpv_render_context_free(m_gl);
    doneCurrent();
    mpv_terminate_destroy(m_mpv);
}

void MpvWidget::initializeGL() {
    mpv_opengl_init_params glInit{&getProcAddress, nullptr};
    // ADVANCED_CONTROL is deliberately left off: with it, the mpv core may wait
    // on the GUI thread while the GUI thread waits on a property -> deadlock.
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&m_gl, m_mpv, params) < 0) {
        m_lastError = QStringLiteral("OpenGL render context could not be created (update the graphics driver)");
        emit playbackError(m_lastError);
        return;
    }
    mpv_render_context_set_update_callback(m_gl, &MpvWidget::onUpdate, this);
    if (auto *ctx = QOpenGLContext::currentContext()) {
        auto *f = ctx->functions();
        emit logMessage("info", QString("OpenGL: %1 / %2 / %3").arg(
            reinterpret_cast<const char *>(f->glGetString(GL_VENDOR)),
            reinterpret_cast<const char *>(f->glGetString(GL_RENDERER)),
            reinterpret_cast<const char *>(f->glGetString(GL_VERSION))));
    }
}

void MpvWidget::paintGL() {
    if (!m_gl) return;
    const qreal dpr = devicePixelRatioF();
    mpv_opengl_fbo fbo{static_cast<int>(defaultFramebufferObject()),
                       int(width() * dpr), int(height() * dpr), 0};
    int flip = 1;
    mpv_render_param params[]{{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                              {MPV_RENDER_PARAM_FLIP_Y, &flip},
                              {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(m_gl, params);
}

void MpvWidget::onUpdate(void *ctx) {
    QMetaObject::invokeMethod(static_cast<MpvWidget *>(ctx), "maybeUpdate", Qt::QueuedConnection);
}

void MpvWidget::maybeUpdate() {
    // When the window is hidden/minimised Qt will not paint, but mpv still needs
    // the frame to be consumed so the playback clock keeps running.
    if (window()->isMinimized() || !isVisible()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
        if (m_gl) mpv_render_context_report_swap(m_gl);
    } else {
        update();
    }
}

void MpvWidget::onWakeup(void *ctx) {
    QMetaObject::invokeMethod(static_cast<MpvWidget *>(ctx), "onMpvEvents", Qt::QueuedConnection);
}

void MpvWidget::onMpvEvents() {
    while (m_mpv) {
        mpv_event *ev = mpv_wait_event(m_mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;
        handleEvent(ev);
    }
}

void MpvWidget::handleEvent(void *raw) {
    auto *ev = static_cast<mpv_event *>(raw);
    switch (ev->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *p = static_cast<mpv_event_property *>(ev->data);
        const QByteArray name(p->name);
        if (name == "time-pos") {
            if (p->format == MPV_FORMAT_DOUBLE) { m_position = *static_cast<double *>(p->data); emit positionChanged(m_position); }
        } else if (name == "duration") {
            m_duration = p->format == MPV_FORMAT_DOUBLE ? *static_cast<double *>(p->data) : 0;
            emit durationChanged(m_duration);
        } else if (name == "pause") {
            if (p->format == MPV_FORMAT_FLAG) { m_paused = *static_cast<int *>(p->data); emit pauseChanged(m_paused); }
        } else if (name == "volume") {
            if (p->format == MPV_FORMAT_DOUBLE) { m_volume = qRound(*static_cast<double *>(p->data)); emit volumeChanged(m_volume); }
        } else if (name == "mute") {
            if (p->format == MPV_FORMAT_FLAG) { m_muted = *static_cast<int *>(p->data); emit muteChanged(m_muted); }
        } else if (name == "speed") {
            if (p->format == MPV_FORMAT_DOUBLE) { m_speed = *static_cast<double *>(p->data); emit speedChanged(m_speed); }
        } else if (name == "track-list") {
            parseTracks();
        } else if (name == "media-title") {
            if (p->format == MPV_FORMAT_STRING) emit titleChanged(QString::fromUtf8(*static_cast<char **>(p->data)));
        } else if (name == "idle-active") {
            if (p->format == MPV_FORMAT_FLAG) emit idleChanged(*static_cast<int *>(p->data));
        } else if (name == "dwidth" || name == "dheight") {
            emit videoSizeChanged(getProperty("dwidth").toInt(), getProperty("dheight").toInt());
        } else if (name == "hwdec-current") {
            emit hwdecChanged(p->format == MPV_FORMAT_STRING ? QString::fromUtf8(*static_cast<char **>(p->data)) : QString());
        } else if (name == "ab-loop-a" || name == "ab-loop-b") {
            emit abLoopChanged();
        } else if (name == "eof-reached") {
            if (p->format == MPV_FORMAT_FLAG && *static_cast<int *>(p->data) && m_hasMedia)
                emit endOfFile(true, QString());
        }
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        m_hasMedia = true;
        m_lastError.clear();
        parseTracks();
        emit fileLoaded();
        break;
    case MPV_EVENT_END_FILE: {
        auto *e = static_cast<mpv_event_end_file *>(ev->data);
        if (e->reason == MPV_END_FILE_REASON_ERROR) {
            m_lastError = QString::fromUtf8(mpv_error_string(e->error));
            m_hasMedia = false;
            emit playbackError(m_lastError);
            emit endOfFile(false, m_lastError);
        }
        // Normal EOF is reported through "eof-reached" because keep-open=yes.
        break;
    }
    case MPV_EVENT_LOG_MESSAGE: {
        auto *m = static_cast<mpv_event_log_message *>(ev->data);
        emit logMessage(QString::fromUtf8(m->level),
                        QString::fromUtf8(m->prefix) + ": " + QString::fromUtf8(m->text).trimmed());
        break;
    }
    default: break;
    }
}

void MpvWidget::parseTracks() {
    m_tracks.clear();
    const QVariantList list = getProperty("track-list").toList();
    for (const QVariant &v : list) {
        const QVariantMap m = v.toMap();
        MediaTrack t;
        t.id = m.value("id").toInt();
        t.type = m.value("type").toString();
        t.title = m.value("title").toString();
        t.lang = m.value("lang").toString();
        t.codec = m.value("codec").toString();
        t.selected = m.value("selected").toBool();
        t.external = m.value("external").toBool();
        t.channels = m.value("demux-channel-count").toInt();
        m_tracks.append(t);
    }
    emit tracksChanged();
}

// ---------------------------------------------------------------------------

void MpvWidget::command(const QVariantList &args) {
    NodeBuilder n(args);
    mpv_node result{};
    if (mpv_command_node(m_mpv, &n.node, &result) >= 0) mpv_free_node_contents(&result);
}

void MpvWidget::commandAsync(const QVariantList &args) {
    NodeBuilder n(args);
    mpv_command_node_async(m_mpv, 0, &n.node);
}

void MpvWidget::setOption(const QString &name, const QVariant &value) { setProperty(name, value); }

void MpvWidget::setProperty(const QString &name, const QVariant &value) {
    NodeBuilder n(value);
    mpv_set_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &n.node);
}

QVariant MpvWidget::getProperty(const QString &name) const {
    mpv_node node{};
    if (mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node) < 0) return {};
    QVariant v = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return v;
}

QString MpvWidget::getPropertyString(const QString &name) const {
    char *s = mpv_get_property_string(m_mpv, name.toUtf8().constData());
    if (!s) return {};
    QString r = QString::fromUtf8(s);
    mpv_free(s);
    return r;
}

void MpvWidget::loadFile(const QString &pathOrUrl, double startSeconds) {
    m_path = pathOrUrl;
    m_hasMedia = false;
    m_position = 0;
    // mpv >= 0.38 inserts an "index" argument before options; using the property
    // keeps us compatible with every libmpv version shipped by distributions.
    setProperty("start", startSeconds > 1 ? QString::number(startSeconds, 'f', 3) : QString("none"));
    command({"loadfile", pathOrUrl, "replace"});
    setPaused(false);
}

void MpvWidget::stop() {
    command({"stop"});
    m_hasMedia = false;
    m_position = 0;
    m_duration = 0;
    emit positionChanged(0);
    emit durationChanged(0);
}

void MpvWidget::togglePause() { setPaused(!m_paused); }
void MpvWidget::setPaused(bool p) { setProperty("pause", p); }
void MpvWidget::seekAbsolute(double s) { commandAsync({"seek", s, "absolute+exact"}); }
void MpvWidget::seekRelative(double s) { commandAsync({"seek", s, "relative+exact"}); }
void MpvWidget::seekPercent(double pct) { commandAsync({"seek", pct, "absolute-percent+exact"}); }
void MpvWidget::frameStep() { command({"frame-step"}); }
void MpvWidget::frameBackStep() { command({"frame-back-step"}); }
void MpvWidget::setVolume(int v) { setProperty("volume", double(qBound(0, v, 200))); }
void MpvWidget::setMuted(bool m) { setProperty("mute", m); }
void MpvWidget::setSpeed(double s) { setProperty("speed", qBound(0.1, s, 16.0)); }
void MpvWidget::setAudioTrack(int id) { setProperty("aid", id > 0 ? QVariant(qlonglong(id)) : QVariant("no")); }
void MpvWidget::setSubTrack(int id) { setProperty("sid", id > 0 ? QVariant(qlonglong(id)) : QVariant("no")); }
void MpvWidget::setSecondarySubTrack(int id) { setProperty("secondary-sid", id > 0 ? QVariant(qlonglong(id)) : QVariant("no")); }
void MpvWidget::addSubtitle(const QString &file) { command({"sub-add", file, "select"}); }
void MpvWidget::addAudioFile(const QString &file) { command({"audio-add", file, "select"}); }
void MpvWidget::setLoopFile(bool on) { setProperty("loop-file", on ? "inf" : "no"); }
void MpvWidget::setHardwareDecoding(const QString &mode) { setProperty("hwdec", mode); }

void MpvWidget::screenshot(const QString &dir) {
    QDir().mkpath(dir);
    const QString base = QFileInfo(m_path).completeBaseName().left(60);
    const QString file = QDir(dir).filePath(QString("%1_%2.png").arg(base.isEmpty() ? "iBRA" : base,
                          QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));
    command({"screenshot-to-file", file, "subtitles"});
}

void MpvWidget::setAbLoopA() { setProperty("ab-loop-a", m_position); }
void MpvWidget::setAbLoopB() { setProperty("ab-loop-b", m_position); }
void MpvWidget::clearAbLoop() { setProperty("ab-loop-a", "no"); setProperty("ab-loop-b", "no"); }

QJsonObject MpvWidget::mediaInfo() const {
    QJsonObject o;
    const QString path = getPropertyString("path");
    QFileInfo fi(path);
    o["path"] = path;
    o["filename"] = getPropertyString("filename");
    o["extension"] = fi.suffix().toLower();
    o["container"] = getPropertyString("file-format");
    o["fileSize"] = getProperty("file-size").toLongLong();
    o["duration"] = m_duration;
    o["videoCodec"] = getPropertyString("video-codec");
    o["videoFormat"] = getPropertyString("video-format");
    o["width"] = getProperty("width").toInt();
    o["height"] = getProperty("height").toInt();
    o["displayWidth"] = getProperty("dwidth").toInt();
    o["displayHeight"] = getProperty("dheight").toInt();
    o["fps"] = getProperty("container-fps").toDouble();
    o["estimatedFps"] = getProperty("estimated-vf-fps").toDouble();
    o["videoBitrate"] = getProperty("video-bitrate").toLongLong();
    o["audioBitrate"] = getProperty("audio-bitrate").toLongLong();
    o["aspect"] = getProperty("video-params/aspect").toDouble();
    o["pixelFormat"] = getPropertyString("video-params/pixelformat");
    o["colormatrix"] = getPropertyString("video-params/colormatrix");
    o["primaries"] = getPropertyString("video-params/primaries");
    o["gamma"] = getPropertyString("video-params/gamma");
    const QString gamma = o["gamma"].toString();
    o["hdr"] = gamma == "pq" ? "HDR10 (PQ)" : gamma == "hlg" ? "HLG" : "SDR";
    o["audioCodec"] = getPropertyString("audio-codec");
    o["audioCodecName"] = getPropertyString("audio-codec-name");
    o["audioChannels"] = getPropertyString("audio-params/hr-channels");
    o["audioSampleRate"] = getProperty("audio-params/samplerate").toInt();
    o["audioOutput"] = getPropertyString("current-ao");
    o["videoOutput"] = getPropertyString("current-vo");
    o["hwdec"] = getPropertyString("hwdec-current");
    o["position"] = m_position;
    o["droppedFrames"] = getProperty("frame-drop-count").toLongLong();
    o["avsync"] = getProperty("avsync").toDouble();
    QJsonArray tracks;
    for (const MediaTrack &t : m_tracks) {
        QJsonObject j;
        j["id"] = t.id; j["type"] = t.type; j["codec"] = t.codec; j["lang"] = t.lang;
        j["title"] = t.title; j["selected"] = t.selected; j["external"] = t.external;
        if (t.channels) j["channels"] = t.channels;
        tracks.append(j);
    }
    o["tracks"] = tracks;
    o["error"] = m_lastError;
    return o;
}
