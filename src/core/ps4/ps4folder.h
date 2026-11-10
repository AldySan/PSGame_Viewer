#pragma once

#include "../types.h"

namespace psg::ps4 {

// PS4 app dump folder: sce_sys/param.sfo at its root (eboot.bin,
// sce_module/... around it). The PS5 app layout carries
// sce_sys/param.json instead, so this check is unambiguous.
bool isPs4AppFolder(const QString &path);

// Full folder viewer: param.sfo metadata + the whole file tree.
GameInfo parsePs4Folder(const QString &path);

} // namespace psg::ps4
