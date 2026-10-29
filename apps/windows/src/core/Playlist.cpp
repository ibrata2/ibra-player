#include "Playlist.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QRandomGenerator>
#include <QCollator>
#include <QSaveFile>
#include <QStringConverter>
#include <algorithm>

QString PlaylistItem::displayName() const {
    if (!title.isEmpty()) return title;
    if (path.contains("://")) {
        QUrl u(path);
        const QString f = u.fileName();
        return f.isEmpty() ? path : QUrl::fromPercentEncoding(f.toUtf8());
    }
    return QFileInfo(path).fileName();
}

const QStringList &Playlist::mediaExtensions() {
    static const QStringList e = {
        // video
        "mp4","mkv","avi","mov","wmv","flv","webm","mpeg","mpg","m4v","ts","mts","m2ts","vob","3gp","3g2",
        "ogv","divx","xvid","asf","dvr-ms","mxf","ivf","f4v","rm","rmvb","m2v","mpe","h264","h265","hevc",
        "264","265","av1","y4m","wtv","nut","dav","mk3d","iso","evo","trp","tp","ogm","qt","mod","tod",
        // audio
        "mp3","aac","m4a","flac","wav","wma","ogg","oga","opus","ac3","eac3","ec3","dts","dtshd","thd",
        "truehd","alac","ape","aiff","aif","aifc","amr","awb","mid","midi","mka","wv","tta","mpc","au",
        "caf","dsf","dff","spx","weba","m4b","mp2","mpa","ra","w64","cue",
        // images
        "jpg","jpeg","png","webp","bmp","gif","tif","tiff","avif","jxl"};
    return e;
}
const QStringList &Playlist::playlistExtensions() {
    static const QStringList e = {"m3u","m3u8","pls"};
    return e;
}
const QStringList &Playlist::subtitleExtensions() {
    static const QStringList e = {"srt","ass","ssa","sub","idx","vtt","sup","smi","sami","txt","lrc","ttml","dfxp","usf","jss","rt","pjs","mpl","stl"};
    return e;
}

bool Playlist::isMediaFile(const QString &p) { return mediaExtensions().contains(QFileInfo(p).suffix().toLower()); }
bool Playlist::isPlaylistFile(const QString &p) { return playlistExtensions().contains(QFileInfo(p).suffix().toLower()); }
bool Playlist::isSubtitleFile(const QString &p) {
    const QString s = QFileInfo(p).suffix().toLower();
    return s != "txt" && subtitleExtensions().contains(s);
}

static void naturalSort(QStringList &l) {
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(l.begin(), l.end(), [&](const QString &a, const QString &b) { return c.compare(a, b) < 0; });
}

QStringList Playlist::scanFolder(const QString &dir, bool recursive) {
    QStringList out;
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                    recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        const QString f = it.next();
        if (isMediaFile(f)) out << f;
        if (out.size() > 20000) break;
    }
    naturalSort(out);
    return out;
}

QVector<PlaylistItem> Playlist::expand(const QStringList &paths) {
    QVector<PlaylistItem> out;
    for (const QString &raw : paths) {
        if (raw.contains("://") && !raw.startsWith("file:")) { out.append({raw, {}, 0}); continue; }
        const QString p = raw.startsWith("file:") ? QUrl(raw).toLocalFile() : raw;
        QFileInfo fi(p);
        if (fi.isDir()) {
            for (const QString &f : scanFolder(p)) out.append({f, {}, 0});
        } else if (isPlaylistFile(p)) {
            out += parseFile(p);
        } else if (fi.exists()) {
            out.append({fi.absoluteFilePath(), {}, 0});
        }
    }
    return out;
}

QVector<PlaylistItem> Playlist::parseFile(const QString &file) {
    QVector<PlaylistItem> out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    if (f.size() > 32 * 1024 * 1024) return out;   // refuse absurd playlist sizes
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);  // M3U8 is UTF-8; plain M3U is usually ASCII/UTF-8 too
    const QDir base = QFileInfo(file).absoluteDir();
    const QString ext = QFileInfo(file).suffix().toLower();

    auto resolve = [&](QString entry) -> QString {
        entry = entry.trimmed();
        if (entry.startsWith("file:")) return QUrl(entry).toLocalFile();
        if (entry.contains("://")) return entry;
        entry.replace('\\', '/');
        QFileInfo fi(entry);
        return fi.isAbsolute() ? fi.absoluteFilePath() : QFileInfo(base.filePath(entry)).absoluteFilePath();
    };

    if (ext == "pls") {
        QMap<int, PlaylistItem> map;
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            const int eq = line.indexOf('=');
            if (eq < 0) continue;
            const QString key = line.left(eq).toLower();
            const QString val = line.mid(eq + 1);
            auto num = [&](const QString &prefix) { return key.mid(prefix.size()).toInt(); };
            if (key.startsWith("file")) map[num("file")].path = resolve(val);
            else if (key.startsWith("title")) map[num("title")].title = val;
            else if (key.startsWith("length")) map[num("length")].duration = qMax(0, val.toInt());
        }
        for (const auto &it : map) if (!it.path.isEmpty()) out.append(it);
    } else {
        QString pendingTitle;
        double pendingDur = 0;
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith(QChar(0xFEFF))) line.remove(0, 1);
            if (line.isEmpty()) continue;
            if (line.startsWith("#EXTINF:", Qt::CaseInsensitive)) {
                const QString rest = line.mid(8);
                const int comma = rest.indexOf(',');
                pendingDur = qMax(0.0, rest.left(comma).section(' ', 0, 0).toDouble());
                pendingTitle = comma >= 0 ? rest.mid(comma + 1).trimmed() : QString();
                continue;
            }
            if (line.startsWith('#')) continue;
            out.append({resolve(line), pendingTitle, pendingDur});
            pendingTitle.clear();
            pendingDur = 0;
        }
    }
    return out;
}

