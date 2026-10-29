#pragma once
#include <QDialog>
#include <QTimer>
#include "theme/Theme.h"
class QListWidget;
class QLineEdit;
class QCheckBox;
class QSlider;
class QPushButton;

class ThemeStudio : public QDialog {
    Q_OBJECT
public:
    explicit ThemeStudio(QWidget *parent);
protected:
    void reject() override;
private:
    void loadTheme(const Theme &t);
    void schedulePreview();
    void refreshList();
    void syncEditors();
    Theme m_edit;
    QString m_savedId;          // theme active when the studio opened
    bool m_dirty = false, m_loading = false;
    QTimer m_previewTimer;
    QListWidget *m_list;
    QLineEdit *m_nameEn, *m_nameAr;
    QCheckBox *m_dark;
    QHash<QString, QPushButton *> m_colors;
    QHash<QString, QSlider *> m_sliders;
    QPushButton *m_delete;
};
