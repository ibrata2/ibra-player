#pragma once
#include "GlassPanel.h"
#include "core/Playlist.h"
class QListWidget;
class QLineEdit;
class QLabel;
class QToolButton;

class PlaylistPanel : public GlassPanel {
    Q_OBJECT
public:
    PlaylistPanel(Playlist *pl, QWidget *parent = nullptr);
signals:
    void playRequested(int index);
    void addFilesRequested();
    void addFolderRequested();
    void addUrlRequested();
    void loadPlaylistRequested();
    void savePlaylistRequested();
public slots:
    void rebuild();
    void retheme();
    void retranslate();
protected:
    void keyPressEvent(QKeyEvent *e) override;
private:
    void applyFilter();
    void syncOrderFromView();
    void removeSelected();
    Playlist *m_pl;
    QLabel *m_header, *m_count;
    QLineEdit *m_search;
    QListWidget *m_list;
    QHash<QString, QToolButton *> m_btn;
};
