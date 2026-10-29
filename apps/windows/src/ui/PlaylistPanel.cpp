#include "PlaylistPanel.h"
#include "ControlBar.h"
#include "Icons.h"
#include "core/I18n.h"
#include "core/Library.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>
#include <QKeyEvent>
#include <QMenu>

PlaylistPanel::PlaylistPanel(Playlist *pl, QWidget *parent) : GlassPanel(parent), m_pl(pl) {
    setGlow(false);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(12, 12, 12, 12);
    v->setSpacing(8);

    auto *head = new QHBoxLayout;
    m_header = new QLabel(this);
    m_header->setProperty("heading", true);
    m_count = new QLabel(this);
    m_count->setProperty("muted", true);
    head->addWidget(m_header);
    head->addStretch();
    head->addWidget(m_count);
    v->addLayout(head);

    m_search = new QLineEdit(this);
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &PlaylistPanel::applyFilter);
    v->addWidget(m_search);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setUniformItemSizes(true);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
        emit playRequested(it->data(Qt::UserRole).toInt());
    });
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
        QMetaObject::invokeMethod(this, [this] { syncOrderFromView(); }, Qt::QueuedConnection);
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QMenu m(this);
        auto *it = m_list->itemAt(pos);
        if (it) {
            const int idx = it->data(Qt::UserRole).toInt();
            m.addAction(T("act.play"), this, [this, idx] { emit playRequested(idx); });
            const QString path = m_pl->items().value(idx).path;
            m.addAction(Library::instance()->isFavorite(path) ? T("act.removeFavorite") : T("act.addFavorite"),
                        this, [path] { Library::instance()->toggleFavorite(path); });
            m.addAction(T("pl.remove"), this, [this] { removeSelected(); });
            m.addSeparator();
        }
        m.addAction(T("pl.addFiles"), this, &PlaylistPanel::addFilesRequested);
        m.addAction(T("pl.addFolder"), this, &PlaylistPanel::addFolderRequested);
        m.addAction(T("act.openUrl"), this, &PlaylistPanel::addUrlRequested);
        m.addSeparator();
        m.addAction(T("pl.sort"), this, [this] { m_pl->sortByName(); });
        m.addAction(T("pl.clear"), this, [this] { m_pl->clear(); });
        m.exec(m_list->viewport()->mapToGlobal(pos));
    });
    v->addWidget(m_list, 1);

    auto *tools = new QHBoxLayout;
    tools->setSpacing(2);
    auto mk = [&](const QString &id, const QString &icon) {
        auto *b = new QToolButton(this);
        b->setObjectName("GlassToolButton");
        b->setFixedSize(34, 34);
        b->setIconSize(QSize(18, 18));
        b->setProperty("icon", icon);
        b->setFocusPolicy(Qt::NoFocus);
        m_btn[id] = b;
        tools->addWidget(b);
        return b;
    };
    connect(mk("addFiles", "plus"), &QToolButton::clicked, this, &PlaylistPanel::addFilesRequested);
    connect(mk("addFolder", "folder"), &QToolButton::clicked, this, &PlaylistPanel::addFolderRequested);
    connect(mk("remove", "minus"), &QToolButton::clicked, this, &PlaylistPanel::removeSelected);
    connect(mk("sort", "sort"), &QToolButton::clicked, this, [this] { m_pl->sortByName(); });
    tools->addStretch();
    connect(mk("load", "open"), &QToolButton::clicked, this, &PlaylistPanel::loadPlaylistRequested);
    connect(mk("save", "save"), &QToolButton::clicked, this, &PlaylistPanel::savePlaylistRequested);
    connect(mk("clear", "trash"), &QToolButton::clicked, this, [this] { m_pl->clear(); });
    v->addLayout(tools);

    connect(m_pl, &Playlist::changed, this, &PlaylistPanel::rebuild);
    connect(m_pl, &Playlist::currentChanged, this, &PlaylistPanel::rebuild);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &PlaylistPanel::retheme);
    connect(I18n::instance(), &I18n::languageChanged, this, &PlaylistPanel::retranslate);
    retheme();
    retranslate();
}

void PlaylistPanel::retheme() {
    const QColor c = ThemeManager::instance()->current().text;
    for (auto *b : m_btn) b->setIcon(Icons::make(b->property("icon").toString(), c, 20));
    rebuild();
}

void PlaylistPanel::retranslate() {
    m_header->setText(T("pl.title"));
    m_search->setPlaceholderText(T("pl.search"));
    static const QHash<QString, const char *> tips = {
        {"addFiles", "pl.addFiles"}, {"addFolder", "pl.addFolder"}, {"remove", "pl.remove"},
        {"sort", "pl.sort"}, {"load", "pl.load"}, {"save", "act.savePlaylist"}, {"clear", "pl.clear"}};
    for (auto it = tips.begin(); it != tips.end(); ++it) m_btn[it.key()]->setToolTip(T(it.value()));
    rebuild();
}

void PlaylistPanel::rebuild() {
    const Theme &t = ThemeManager::instance()->current();
    QSignalBlocker block(m_list);
    m_list->clear();
    const auto &items = m_pl->items();
    double total = 0;
    for (int i = 0; i < items.size(); ++i) {
        const auto &pi = items[i];
        double d = pi.duration > 0 ? pi.duration : Library::instance()->knownDuration(pi.path);
        total += d;
        const QString dur = d > 0 ? ControlBar::formatTime(d) : QString("--:--");
        auto *it = new QListWidgetItem(QString("%1.  %2    %3").arg(i + 1).arg(ltr(pi.displayName()), ltr(dur)));
        it->setData(Qt::UserRole, i);
        it->setToolTip(pi.path);
        if (i == m_pl->current()) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
            it->setForeground(t.accent);
            it->setIcon(Icons::make("play", t.accent, 16));
        }
        m_list->addItem(it);
    }
    m_count->setText(ltr(QString("%1 · %2").arg(items.size()).arg(ControlBar::formatTime(total))));
    if (m_pl->current() >= 0 && m_pl->current() < m_list->count())
        m_list->scrollToItem(m_list->item(m_pl->current()), QAbstractItemView::EnsureVisible);
    applyFilter();
}

void PlaylistPanel::applyFilter() {
    const QString q = m_search->text().trimmed();
    for (int i = 0; i < m_list->count(); ++i) {
        auto *it = m_list->item(i);
        it->setHidden(!q.isEmpty() && !it->text().contains(q, Qt::CaseInsensitive));
    }
    // Reordering a filtered view would be ambiguous
    m_list->setDragDropMode(q.isEmpty() ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
}

void PlaylistPanel::syncOrderFromView() {
    QVector<int> order;
    for (int i = 0; i < m_list->count(); ++i) order << m_list->item(i)->data(Qt::UserRole).toInt();
    m_pl->setOrder(order);
}

void PlaylistPanel::removeSelected() {
    QList<int> rows;
    for (auto *it : m_list->selectedItems()) rows << it->data(Qt::UserRole).toInt();
    m_pl->removeAt(rows);
}

void PlaylistPanel::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Delete) { removeSelected(); return; }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (auto *it = m_list->currentItem()) emit playRequested(it->data(Qt::UserRole).toInt());
        return;
    }
    GlassPanel::keyPressEvent(e);
}
