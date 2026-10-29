#pragma once
#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QMap>

// Persistent settings. Stored as INI in %APPDATA%\iBRA PLAYER, or next to the
// executable when a file named "portable.ini" exists there (portable build).
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings *instance();

    QVariant value(const QString &key) const;
    void setValue(const QString &key, const QVariant &v);
    bool boolValue(const QString &key) const { return value(key).toBool(); }
    int intValue(const QString &key) const { return value(key).toInt(); }
    double doubleValue(const QString &key) const { return value(key).toDouble(); }
    QString stringValue(const QString &key) const { return value(key).toString(); }

    QString dataDir() const { return m_dataDir; }
    bool isPortable() const { return m_portable; }
    QSettings *raw() { return m_settings; }

    bool exportTo(const QString &file) const;
    bool importFrom(const QString &file);
    void restoreDefaults();
    static const QMap<QString, QVariant> &defaults();
    void sync() { m_settings->sync(); }

signals:
    void changed(const QString &key);
    void reloaded();

private:
    AppSettings();
    QSettings *m_settings = nullptr;
    QString m_dataDir;
    bool m_portable = false;
};
