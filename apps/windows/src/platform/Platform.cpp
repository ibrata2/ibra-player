#include "Platform.h"
#include "core/Playlist.h"
#include <QWidget>
#include <QCoreApplication>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QOperatingSystemVersion>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#include <shlobj.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

namespace {
const wchar_t *kProgId = L"iBRAPLAYER.Media";

QString exePath() { return QDir::toNativeSeparators(QCoreApplication::applicationFilePath()); }

bool setKey(const QString &path, const QString &name, const QString &value) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(path.utf16()), 0, nullptr, 0,
                        KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    const std::wstring v = value.toStdWString();
    const LONG r = RegSetValueExW(key, name.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(name.utf16()), 0, REG_SZ,
                                  reinterpret_cast<const BYTE *>(v.c_str()), DWORD((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

void deleteTree(const QString &path) {
    RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(path.utf16()));
}

QStringList shellTargets() {
    QStringList t;
    for (const QString &e : Playlist::mediaExtensions()) t << "Software\\Classes\\SystemFileAssociations\\." + e;
    t << "Software\\Classes\\Directory";
    return t;
}
}

bool Platform::backdropSupported() {
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 22621);
}

bool Platform::setWindowBackdrop(QWidget *w, bool enable, bool dark) {
    if (!backdropSupported()) return false;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    setDarkTitle(w, dark);
    MARGINS m = enable ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &m);
    int type = enable ? 3 /* DWMSBT_TRANSIENTWINDOW (acrylic) */ : 1 /* none */;
    return SUCCEEDED(DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type)));
}

void Platform::setRoundedCorners(QWidget *w) {
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    int pref = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
}

void Platform::setDarkTitle(QWidget *w, bool dark) {
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    BOOL v = dark;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &v, sizeof(v));
}

bool Platform::registerContextMenu() {
    const QString exe = exePath();
    bool ok = true;
    for (const QString &base : shellTargets()) {
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Play", "", "Play with iBRA PLAYER");
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Play", "Icon", "\"" + exe + "\",0");
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Play\\command", "", "\"" + exe + "\" \"%1\"");
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Enqueue", "", "Add to iBRA PLAYER playlist");
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Enqueue", "Icon", "\"" + exe + "\",0");
        ok &= setKey(base + "\\shell\\iBRAPLAYER.Enqueue\\command", "", "\"" + exe + "\" --enqueue \"%1\"");
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

bool Platform::isContextMenuRegistered() {
    HKEY key;
    const QString p = "Software\\Classes\\SystemFileAssociations\\.mp4\\shell\\iBRAPLAYER.Play";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(p.utf16()), 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    RegCloseKey(key);
    return true;
}

