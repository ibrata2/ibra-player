#pragma once
#include <QObject>
#include <QStringList>
#include <QHash>

// Local-only media library: recent files, favourites and resume positions.
// Stored in library.json inside the settings folder; never transmitted.
class Library : public QObject {
    Q_OBJECT
public:
    static Library *instance();

    void addRecent(const QString &path);
    QStringList recent() const { return m_recent; }

    bool isFavorite(const QString &path) const { return m_favorites.contains(path); }
    void toggleFavorite(const QString &path);
    QStringList favorites() const { return m_favorites; }

    void setResume(const QString &path, double seconds, double duration);
    double resumePosition(const QString &path) const;
    void setDuration(const QString &path, double seconds);
    double knownDuration(const QString &path) const { return m_durations.value(path, 0); }

    void clearHistory();          // recent + resume positions
    void clearFavorites();
    void save();

signals:
    void changed();

private:
    Library();
    void load();
    bool historyEnabled() const;
    QString m_file;
    QStringList m_recent, m_favorites;
    QHash<QString, double> m_resume, m_durations;
};
