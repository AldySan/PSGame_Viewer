#pragma once

#include "../types.h"

namespace psg::ps2 {

// PS2 disc dump folder: SYSTEM.CNF with a cdrom0:\ BOOT2 executable at
// its root (extracted-ISO layout). Returns `path` when the layout
// matches, an empty string when it does not.
QString ps2FolderBase(const QString &path);

// Full folder viewer: SYSTEM.CNF metadata + the whole file tree.
GameInfo parsePs2Folder(const QString &path);

} // namespace psg::ps2
