#pragma once

#include "types.h"

#include <QList>
#include <QPair>
#include <QStringList>

namespace psg {

// Rename name-component ticks (clean-name preset).
struct RenameParts {
    bool title = true;
    bool tid = true;
    bool ver = true;
    bool region = true;
};

// Clean uniform base name: "Title - TID - vVersion - Region" (empty parts
// dropped, Update/DLC suffix added). Empty when there is not enough info.
QString buildCleanName(const GameInfo &info, const RenameParts &parts = {});

struct FormatResult {
    QString base;
    QStringList missing; // tokens referenced by the pattern with no value
};

// Render a rename pattern ({title} {tid} {cid} {version} {region} {type}
// {orig}); empty tokens are dropped with their separators.
FormatResult formatName(const GameInfo &info, const QString &pattern,
                        const QString &origBase = QString(),
                        const RenameParts &parts = {});

struct RenamePreset {
    QString id;
    QString label;
    QString pattern; // empty = clean-name ticks
};
QList<RenamePreset> renamePresets();

// Expand files/folders into a deduped file list (batch extensions only).
QStringList collectBatchFiles(const QStringList &inputs, bool recursive);

struct BatchItem {
    QStringList files;
    QStringList newFiles;
    QString newBase;
    bool ok = false;
    QString reason;
};

// Build a rename plan; split sets stay together as one item.
QList<BatchItem> previewBatch(const QStringList &files,
                              const QString &pattern = QString(),
                              const RenameParts &include = {});

struct BatchOutcome {
    QList<QPair<QString, QString>> renamed;  // (old, new)
    QList<QPair<QString, QString>> skipped;  // (file, reason)
    QString logPath;
    QString logError;  // set when the revert log could not be written
};

// Execute a plan and write a revert log ("old\tnew" lines).
BatchOutcome applyBatch(const QList<BatchItem> &plan, const QString &logPath);

// Read a revert log -> (current, restore) pairs.
QList<QPair<QString, QString>> parseRevertLog(const QString &path);

// Rename current names back to the originals.
BatchOutcome applyRevert(const QList<QPair<QString, QString>> &pairs,
                         bool onlyExisting = true);

// Batch extensions the rename engine collects.
extern const char *const kBatchExtensions[];

} // namespace psg
