#pragma once

#include "../types.h"

class QFile;

namespace psg::ps4 {

struct CntHeader {
    quint32 flags = 0;          // @0x04, bit 31 = finalized/retail
    quint32 entryCount = 0;     // @0x10
    quint16 sysEntryCount = 0;  // @0x14
    quint32 entryTableOff = 0;  // @0x18
    quint64 bodyOff = 0;        // @0x20
    QString contentId;          // @0x40, 48 bytes NUL-padded
};

struct CntEntry {
    quint32 id = 0;
    quint32 nameOff = 0;
    quint32 flags1 = 0;
    quint32 flags2 = 0;
    quint32 dataOff = 0;
    quint32 dataSize = 0;
};

// OFC (retail, bit 31 set) vs FPKG label for the Package row.
QString cntPackageType(quint32 flags);

// Read the CNT header at absolute file offset `base`.
bool readCntHeader(QFile &f, quint64 base, CntHeader *hdr, QString *error);

// Read the 32-byte entry records + the 0x200 name table entry names.
bool readCntEntries(QFile &f, quint64 base, const CntHeader &hdr,
                    QList<CntEntry> *entries, QString *namesBlob,
                    QString *error);

// Full PS4 CNT PKG parse (param.sfo metadata, entry listing, names).
// `containerPath` is the file the offsets refer to; `displayPath` is
// what the user opened (differs for split parts).
GameInfo parsePs4Pkg(const QString &path, const QString &containerPath,
                     quint64 magicBase = 0);

} // namespace psg::ps4
