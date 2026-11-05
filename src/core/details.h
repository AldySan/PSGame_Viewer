#pragma once

#include "types.h"

namespace psg {

// Curated ("Details" tab, short useful subset) lines "KEY = value".
QStringList curatedMetaLines(Platform platform, const QVariantMap &meta);

// Full dump lines (every key, sorted, private "_error" skipped).
QStringList fullMetaLines(const QVariantMap &meta);

} // namespace psg
