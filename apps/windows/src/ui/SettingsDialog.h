#pragma once
#include <QDialog>
#include <QHash>
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class QFormLayout;
class QComboBox;
class QCheckBox;
class QSpinBox;
class MpvWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent);
    void selectPage(const QString &id);
signals:
    void openThemeStudio();
private:
    QFormLayout *addPage(const QString &id, const char *titleKey);
    QCheckBox *check(QFormLayout *f, const char *labelKey, const QString &key);
    QSpinBox *spin(QFormLayout *f, const char *labelKey, const QString &key, int lo, int hi, const QString &suffix = QString(), int step = 1);
    QComboBox *combo(QFormLayout *f, const char *labelKey, const QString &key, const QList<QPair<QString, QString>> &items);
    QWidget *colorButton(const QString &key, bool alpha);
    void buildPages();
    void buildShortcutsPage();
    void buildCodecsPage();
    QListWidget *m_nav;
    QStackedWidget *m_stack;
    QHash<QString, int> m_pageIndex;
    MpvWidget *m_engine = nullptr;
};
