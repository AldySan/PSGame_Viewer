#pragma once

#include "../types.h"

namespace psg::ps3 {

// PS3 disc ISO: file list + PS3_GAME/PARAM.SFO metadata. Returns an
// error when the image is not a PS3 disc.
GameInfo parsePs3Iso(const QString &path);

} // namespace psg::ps3
