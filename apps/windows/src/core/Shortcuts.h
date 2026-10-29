#pragma once
#include <QKeySequence>
#include <QString>
#include <QVector>

// Customisable keyboard shortcuts. Each action has a stable id, a translation
// key for its label and a default key sequence (MPC-HC inspired).
struct ShortcutDef {
    const char *id;
    const char *labelKey;
    const char *defaultKeys;
};

namespace Shortcuts {
const QVector<ShortcutDef> &all();
QKeySequence get(const QString &id);
void set(const QString &id, const QKeySequence &seq);
void resetAll();
const ShortcutDef *find(const QString &id);
}
