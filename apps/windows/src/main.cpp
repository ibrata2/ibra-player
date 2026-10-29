// iBRA PLAYER — برمجة و تطوير ابراهيم الخزرجي
#include <QApplication>
#include <QSplashScreen>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>
#include <QStyleFactory>
#include <QTimer>
#include <QPainter>
#include <QFileInfo>
#include <QDir>
#include <QSurfaceFormat>
#include <QEventLoop>

#include "ui/MainWindow.h"
#include "engine/MpvWidget.h"
#include "ui/Icons.h"
#include "core/AppSettings.h"
#include "core/I18n.h"
#include "core/Log.h"
#include "theme/Theme.h"
#include "platform/Platform.h"
#include "core/Playlist.h"

static QString serverName() {
    return QStringLiteral("iBRA-PLAYER-") + qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "user"));
}

static QStringList absoluteArgs(const QStringList &args) {
    QStringList out;
    for (const QString &a : args) {
        if (a.startsWith("--") || a.contains("://")) out << a;
        else out << QFileInfo(a).absoluteFilePath();
    }
    return out;
}

static QPixmap splashPixmap() {
    QPixmap pm(520, 300);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QLinearGradient g(0, 0, 520, 300);
    g.setColorAt(0, QColor("#1a0b0d"));
    g.setColorAt(1, QColor("#050304"));
    p.setBrush(g);
    p.setPen(QPen(QColor(255, 80, 95, 120), 1.2));
    p.drawRoundedRect(QRectF(1, 1, 518, 298), 18, 18);
    QRadialGradient glow(QPointF(260, 120), 200);
    glow.setColorAt(0, QColor(255, 31, 53, 90));
    glow.setColorAt(1, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawRoundedRect(QRectF(1, 1, 518, 298), 18, 18);
    p.drawPixmap(QRect(200, 34, 120, 120), Icons::logo(240));
    QFont f("Segoe UI");
    f.setPixelSize(30);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 170, 520, 44), Qt::AlignCenter, "iBRA PLAYER");
    f.setPixelSize(13);
    f.setBold(false);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    p.setFont(f);
    p.setPen(QColor(255, 255, 255, 170));
    p.drawText(QRect(0, 214, 520, 24), Qt::AlignCenter, QString("v%1").arg(IBRA_VERSION));
    p.drawText(QRect(0, 250, 520, 24), Qt::AlignCenter, QString::fromUtf8("برمجة و تطوير ابراهيم الخزرجي"));
    return pm;
}

