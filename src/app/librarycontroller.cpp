#include "librarycontroller.h"

#include "core/detect.h"
#include "core/rename.h"
#include "core/util.h"

#include <QDirIterator>
#include <QFileInfo>

namespace psg {

LibraryController::LibraryController(GameLibrary *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
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
    return m_library ? m_library->recentFiles(20) : QStringList();
}

void LibraryController::refresh()
{
    beginResetModel();
    m_entries = m_library ? m_library->games(m_search) : QList<LibraryEntry>();
    endResetModel();
    emit countChanged();
}

void LibraryController::openAt(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    emit openRequested(QUrl::fromLocalFile(m_entries.at(row).path));
}

void LibraryController::openPath(const QString &path)
{
    if (!path.isEmpty())
        emit openRequested(QUrl::fromLocalFile(path));
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
        const GameInfo info = detect::openAny(f);
        if (!info.ok)
            continue;
        if (m_library->upsertGame(f, info.title,
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
    if (m_library)
        emit statusMessage(tr("Removed %1 missing entries").arg(removed));
    return removed;
}

} // namespace psg
