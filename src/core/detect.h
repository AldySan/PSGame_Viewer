#pragma once

#include "types.h"

namespace psg::detect {

// Open any supported game file or folder: PS2/PS3 ISOs, PS3 PKG/folders,
// PS4/PS5 PKGs (incl. split parts and retail stubs), PS5 exFAT images and
// app folders. The GameInfo carries the error for unsupported input.
GameInfo openAny(const QString &path);

// All parts of a split set (base_0.pkg, base_1.pkg, ...), [path] otherwise.
QStringList splitSetSiblings(const QString &path);

} // namespace psg::detect
