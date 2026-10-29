#include "AboutDialog.h"
#include "Icons.h"
#include "MainWindow.h"
#include "engine/MpvWidget.h"
#include "core/I18n.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSysInfo>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(T("act.about"));
    setFixedWidth(500);
    auto *v = new QVBoxLayout(this);
    v->setSpacing(10);
    v->setContentsMargins(28, 24, 28, 20);

    auto *logo = new QLabel;
    logo->setPixmap(Icons::logo(240).scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    v->addWidget(logo);
    auto *name = new QLabel("iBRA PLAYER");
    name->setAlignment(Qt::AlignCenter);
    name->setStyleSheet("font-size: 20pt; font-weight: 800; letter-spacing: 2px;");
    v->addWidget(name);
    auto *ver = new QLabel(T("about.version").arg(IBRA_VERSION) + "  ·  " + QSysInfo::buildCpuArchitecture());
    ver->setAlignment(Qt::AlignCenter);
    ver->setProperty("muted", true);
    v->addWidget(ver);
    auto *credit = new QLabel("برمجة و تطوير ابراهيم الخزرجي");
    credit->setAlignment(Qt::AlignCenter);
    credit->setStyleSheet("font-size: 12pt; font-weight: 600;");
    v->addWidget(credit);
    auto *links = new QLabel("<a href='https://t.me/ibraprogram'>Telegram: t.me/ibraprogram</a> &nbsp;·&nbsp; "
                             "<a href='https://www.tiktok.com/@ibraprogram'>TikTok: @ibraprogram</a>");
    links->setOpenExternalLinks(true);
    links->setAlignment(Qt::AlignCenter);
    v->addWidget(links);

    QString engine;
    if (auto *mw = qobject_cast<MainWindow *>(parent))
        engine = mw->engine()->getPropertyString("mpv-version") + "  ·  FFmpeg " + mw->engine()->getPropertyString("ffmpeg-version");
    auto *eng = new QLabel(T("about.engine") + "\n" + engine + "\nQt " + qVersion());
    eng->setAlignment(Qt::AlignCenter);
    eng->setProperty("muted", true);
    eng->setWordWrap(true);
    v->addWidget(eng);
    auto *lic = new QLabel(T("about.license"));
    lic->setWordWrap(true);
    lic->setAlignment(Qt::AlignCenter);
    lic->setProperty("muted", true);
    v->addWidget(lic);

    auto *ok = new QPushButton(T("dlg.close"));
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    auto *h = new QHBoxLayout;
    h->addStretch();
    h->addWidget(ok);
    h->addStretch();
    v->addLayout(h);
}
