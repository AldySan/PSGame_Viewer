#pragma once

#include "types.h"

#include <functional>

namespace psg {

// Entry name -> safe relative path (keeps subdirs, kills ".." and drives).
QString sanitizePkgPath(const QString &name);

// Read one entry's bytes (dispatches over the entry source kinds).
// Returns an empty array above `limit` or on failure.
QByteArray readEntryBytes(const Entry &e, const QString &containerPath,
                          quint64 limit = 32000000);

struct ExtractResult {
    int ok = 0;
    int fail = 0;
};

// Extract entries preserving the folder structure. `progress` gets the
// relative path of each attempted entry.
ExtractResult extractEntries(const QList<Entry> &entries,
                             const QString &containerPath,
                             const QString &destDir,
                             const std::function<void(const QString &)> &progress
                             = nullptr);

// Write a minimal .gp4 project for orbis-pub-gen from parsed metadata.
QString writeGp4(const GameInfo &info, const QString &extractedDir,
                 const QString &gp4Path);

} // namespace psg
