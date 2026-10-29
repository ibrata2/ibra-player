#pragma once
#include "GlassPanel.h"
#include <QToolButton>
class QLabel;
class QSlider;

class SeekSlider : public QWidget {
    Q_OBJECT
public:
    explicit SeekSlider(QWidget *parent = nullptr);
    void setDuration(double d) { m_duration = d; update(); }
    void setPosition(double p) { if (!m_dragging) { m_position = p; update(); } }
    void setAbLoop(double a, double b) { m_a = a; m_b = b; update(); }
    void setChapters(const QList<double> &c) { m_chapters = c; update(); }
    bool isDragging() const { return m_dragging; }
signals:
    void seekRequested(double seconds, bool final);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
private:
    double posFromX(int x) const;
    double m_duration = 0, m_position = 0, m_a = -1, m_b = -1, m_hoverPos = -1;
    QList<double> m_chapters;
    bool m_dragging = false, m_hover = false;
};

class ControlBar : public GlassPanel {
    Q_OBJECT
public:
    explicit ControlBar(QWidget *parent = nullptr);

    SeekSlider *seek() const { return m_seek; }
    QToolButton *btn(const QString &name) const { return m_buttons.value(name); }
    QSlider *volumeSlider() const { return m_volume; }

    void setTimes(double pos, double dur);
    void setPlaying(bool playing);
    void setMuted(bool muted, int volume);
    void setFullscreen(bool fs);
    void setSpeed(double s);
    void setRepeatShuffle(int repeatMode, bool shuffle);
    static QString formatTime(double sec, bool forceHours = false);

signals:
    void action(const QString &id);
    void volumeMoved(int v);

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

public slots:
    void retheme();
    void retranslate();

private:
    QToolButton *makeButton(const QString &name, const QString &icon, int size = 0);
    SeekSlider *m_seek;
    QLabel *m_cur, *m_total;
    QSlider *m_volume;
    QHash<QString, QToolButton *> m_buttons;
    QHash<QString, QString> m_iconOf;
    bool m_showRemaining = false;
    double m_lastPos = 0, m_lastDur = 0;
};
