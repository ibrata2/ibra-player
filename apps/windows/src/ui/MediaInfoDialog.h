#pragma once
#include <QDialog>
#include <QJsonObject>

class MediaInfoDialog : public QDialog {
    Q_OBJECT
public:
    MediaInfoDialog(const QJsonObject &info, QWidget *parent);
};
