#pragma once

#include <QFile>
#include <QList>
#include <QString>

namespace psg {

// Read-only ISO9660 walker (2048-byte sectors), shared by the PS2 and
// PS3 disc viewers. Files on PS discs are contiguous extents, so the
// entries carry plain absolute offsets for streaming/extract.
struct IsoEntry {
    QString name;     // path with "/" separators
    quint64 size = 0;
    quint64 offset = 0; // absolute byte offset (lba * 2048)
    bool isDir = false;
};

class Iso9660 {
public:
    // Validate the Primary Volume Descriptor and return the volume label
    // plus the root directory record. Fails for non-ISO images.
    static bool open(QFile &f, QString *volumeLabel, quint32 *rootLba,
                     quint32 *rootSize, QString *error);

    // Walk the whole directory tree (records never cross sector bounds).
    static QList<IsoEntry> walk(QFile &f, quint32 rootLba, quint32 rootSize,
                                int maxEntries = 200000);

    static QString cleanName(const QByteArray &raw);
};

} // namespace psg
