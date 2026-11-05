#pragma once

#include "types.h"

namespace psg {

// Folder tree built from flat entry names, for the Files tab. Names may
// carry a path ("PS3_GAME/USRDIR/EBOOT.BIN"); plain names stay at the
// root. A name ending in "/" is a folder with no file. Node 0 is the
// root; children are stored as indexes into the flat pool.
struct EntryTreeFile {
    QString name;
    int entryIndex = 0;
    quint64 size = 0;
    QString codec;
};

struct EntryTreeNode {
    QString name;
    QString path;
    QList<int> childDirs;
    QList<EntryTreeFile> files;
    quint64 size = 0; // summed size of files below (folders are not files)
    int count = 0;    // number of files below
};

QList<EntryTreeNode> buildEntryTree(const QList<Entry> &entries);

} // namespace psg
