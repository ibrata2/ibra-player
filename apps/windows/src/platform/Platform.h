#pragma once
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QStringList>

class QWidget;

// Windows-specific integration. Every function is a safe no-op elsewhere.
namespace Platform {
bool backdropSupported();                       // Windows 11 22H2+ system backdrop
bool setWindowBackdrop(QWidget *w, bool enable, bool dark);
void setRoundedCorners(QWidget *w);
void setDarkTitle(QWidget *w, bool dark);

// Explorer integration (per-user, HKCU — never needs administrator rights)
bool registerContextMenu();
bool registerFileAssociations(const QStringList &extensions);
bool unregisterAll();
void openDefaultAppsSettings();
bool isContextMenuRegistered();
}

class MediaKeyFilter : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit MediaKeyFilter(QObject *parent = nullptr);
    ~MediaKeyFilter() override;
    void setEnabled(bool on, QWidget *window);
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;
signals:
    void playPause();
    void stop();
    void next();
    void previous();
private:
    bool m_registered = false;
    quintptr m_hwnd = 0;
};
