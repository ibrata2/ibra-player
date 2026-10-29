#include "Library.h"
#include "AppSettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>

Library *Library::instance() {
    static Library *l = new Library();
    return l;
}

Library::Library() {
    m_file = AppSettings::instance()->dataDir() + "/library.json";
    load();
}

bool Library::historyEnabled() const { return AppSettings::instance()->boolValue("privacy/history"); }

void Library::load() {
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    for (const auto &v : o["recent"].toArray()) m_recent << v.toString();
    for (const auto &v : o["favorites"].toArray()) m_favorites << v.toString();
    const QJsonObject r = o["resume"].toObject();
    for (auto it = r.begin(); it != r.end(); ++it) m_resume.insert(it.key(), it.value().toDouble());
    const QJsonObject d = o["durations"].toObject();
    for (auto it = d.begin(); it != d.end(); ++it) m_durations.insert(it.key(), it.value().toDouble());
}

void Library::save() {
    QJsonObject o;
    o["recent"] = QJsonArray::fromStringList(m_recent);
    o["favorites"] = QJsonArray::fromStringList(m_favorites);
    QJsonObject r, d;
    for (auto it = m_resume.cbegin(); it != m_resume.cend(); ++it) r[it.key()] = it.value();
    for (auto it = m_durations.cbegin(); it != m_durations.cend(); ++it) d[it.key()] = it.value();
    o["resume"] = r;
    o["durations"] = d;
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        f.commit();
    }
}

void Library::addRecent(const QString &path) {
    if (!historyEnabled() || path.isEmpty()) return;
    m_recent.removeAll(path);
    m_recent.prepend(path);
    while (m_recent.size() > 25) m_recent.removeLast();
    save();
    emit changed();
}

void Library::toggleFavorite(const QString &path) {
    if (path.isEmpty()) return;
    if (m_favorites.contains(path)) m_favorites.removeAll(path);
    else m_favorites.append(path);
    save();
    emit changed();
}

void Library::setResume(const QString &path, double seconds, double duration) {
    if (!historyEnabled() || path.isEmpty()) return;
    // Do not keep a resume point for the very start/end or for short clips
    if (duration < 60 || seconds < 10 || seconds > duration - 15) m_resume.remove(path);
    else m_resume.insert(path, seconds);
    if (m_resume.size() > 500) m_resume.erase(m_resume.begin());
    save();
}

double Library::resumePosition(const QString &path) const {
    if (!historyEnabled()) return 0;
    return m_resume.value(path, 0);
}

void Library::setDuration(const QString &path, double seconds) {
    if (seconds <= 0 || path.isEmpty()) return;
    m_durations.insert(path, seconds);
    if (m_durations.size() > 2000) m_durations.erase(m_durations.begin());
}

void Library::clearHistory() {
    m_recent.clear();
    m_resume.clear();
    m_durations.clear();
    save();
    emit changed();
}

void Library::clearFavorites() {
    m_favorites.clear();
    save();
    emit changed();
}