#ifdef Q_OS_WIN
#include <windows.h>
// Versions 1.0.0/1.0.1 shipped Mesa's opengl32.dll by mistake; an update
// installed over them can leave it behind, which replaces the GPU driver and
// makes the player close at start. Rename it away and restart cleanly.
static bool disableStrayOpenGL() {
    wchar_t exe[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    std::wstring dir(exe, n);
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    bool moved = false;
    for (const wchar_t *name : {L"opengl32.dll", L"libgallium_wgl.dll", L"libglapi.dll"}) {
        const std::wstring f = dir + name;
        if (GetFileAttributesW(f.c_str()) == INVALID_FILE_ATTRIBUTES) continue;
        const std::wstring off = f + L".disabled";
        DeleteFileW(off.c_str());
        if (MoveFileExW(f.c_str(), off.c_str(), MOVEFILE_REPLACE_EXISTING)) moved = true;
    }
    if (!moved) return false;
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe, GetCommandLineW(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return true;
    }
    return false;
}
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    if (disableStrayOpenGL()) return 0;   // the restarted copy runs with the real GPU driver
#endif
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QApplication::setApplicationName("iBRA PLAYER");
    QApplication::setApplicationDisplayName("iBRA PLAYER");
    QApplication::setOrganizationName(QString());   // data lives in %APPDATA%\\iBRA PLAYER
    QApplication::setApplicationVersion(IBRA_VERSION);
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/brand/logo.png"));

    QStringList args = app.arguments().mid(1);

    // ---- installer hooks (run silently, no UI)
    if (args.contains("--register-context-menu")) return Platform::registerContextMenu() ? 0 : 1;
    if (args.contains("--register-associations")) {
        QStringList ext = Playlist::mediaExtensions();
        ext << Playlist::playlistExtensions();
        return Platform::registerFileAssociations(ext) ? 0 : 1;
    }
    if (args.contains("--unregister-all")) return Platform::unregisterAll() ? 0 : 1;

    AppSettings *settings = AppSettings::instance();
    Log::init();

    const int selfTest = args.indexOf("--selftest");
    const int themeShots = args.indexOf("--theme-shots");
    const int uiShots = args.indexOf("--ui-shots");
    const bool testing = selfTest >= 0 || themeShots >= 0 || uiShots >= 0;

    // ---- single instance: forward files to the running player
    QLocalServer *server = nullptr;
    if (!testing && settings->boolValue("ui/singleInstance")) {
        QLocalSocket probe;
        probe.connectToServer(serverName());
        if (probe.waitForConnected(400)) {
            QByteArray data;
            QDataStream ds(&data, QIODevice::WriteOnly);
            ds << absoluteArgs(args);
            probe.write(data);
            probe.waitForBytesWritten(1000);
            probe.disconnectFromServer();
            return 0;
        }
        QLocalServer::removeServer(serverName());
        server = new QLocalServer(&app);
        server->setSocketOptions(QLocalServer::UserAccessOption);
        server->listen(serverName());
    }

    I18n::instance()->setLanguage(settings->stringValue("ui/language"));
    ThemeManager::instance()->apply(settings->stringValue("ui/theme"));

    QSplashScreen *splash = nullptr;
    if (!testing && settings->boolValue("ui/showSplash")) {
        splash = new QSplashScreen(splashPixmap(), Qt::WindowStaysOnTopHint);
        splash->setAttribute(Qt::WA_TranslucentBackground);
        splash->show();
        app.processEvents();
    }

    MainWindow w;
    if (server) {
        QObject::connect(server, &QLocalServer::newConnection, &w, [server, &w] {
            QLocalSocket *s = server->nextPendingConnection();
            QObject::connect(s, &QLocalSocket::readyRead, &w, [s, &w] {
                QDataStream ds(s->readAll());
                QStringList a;
                ds >> a;
                w.handleInstanceMessage(a);
            });
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        });
    }

    // Only the main window's own close quits the app: a splash screen closing
    // must never end the program.
    QApplication::setQuitOnLastWindowClosed(false);
    w.show();
    w.raise();
    w.activateWindow();
    QTimer::singleShot(2500, &w, [&w] {
        if (!w.isVisible() || w.isMinimized()) {
            Log::write("warn", "main window was not visible after start; showing it again");
            w.showNormal();
            w.raise();
            w.activateWindow();
        }
    });
    if (splash) QTimer::singleShot(700, splash, [splash, &w] { splash->finish(&w); splash->deleteLater(); });

    if (selfTest >= 0) {
        // --selftest <out-dir> <file>...
        const QString out = args.value(selfTest + 1);
        const QStringList files = args.mid(selfTest + 2);
        QTimer::singleShot(500, &w, [&w, out, files] {
            w.runSelfTest(files, out);
            qApp->quit();
        });
        return app.exec();
    }
    if (uiShots >= 0) {
        const QString out = args.value(uiShots + 1);
        const QStringList files = args.mid(uiShots + 2);
        QTimer::singleShot(500, &w, [&w, out, files] { w.captureUiShots(out, files); qApp->quit(); });
        return app.exec();
    }
    if (themeShots >= 0) {
        const QString out = args.value(themeShots + 1);
        const QStringList files = args.mid(themeShots + 2);
        QTimer::singleShot(500, &w, [&w, out, files] {
            if (!files.isEmpty()) {
                w.openPaths(files, false);
                QEventLoop l; QTimer::singleShot(2500, &l, &QEventLoop::quit); l.exec();
                w.engine()->setPaused(true);
            }
            w.captureThemeShots(out);
            qApp->quit();
        });
        return app.exec();
    }

    QStringList files;
    bool enqueue = false;
    for (const QString &a : args) {
        if (a == "--enqueue") enqueue = true;
        else if (!a.startsWith("--")) files << a;
    }
    if (!files.isEmpty()) QTimer::singleShot(0, &w, [&w, files, enqueue] { w.openPaths(absoluteArgs(files), enqueue); });
    return app.exec();
}
