#include "Log.h"
#include "AppSettings.h"
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

namespace {
QMutex g_mutex;
QString g_file;

void qtHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    static const char *names[] = {"debug", "warn", "critical", "fatal", "info"};
    const int idx = int(type) < 5 ? int(type) : 0;
    if (type == QtDebugMsg) return;
    Log::write(names[idx], msg);
}
}

QString Log::filePath() { return g_file; }

void Log::init() {
    const QString dir = AppSettings::instance()->dataDir() + "/logs";
    QDir().mkpath(dir);
    g_file = dir + "/ibra-player.log";
    QFile f(g_file);
    if (f.size() > 1024 * 1024) {           // simple rotation
        QFile::remove(g_file + ".1");
        QFile::rename(g_file, g_file + ".1");
    }
    qInstallMessageHandler(qtHandler);
    write("info", QString("iBRA PLAYER %1 started on %2 (%3)")
                      .arg(IBRA_VERSION, QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()));
}

QString Log::redact(const QString &text) {
    if (!AppSettings::instance()->boolValue("privacy/redactLogs")) return text;
    QString t = text;
    // Windows paths, UNC paths, POSIX paths and URLs -> keep only the extension
    static const QRegularExpression re(
        R"(([A-Za-z]:[\\/]|\\\\|/home/|/Users/|/mnt/|[a-z]+://)[^\s'"<>|]*)");
    t.replace(re, "<path>");
    return t;
}

void Log::write(const QString &level, const QString &text) {
    if (g_file.isEmpty()) return;
    QMutexLocker lock(&g_mutex);
    QFile f(g_file);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&f);
    ts << QDateTime::currentDateTime().toString(Qt::ISODate) << " [" << level << "] " << redact(text) << "\n";
}
