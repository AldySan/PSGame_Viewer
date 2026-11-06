#pragma once

#include <QByteArray>
#include <QVariantMap>

namespace psg {

// Parse a PS3/PS4 param.sfo blob into a map. String entries become
// QString, u32 entries become uint, anything else becomes a hex string.
// Never throws; sets *error for a bad magic or truncated table and
// returns a map with an "_error" key (matching the historical behaviour).
QVariantMap parseSfo(const QByteArray &data);

} // namespace psg
