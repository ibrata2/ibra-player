#pragma once
#include <QString>

// Local diagnostic log (logs/ibra-player.log in the settings folder).
// File paths are redacted by default so the log can be shared safely.
namespace Log {
void init();
void write(const QString &level, const QString &text);
QString redact(const QString &text);
QString filePath();
}
