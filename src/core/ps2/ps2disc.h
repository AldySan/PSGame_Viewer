#pragma once

#include "../types.h"

namespace psg::ps2 {

// PS2 disc ISO (ISO9660, 2048-byte sectors): file list + SYSTEM.CNF meta.
// Detects via SYSTEM.CNF with a cdrom0:\ BOOT2 executable; the title is
// derived from the file name (PS2 discs embed none).
GameInfo parsePs2Iso(const QString &path);

// SYSTEM.CNF key/value text -> map with upper-cased keys.
QVariantMap parseSystemCnf(const QByteArray &raw);

} // namespace psg::ps2
