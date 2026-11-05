#pragma once

#include "../types.h"

namespace psg::ps5 {

// PS5 exFAT game image: sce_sys/param.json + icon0.png via FAT walk,
// plus trophy packs anywhere in the image.
GameInfo parseExfatImage(const QString &path);

// PS5 app dump folder: sce_sys/param.json + icon0.png, AMPR/LZ4 asset
// containers (LIZARD dumps) and trophy packs anywhere in the dump.
GameInfo parseAppFolder(const QString &path);

} // namespace psg::ps5