bool Playlist::saveTo(const QString &file) const {
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    const QString ext = QFileInfo(file).suffix().toLower();
    if (ext == "pls") {
        ts << "[playlist]\n";
        for (int i = 0; i < m_items.size(); ++i) {
            const auto &it = m_items[i];
            ts << "File" << i + 1 << "=" << QDir::toNativeSeparators(it.path) << "\n";
            ts << "Title" << i + 1 << "=" << it.displayName() << "\n";
            ts << "Length" << i + 1 << "=" << (it.duration > 0 ? qRound(it.duration) : -1) << "\n";
        }
        ts << "NumberOfEntries=" << m_items.size() << "\nVersion=2\n";
    } else {
        ts << "#EXTM3U\n";
        for (const auto &it : m_items) {
            ts << "#EXTINF:" << (it.duration > 0 ? qRound(it.duration) : -1) << "," << it.displayName() << "\n";
            ts << (it.path.contains("://") ? it.path : QDir::toNativeSeparators(it.path)) << "\n";
        }
    }
    ts.flush();
    return f.commit();
}

const PlaylistItem *Playlist::currentItem() const {
    return (m_current >= 0 && m_current < m_items.size()) ? &m_items[m_current] : nullptr;
}

void Playlist::setCurrent(int index) {
    if (index < -1 || index >= m_items.size()) return;
    m_current = index;
    emit currentChanged(index);
}

void Playlist::add(const QVector<PlaylistItem> &items) {
    m_items += items;
    rebuildShuffle();
    emit changed();
}

void Playlist::replace(const QVector<PlaylistItem> &items) {
    m_items = items;
    m_current = -1;
    rebuildShuffle();
    emit changed();
}

void Playlist::removeAt(const QList<int> &rowsIn) {
    QList<int> rows = rowsIn;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        if (r < 0 || r >= m_items.size()) continue;
        m_items.removeAt(r);
        if (r < m_current) --m_current;
        else if (r == m_current) m_current = -1;
    }
    rebuildShuffle();
    emit changed();
}

void Playlist::move(int from, int to) {
    if (from < 0 || from >= m_items.size() || to < 0 || to >= m_items.size() || from == to) return;
    m_items.move(from, to);
    if (m_current == from) m_current = to;
    else if (from < m_current && to >= m_current) --m_current;
    else if (from > m_current && to <= m_current) ++m_current;
    emit changed();
}

void Playlist::setOrder(const QVector<int> &order) {
    if (order.size() != m_items.size()) return;
    QVector<PlaylistItem> n;
    int newCurrent = -1;
    for (int i = 0; i < order.size(); ++i) {
        n.append(m_items[order[i]]);
        if (order[i] == m_current) newCurrent = i;
    }
    m_items = n;
    m_current = newCurrent;
    rebuildShuffle();
    emit changed();
}

void Playlist::clear() { replace({}); }

void Playlist::sortByName() {
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    const QString cur = currentItem() ? currentItem()->path : QString();
    std::stable_sort(m_items.begin(), m_items.end(), [&](const PlaylistItem &a, const PlaylistItem &b) {
        return c.compare(a.displayName(), b.displayName()) < 0;
    });
    m_current = -1;
    for (int i = 0; i < m_items.size(); ++i) if (m_items[i].path == cur) m_current = i;
    emit changed();
}

void Playlist::setDuration(int index, double d) {
    if (index < 0 || index >= m_items.size()) return;
    m_items[index].duration = d;
}

void Playlist::setShuffle(bool on) {
    m_shuffle = on;
    rebuildShuffle();
    emit modeChanged();
}

void Playlist::rebuildShuffle() {
    m_shuffleOrder.clear();
    for (int i = 0; i < m_items.size(); ++i) m_shuffleOrder << i;
    std::shuffle(m_shuffleOrder.begin(), m_shuffleOrder.end(), *QRandomGenerator::global());
}

int Playlist::nextIndex(bool userAction) const {
    if (m_items.isEmpty()) return -1;
    if (m_repeat == RepeatOne && !userAction) return m_current;
    if (m_shuffle) {
        const int pos = m_shuffleOrder.indexOf(m_current);
        if (pos + 1 < m_shuffleOrder.size()) return m_shuffleOrder[pos + 1];
        return (m_repeat == RepeatAll || userAction) ? m_shuffleOrder.value(0, -1) : -1;
    }
    if (m_current + 1 < m_items.size()) return m_current + 1;
    return (m_repeat == RepeatAll || userAction) ? 0 : -1;
}

int Playlist::previousIndex() const {
    if (m_items.isEmpty()) return -1;
    if (m_shuffle) {
        const int pos = m_shuffleOrder.indexOf(m_current);
        return pos > 0 ? m_shuffleOrder[pos - 1] : m_shuffleOrder.last();
    }
    return m_current > 0 ? m_current - 1 : m_items.size() - 1;
}
