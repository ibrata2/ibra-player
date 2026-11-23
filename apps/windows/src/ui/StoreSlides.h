#pragma once
#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include <QHash>
#include <QVariantAnimation>

class QNetworkAccessManager;

// Slideshow of the newest programs and games on IBRA STORE (ibraprogram.netlify.app),
// shown on the idle screen. Takes its font, colours and accent from the current app theme.
// Stays hidden if the list cannot be fetched.
class StoreSlides : public QWidget {
    Q_OBJECT
public:
    explicit StoreSlides(QWidget *parent = nullptr);

    // true once at least one app was loaded and the "ui/storeSlides" setting is on
    bool hasContent() const;
    // preferred height for a given width
    int heightForWidth(int w) const override;

signals:
    void contentChanged();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    struct Item {
        QString name, slug, tagline, category, createdAt, iconUrl, bgUrl;
    };
    void fetch();
    void loadImage(const QString &url);
    void go(int index, int dir);
    void step(int dir) { if (!m_items.isEmpty()) go((m_cur + dir + m_items.size()) % m_items.size(), dir); }
    QColor accent() const;
    void paintSlide(QPainter &p, const QRectF &r, const Item &it, qreal opacity);
    QString appUrl(const Item &it) const;
    QRectF cardRect() const;
    QRectF arrowRect(int dir) const;   // dir -1 = previous, +1 = next (visual side follows RTL)
    QRectF buttonRect(const QRectF &card) const;
    QRectF dotRect(int i) const;

    QNetworkAccessManager *m_net = nullptr;
    QList<Item> m_items;
    QHash<QString, QPixmap> m_images;   // url -> loaded image (null pixmap = loading / failed)
    QHash<QString, QPixmap> m_soft;     // url -> small copy, drawn enlarged as a soft (blurred) backdrop
    QTimer m_timer;
    QVariantAnimation m_anim;
    int m_cur = 0, m_prev = -1, m_dir = 1;
    bool m_hover = false;
    QString m_hot;                      // "prev", "next", "btn", "card", "dot<i>"
};
