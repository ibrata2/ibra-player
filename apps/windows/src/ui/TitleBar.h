#pragma once
#include <QWidget>
class QLabel;
class QToolButton;
class QMenuBar;

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *window, QWidget *parent = nullptr);
    QMenuBar *menuBar() const { return m_menu; }
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
    QMenuBar *m_menu;
    QToolButton *m_min, *m_max, *m_close;
};
