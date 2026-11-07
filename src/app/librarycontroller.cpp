#include "librarycontroller.h"

#include "core/detect.h"
#include "core/rename.h"
#include "core/util.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace psg {

namespace {
// how many recent files the home screen shows (and keeps)
constexpr int kMaxRecentFiles = 5;
} // namespace

LibraryController::LibraryController(GameLibrary *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    if (m_library && !m_library->isValid())
        m_error = m_library->lastError();
}

int LibraryController::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant LibraryController::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_entries.size())
        return QVariant();
    const LibraryEntry &e = m_entries.at(row);
    switch (role) {
    case TitleRole: return e.title;
    case PathRole: return e.path;
    case PlatformRole: return e.platform;
    case TitleIdRole: return e.titleId;
    case ContentIdRole: return e.contentId;
    case RegionRole: return e.region;
    case VersionRole: return e.version;
    case SizeRole: return e.size;
    case SizeTextRole: return fmtSize(e.size);
    case AddedTextRole:
        return e.addedAt.toString(QStringLiteral("yyyy-MM-dd"));
    default: return QVariant();
    }
}

QHash<int, QByteArray> LibraryController::roleNames() const
{
    return {
        {TitleRole, "title"},
        {PathRole, "path"},
        {PlatformRole, "platform"},
        {TitleIdRole, "titleId"},
        {ContentIdRole, "contentId"},
        {RegionRole, "region"},
        {VersionRole, "version"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {AddedTextRole, "addedText"},
    };
}

void LibraryController::setSearch(const QString &search)
{
    if (m_search == search)
        return;
    m_search = search;
    refresh();
}

QStringList LibraryController::recentFiles() const
{
    if (!m_library)
        return {};
    // dedupe before capping: the same file may sit in the table under
    // two spellings (legacy lower-case drive letters), and two copies
    // of the same file share name and size — show it once, five at most
    const QStringList raw = m_library->recentFiles(20);
    QStringList out;
    QSet<QString> seenPaths;
    QSet<QString> seenNameSize;
    for (const QString &path : raw) {
        if (out.size() >= kMaxRecentFiles)
            break;
        const QString norm = normalizeGamePath(path);
        if (!seenPaths.contains(norm)) {
            const QFileInfo fi(norm);
            const QString nameSize = fi.fileName() + u'\n'
                + QString::number(fi.size());
            if (!seenNameSize.contains(nameSize)) {
                seenPaths.insert(norm);
                seenNameSize.insert(nameSize);
                out.append(path);
            }
        }
    }
    return out;
}

void LibraryController::refresh()
{
    beginResetModel();
    const QList<LibraryEntry> all =
        m_library ? m_library->games(m_search) : QList<LibraryEntry>();
    // rows written before path normalization may coexist in two
    // spellings ("C:/..." and "c:/..."): show one — games() sorts by
    // recency, so the first hit is the freshest row
    m_entries.clear();
    QSet<QString> seen;
    for (const LibraryEntry &e : all) {
        const QString key = normalizeGamePath(e.path);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        m_entries.append(e);
    }
    endResetModel();
    emit countChanged();
}

void LibraryController::removeAt(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    m_library->removePath(m_entries.at(row).path);
    refresh();
}

int LibraryController::scanFolder(const QUrl &folder, bool recursive)
{
    if (!m_library)
        return 0;
    const QString dir = folder.isLocalFile() ? folder.toLocalFile()
                                             : folder.toString();
    // same extensions the rename engine collects
    const QStringList files = collectBatchFiles({dir}, recursive);
    int added = 0;
    for (const QString &f : files) {
        const QString canon = normalizeGamePath(f);
        const GameInfo info = detect::openAny(canon);
        if (!info.ok)
            continue;
        if (m_library->upsertGame(canon, info.title,
                                  platformName(info.platform), info.titleId(),
                                  info.contentId(), info.region(),
                                  info.version(), info.size) > 0)
            ++added;
    }
    refresh();
    emit statusMessage(tr("Scanned %1, added %2").arg(files.size()).arg(added));
    return added;
}

int LibraryController::removeMissing()
{
    const int removed = m_library ? m_library->removeMissing() : 0;
    refresh();
    if (m_library) {
        // never silent: "nothing to clean" is feedback too, and without
        // it the button looks like a no-op
        emit statusMessage(removed > 0
                               ? tr("Removed %1 missing entries").arg(removed)
                               : tr("All games present — nothing to clean"));
    }
    return removed;
}

} // namespace psg
