#pragma once
#include <QString>
#include <QObject>

// Lightweight bilingual string table (Arabic / English). Switching language is
// live: widgets listen to I18n::instance()->languageChanged and retranslate.
class I18n : public QObject {
    Q_OBJECT
public:
    static I18n *instance();
    void setLanguage(const QString &code);   // "ar" | "en"
    QString language() const { return m_lang; }
    bool isRtl() const { return m_lang == "ar"; }
    QString text(const char *key) const;
signals:
    void languageChanged();
private:
    QString m_lang = "en";
};

inline QString T(const char *key) { return I18n::instance()->text(key); }
// Isolate left-to-right content (file names, numbers, times) inside Arabic text
inline QString ltr(const QString &s) { return QChar(0x2066) + s + QChar(0x2069); }
