#pragma once

#include "../types.h"

namespace psg::ps5 {

// PS5 exFAT game image: sce_sys/param.json + icon0.png via FAT walk,
// plus trophy packs anywhere in the image.
GameInfo parseExfatImage(const QString &path);

// PS5 compressed PFS image (.ffpfsc): a PFS container holding one inner
// file, either an exFAT image or a nested UFS2 .ffpkg image.
GameInfo parseFfpfscImage(const QString &path);

// PS5 UFS2 game image (.ffpkg): sce_sys/param.json + PNGs via an UFS2 walk.
GameInfo parseFfpkgImage(const QString &path);

// PS5 app dump folder: sce_sys/param.json + icon0.png, AMPR/LZ4 asset
// containers (LIZARD dumps) and trophy packs anywhere in the dump.
GameInfo parseAppFolder(const QString &path);

} // namespace psg::ps5
