#include "MediaInfoDialog.h"
#include "ControlBar.h"
#include "core/I18n.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QLocale>

static QString bitrate(qint64 bps) {
    if (bps <= 0) return QString();
    if (bps >= 1000000) return QString::number(bps / 1e6, 'f', 2) + " Mb/s";
    return QString::number(bps / 1000) + " kb/s";
}

MediaInfoDialog::MediaInfoDialog(const QJsonObject &o, QWidget *parent) : QDialog(parent) {
    setWindowTitle(T("act.mediaInfo") + " — iBRA PLAYER");
    resize(640, 640);
    QList<QPair<QString, QString>> rows;
    auto add = [&](const char *key, const QString &v) { if (!v.isEmpty() && v != "0") rows.append({T(key), v}); };

    add("mi.file", o["filename"].toString());
    add("mi.extension", o["extension"].toString().toUpper());
    add("mi.container", o["container"].toString());
    if (o["fileSize"].toDouble() > 0) add("mi.size", QLocale().formattedDataSize(qint64(o["fileSize"].toDouble())));
    add("mi.duration", ControlBar::formatTime(o["duration"].toDouble(), true));
    add("mi.videoCodec", o["videoCodec"].toString());
    if (o["width"].toInt() > 0) {
        QString res = QString("%1 × %2").arg(o["width"].toInt()).arg(o["height"].toInt());
        const int h = o["height"].toInt();
        const QString cls = h >= 4000 ? "8K" : h >= 2000 ? "4K UHD" : h >= 1400 ? "2K" : h >= 1000 ? "Full HD" : h >= 700 ? "HD" : "SD";
        add("mi.resolution", res + "  (" + cls + ")");
        if (o["displayWidth"].toInt() != o["width"].toInt())
            add("mi.displaySize", QString("%1 × %2").arg(o["displayWidth"].toInt()).arg(o["displayHeight"].toInt()));
    }
    if (o["aspect"].toDouble() > 0) add("mi.aspect", QString::number(o["aspect"].toDouble(), 'f', 3));
    const double fps = o["fps"].toDouble() > 0 ? o["fps"].toDouble() : o["estimatedFps"].toDouble();
    if (fps > 0) add("mi.fps", QString::number(fps, 'f', 3));
    add("mi.videoBitrate", bitrate(qint64(o["videoBitrate"].toDouble())));
    add("mi.pixelFormat", o["pixelFormat"].toString());
    if (!o["primaries"].toString().isEmpty())
        add("mi.color", QString("%1 / %2 / %3").arg(o["colormatrix"].toString(), o["primaries"].toString(), o["gamma"].toString()));
    if (o["width"].toInt() > 0) add("mi.hdr", o["hdr"].toString());
    const QString hw = o["hwdec"].toString();
    if (o["width"].toInt() > 0) add("mi.decoding", hw.isEmpty() || hw == "no" ? T("hw.activeSoftware") : T("mi.hardware") + " (" + hw + ")");
    add("mi.audioCodec", o["audioCodecName"].toString().isEmpty() ? o["audioCodec"].toString()
                                                               : o["audioCodec"].toString() + " (" + o["audioCodecName"].toString() + ")");
    add("mi.channels", o["audioChannels"].toString());
    if (o["audioSampleRate"].toInt() > 0) add("mi.sampleRate", QString::number(o["audioSampleRate"].toInt()) + " Hz");
    add("mi.audioBitrate", bitrate(qint64(o["audioBitrate"].toDouble())));
    add("mi.audioOutput", o["audioOutput"].toString());
    for (const QJsonValue &v : o["tracks"].toArray()) {
        const QJsonObject t = v.toObject();
        const QString type = t["type"].toString();
        const char *key = type == "video" ? "mi.trackVideo" : type == "audio" ? "mi.trackAudio" : "mi.trackSub";
        QString d = QString("#%1 %2").arg(t["id"].toInt()).arg(t["codec"].toString());
        if (!t["lang"].toString().isEmpty()) d += " [" + t["lang"].toString() + "]";
        if (!t["title"].toString().isEmpty()) d += " " + t["title"].toString();
        if (t["external"].toBool()) d += " (ext)";
        if (t["selected"].toBool()) d += "  ●";
        rows.append({T(key), d});
    }
    if (o["droppedFrames"].toInt() > 0) add("mi.dropped", QString::number(o["droppedFrames"].toInt()));
    add("mi.error", o["error"].toString());

    auto *v = new QVBoxLayout(this);
    auto *table = new QTableWidget(rows.size(), 2, this);
    table->horizontalHeader()->hide();
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setWordWrap(false);
    QString text;
    for (int i = 0; i < rows.size(); ++i) {
        auto *k = new QTableWidgetItem(rows[i].first);
        QFont f = k->font(); f.setBold(true); k->setFont(f);
        table->setItem(i, 0, k);
        table->setItem(i, 1, new QTableWidgetItem(ltr(rows[i].second)));
        text += rows[i].first + ": " + rows[i].second + "\n";
    }
    table->resizeRowsToContents();
    v->addWidget(table);
    auto *h = new QHBoxLayout;
    auto *copy = new QPushButton(T("mi.copy"));
    auto *close = new QPushButton(T("dlg.close"));
    close->setDefault(true);
    h->addWidget(copy);
    h->addStretch();
    h->addWidget(close);
    v->addLayout(h);
    connect(copy, &QPushButton::clicked, this, [text] { QGuiApplication::clipboard()->setText(text); });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}