bool Platform::registerFileAssociations(const QStringList &extensions) {
    const QString exe = exePath();
    const QString prog = QString::fromWCharArray(kProgId);
    bool ok = true;
    ok &= setKey("Software\\Classes\\" + prog, "", "iBRA PLAYER media file");
    ok &= setKey("Software\\Classes\\" + prog + "\\DefaultIcon", "", "\"" + exe + "\",0");
    ok &= setKey("Software\\Classes\\" + prog + "\\shell\\open\\command", "", "\"" + exe + "\" \"%1\"");
    const QString app = "Software\\Classes\\Applications\\" + QFileInfo(exe).fileName();
    ok &= setKey(app, "FriendlyAppName", "iBRA PLAYER");
    ok &= setKey(app + "\\shell\\open\\command", "", "\"" + exe + "\" \"%1\"");
    const QString caps = "Software\\iBRA PLAYER\\Capabilities";
    ok &= setKey(caps, "ApplicationName", "iBRA PLAYER");
    ok &= setKey(caps, "ApplicationDescription", "iBRA PLAYER media player");
    for (const QString &e : extensions) {
        ok &= setKey("Software\\Classes\\." + e + "\\OpenWithProgids", prog, "");
        ok &= setKey(app + "\\SupportedTypes", "." + e, "");
        ok &= setKey(caps + "\\FileAssociations", "." + e, prog);
    }
    ok &= setKey("Software\\RegisteredApplications", "iBRA PLAYER", caps);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

bool Platform::unregisterAll() {
    for (const QString &base : shellTargets()) {
        deleteTree(base + "\\shell\\iBRAPLAYER.Play");
        deleteTree(base + "\\shell\\iBRAPLAYER.Enqueue");
    }
    const QString prog = QString::fromWCharArray(kProgId);
    deleteTree("Software\\Classes\\" + prog);
    for (const QString &e : Playlist::mediaExtensions()) {
        HKEY key;
        const QString p = "Software\\Classes\\." + e + "\\OpenWithProgids";
        if (RegOpenKeyExW(HKEY_CURRENT_USER, reinterpret_cast<LPCWSTR>(p.utf16()), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
            RegDeleteValueW(key, kProgId);
            RegCloseKey(key);
        }
    }
    deleteTree("Software\\Classes\\Applications\\" + QFileInfo(exePath()).fileName());
    deleteTree("Software\\iBRA PLAYER\\Capabilities");
    HKEY reg;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, KEY_SET_VALUE, &reg) == ERROR_SUCCESS) {
        RegDeleteValueW(reg, L"iBRA PLAYER");
        RegCloseKey(reg);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

void Platform::openDefaultAppsSettings() {
    QDesktopServices::openUrl(QUrl("ms-settings:defaultapps"));
}

// ---------------------------------------------------------------- media keys
MediaKeyFilter::MediaKeyFilter(QObject *parent) : QObject(parent) {}
MediaKeyFilter::~MediaKeyFilter() { setEnabled(false, nullptr); }

void MediaKeyFilter::setEnabled(bool on, QWidget *window) {
    if (m_registered) {
        for (int id = 1; id <= 4; ++id) UnregisterHotKey(reinterpret_cast<HWND>(m_hwnd), 0xB100 + id);
        m_registered = false;
    }
    if (!on || !window) return;
    m_hwnd = quintptr(window->winId());
    HWND h = reinterpret_cast<HWND>(m_hwnd);
    // Registration may fail when another player owns the key; we then still get
    // the keys through Qt while our window has focus.
    bool any = false;
    any |= RegisterHotKey(h, 0xB101, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE);
    any |= RegisterHotKey(h, 0xB102, MOD_NOREPEAT, VK_MEDIA_STOP);
    any |= RegisterHotKey(h, 0xB103, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK);
    any |= RegisterHotKey(h, 0xB104, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK);
    m_registered = any;
}

bool MediaKeyFilter::nativeEventFilter(const QByteArray &type, void *message, qintptr *) {
    if (type != "windows_generic_MSG") return false;
    MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY) return false;
    switch (msg->wParam) {
    case 0xB101: emit playPause(); return true;
    case 0xB102: emit stop(); return true;
    case 0xB103: emit next(); return true;
    case 0xB104: emit previous(); return true;
    }
    return false;
}

#else  // ------------------------------------------------------------ other OS

bool Platform::backdropSupported() { return false; }
bool Platform::setWindowBackdrop(QWidget *, bool, bool) { return false; }
void Platform::setRoundedCorners(QWidget *) {}
void Platform::setDarkTitle(QWidget *, bool) {}
bool Platform::registerContextMenu() { return false; }
bool Platform::registerFileAssociations(const QStringList &) { return false; }
bool Platform::unregisterAll() { return false; }
void Platform::openDefaultAppsSettings() {}
bool Platform::isContextMenuRegistered() { return false; }

MediaKeyFilter::MediaKeyFilter(QObject *parent) : QObject(parent) {}
MediaKeyFilter::~MediaKeyFilter() {}
void MediaKeyFilter::setEnabled(bool, QWidget *) {}
bool MediaKeyFilter::nativeEventFilter(const QByteArray &, void *, qintptr *) { return false; }
#endif
