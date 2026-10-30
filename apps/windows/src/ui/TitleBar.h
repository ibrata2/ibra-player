#pragma once
#include <QWidget>
class QLabel;
class QToolButton;
class QMenu;
class QHBoxLayout;

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *window, QWidget *parent = nullptr);
    // Replaces the menu buttons (title, menu) in order. Never collapses into an
    // overflow button, whatever the language or font width.
    void setMenus(const QList<QPair<QString, QMenu *>> &menus);
    void setMediaTitle(const QString &t);
    void updateIcons();
    void updateMaximized();
protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *) override;
private:
    QWidget *m_window;
    QLabel *m_logo, *m_title, *m_media;
    QWidget *m_menuStrip;
    QHBoxLayout *m_menuLayout;
    QToolButton *m_min, *m_max, *m_close;
};
