#pragma once

#include "../types.h"

class QFile;

namespace psg::ps3 {

// PS3 NPDRM PKG: retail (AES-CTR keystream with the fixed key) and debug
// (SHA1 XOR keystream) listings, plus PARAM.SFO metadata.
GameInfo parsePs3Pkg(const QString &path);

// Decrypt `size` bytes at stream offset `pos` of the NPDRM data section.
// Shared by the viewer and the extractor. Returns empty on failure.
QByteArray decryptRange(QFile &f, quint64 dataOff, bool retail,
                        const QByteArray &keymat, quint64 pos, quint64 size);

} // namespace psg::ps3
