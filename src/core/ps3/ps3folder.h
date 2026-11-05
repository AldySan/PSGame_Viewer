#pragma once

#include "../types.h"

namespace psg::ps3 {

// Root of PS3 game data: the directory itself (NPDRM extract) or
// PS3_GAME/ inside it (disc extract). Empty string when not a PS3 folder.
QString ps3FolderBase(const QString &path);

// PS3 game folder: PARAM.SFO metadata, PNG gallery from the base folder
// and trophy packs from TROPDIR/.
GameInfo parsePs3Folder(const QString &path);

} // namespace psg::ps3
