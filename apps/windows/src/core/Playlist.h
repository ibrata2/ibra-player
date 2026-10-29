#pragma once
#include <QObject>
#include <QStringList>
#include <QVector>

struct PlaylistItem {
    QString path;      // local file path or URL
    QString title;     // optional title from playlist file
    double duration = 0;
    QString displayName() const;
};

class Playlist : public QObject {
    Q_OBJECT
public:
    enum Repeat { RepeatOff, RepeatOne, RepeatAll };
    explicit Playlist(QObject *parent = nullptr) : QObject(parent) {}

    static const QStringList &mediaExtensions();
    static const QStringList &playlistExtensions();
    static const QStringList &subtitleExtensions();
    static bool isMediaFile(const QString &path);
    static bool isPlaylistFile(const QString &path);
    static bool isSubtitleFile(const QString &path);
    static QStringList scanFolder(const QString &dir, bool recursive = true);

    // Expand any mix of files / folders / playlists into playable entries
    static QVector<PlaylistItem> expand(const QStringList &paths);
    static QVector<PlaylistItem> parseFile(const QString &playlistFile);
    bool saveTo(const QString &file) const;   // .m3u / .m3u8 / .pls by extension

    const QVector<PlaylistItem> &items() const { return m_items; }
    int count() const { return m_items.size(); }
    int current() const { return m_current; }
    const PlaylistItem *currentItem() const;
    void setCurrent(int index);
    void add(const QVector<PlaylistItem> &items);
    void replace(const QVector<PlaylistItem> &items);
    void removeAt(const QList<int> &rows);
    void move(int from, int to);
    void setOrder(const QVector<int> &newOrderOfOldIndexes);
    void clear();
    void sortByName();
    void setDuration(int index, double d);

    int nextIndex(bool userAction) const;   // -1 when finished
    int previousIndex() const;
    void setShuffle(bool on);
    bool shuffle() const { return m_shuffle; }
    void setRepeat(Repeat r) { m_repeat = r; emit modeChanged(); }
    Repeat repeat() const { return m_repeat; }

signals:
    void changed();
    void currentChanged(int index);
    void modeChanged();

private:
    void rebuildShuffle();
    QVector<PlaylistItem> m_items;
    QVector<int> m_shuffleOrder;
    int m_current = -1;
    bool m_shuffle = false;
    Repeat m_repeat = RepeatOff;
};
