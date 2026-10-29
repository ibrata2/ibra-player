#pragma once
#include <QObject>
#include <QColor>
#include <QJsonObject>
#include <QVector>

// A glass theme. Every value is user-editable in Theme Studio.
struct Theme {
    QString id;
    QString nameEn, nameAr;
    bool builtIn = false;
    bool dark = true;

    QColor bgTop, bgBottom;          // window backdrop gradient
    QColor blobA, blobB;             // soft light blobs behind the glass (frosted look)
    QColor surface;                  // glass tint
    int surfaceOpacity = 60;         // 0..100
    QColor text, textMuted;
    QColor accent, accentText;       // buttons, sliders, highlights
    QColor border;                   // luminous edge
    int borderOpacity = 45;          // 0..100
    QColor glow;
    int glow_ = 0;                   // glow strength 0..40 px
    int shadow = 18;                 // drop shadow 0..40 px
    int radius = 14;                 // corner radius px
    int buttonSize = 38;             // transport button size px
    int blur = 60;                   // softness of backdrop blobs 0..100
    int reflection = 35;             // top reflection strength 0..100
    QColor videoBackground = Qt::black;

    QString name(bool arabic) const { return arabic && !nameAr.isEmpty() ? nameAr : nameEn; }
    QColor surfaceColor(int extraAlpha = 0) const;
    QColor borderColor(double factor = 1.0) const;

    QJsonObject toJson() const;
    static Theme fromJson(const QJsonObject &o, bool *ok = nullptr);
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager *instance();

    const Theme &current() const { return m_current; }
    QVector<Theme> builtIns() const;
    QVector<Theme> userThemes() const;
    QVector<Theme> allThemes() const { return builtIns() + userThemes(); }
    Theme find(const QString &id) const;

    void apply(const QString &id);            // persists the choice
    void preview(const Theme &t);             // live, not persisted
    void saveUserTheme(const Theme &t);       // persists into settings
    void deleteUserTheme(const QString &id);
    bool exportTheme(const Theme &t, const QString &file) const;
    bool importTheme(const QString &file, Theme *out);

    QString styleSheet(const Theme &t) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    void activate(const Theme &t);
    Theme m_current;
};

QString rgba(const QColor &c, int alpha255 = -1);
