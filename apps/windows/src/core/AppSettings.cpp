#include "AppSettings.h"
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>

AppSettings *AppSettings::instance() {
    static AppSettings *s = new AppSettings();
    return s;
}

AppSettings::AppSettings() {
    const QString exeDir = QCoreApplication::applicationDirPath();
    if (QFile::exists(exeDir + "/portable.ini")) {
        m_portable = true;
        m_dataDir = exeDir + "/data";
    } else {
        m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QDir().mkpath(m_dataDir);
    m_settings = new QSettings(m_dataDir + "/settings.ini", QSettings::IniFormat, this);
}

const QMap<QString, QVariant> &AppSettings::defaults() {
    static const QMap<QString, QVariant> d = [] {
        QMap<QString, QVariant> m;
        const bool arabic = QLocale::system().language() == QLocale::Arabic;
        // Interface
        m["ui/language"] = arabic ? "ar" : "en";
        m["ui/theme"] = "red-white";
        m["ui/autoHideMs"] = 2500;
        m["ui/alwaysOnTop"] = false;
        m["ui/showSplash"] = true;
        m["ui/windowBackdrop"] = false;      // Windows 11 acrylic behind the window (optional)
        m["ui/singleInstance"] = true;
        m["ui/rememberGeometry"] = true;
        m["ui/osd"] = true;
        // Playback
        m["play/seekSmall"] = 5;
        m["play/seekLarge"] = 30;
        m["play/resume"] = true;
        m["play/autoNext"] = true;
        m["play/repeat"] = "off";            // off | one | all
        m["play/shuffle"] = false;
        m["play/speedStep"] = 0.25;
        m["play/volume"] = 100;
        m["play/muted"] = false;
        m["play/pitchCorrection"] = true;
        m["play/autoPlayOnOpen"] = true;
        // Video
        m["video/hwdec"] = "auto-safe";      // auto-safe | no | d3d11va | d3d11va-copy | dxva2-copy | qsv-copy | nvdec-copy
        m["video/deinterlace"] = "auto";
        m["video/toneMapping"] = "auto";
        m["video/scale"] = "bilinear";       // bilinear (fast) | spline36 | ewa_lanczossharp (quality)
        m["video/debandFilter"] = false;
        m["video/screenshotDir"] = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/iBRA PLAYER";
        // Audio
        m["audio/output"] = "auto";          // auto | wasapi | openal | sdl
        m["audio/exclusive"] = false;
        m["audio/passthrough"] = false;
        m["audio/channels"] = "auto-safe";   // auto-safe | stereo | 5.1 | 7.1
        m["audio/normalize"] = false;
        m["audio/delayMs"] = 0;
        m["audio/balance"] = 0;              // -100..100
        m["audio/preferredLang"] = "ara,arb,ar,eng,en";
        // Subtitles
        m["sub/autoLoad"] = true;
        m["sub/font"] = "Segoe UI";
        m["sub/size"] = 46;
        m["sub/color"] = "#FFFFFF";
        m["sub/borderColor"] = "#000000";
        m["sub/borderSize"] = 3.0;
        m["sub/background"] = false;
        m["sub/backgroundColor"] = "#99000000";
        m["sub/position"] = 100;             // 0 (top) .. 100 (bottom)
        m["sub/bold"] = true;
        m["sub/delayMs"] = 0;
        m["sub/codepage"] = "auto";          // auto | utf-8 | cp1256 | ...
        m["sub/overrideAss"] = false;
        m["sub/preferredLang"] = "ara,arb,ar,eng,en";
        // Playlist & privacy
        m["playlist/visible"] = false;
        m["playlist/addFolderSiblings"] = false;
        m["privacy/history"] = true;
        m["privacy/redactLogs"] = true;
        m["privacy/updateCheck"] = true;
        // Mouse
        m["mouse/wheel"] = "volume";         // volume | seek | none
        m["mouse/doubleClick"] = "fullscreen";
        m["mouse/middleClick"] = "pause";
        m["mouse/leftClick"] = "pause";      // pause | none
        return m;
    }();
    return d;
}

QVariant AppSettings::value(const QString &key) const {
    return m_settings->value(key, defaults().value(key));
}

void AppSettings::setValue(const QString &key, const QVariant &v) {
    if (m_settings->value(key, defaults().value(key)) == v) return;
    m_settings->setValue(key, v);
    emit changed(key);
}

bool AppSettings::exportTo(const QString &file) const {
    m_settings->sync();
    QFile::remove(file);
    return QFile::copy(m_settings->fileName(), file);
}

bool AppSettings::importFrom(const QString &file) {
    QSettings src(file, QSettings::IniFormat);
    if (src.status() != QSettings::NoError) return false;
    const QStringList keys = src.allKeys();
    if (keys.isEmpty()) return false;
    for (const QString &k : keys) {
        // Only accept known keys and the user's saved themes/shortcuts
        if (defaults().contains(k) || k.startsWith("shortcuts/") || k.startsWith("themes/"))
            m_settings->setValue(k, src.value(k));
    }
    m_settings->sync();
    emit reloaded();
    return true;
}

void AppSettings::restoreDefaults() {
    // keep library data (separate file), wipe preferences
    const QStringList keys = m_settings->allKeys();
    for (const QString &k : keys)
        if (!k.startsWith("themes/") && !k.startsWith("window/")) m_settings->remove(k);
    m_settings->sync();
    emit reloaded();
}
