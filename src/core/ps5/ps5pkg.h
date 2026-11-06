#pragma once

#include "../types.h"

#include <utility>

class QFile;

namespace psg::ps5 {

// param.json (PS5 app metadata) -> (title, spec rows). Shared by the
// FIH/LIH PKG viewer, the exFAT image viewer and the app folder viewer.
std::pair<QString, QList<SpecRow>> paramJsonMeta(const QVariantMap &meta);

// Retail (encrypted) PS5 PKG: metadata unreadable, info comes from the
// file name and the split-part set. `fihHeader` is the raw 256-byte
// header of the part-0 file (may be the file itself).
GameInfo parsePs5RetailStub(const QString &path, const QByteArray &fihHeader);

// PS5 finalized PKG: FIH/LIH header wrapping an embedded CNT container
// with param.json. `path` is what the user opened, `containerPath` the
// file that holds the bytes (equal except for split parts).
GameInfo parsePs5Pkg(const QString &path, const QString &containerPath,
                     QFile &f, const QByteArray &hdr, bool isLih);

} // namespace psg::ps5
