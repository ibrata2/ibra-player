#pragma once
#include <QIcon>
#include <QColor>
#include <QString>

// Original vector icon set, painted at runtime so it follows the theme colour.
namespace Icons {
QIcon make(const QString &name, const QColor &color, int size = 24);
QPixmap logo(int size);   // application logo from resources
}
