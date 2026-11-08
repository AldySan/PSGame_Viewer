#include "entrytreemodel.h"

#include "core/util.h"

namespace psg {

EntryTreeModel::EntryTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void EntryTreeModel::setEntries(const QList<Entry> &entries)
{
    beginResetModel();
    // PS4/PS5 CNT containers carry a system entry (id 0x200) that holds
    // the file-name table and has no name of its own; listing it just
    // shows a blank row. Keep nameless entries out of the listing but
    // remember where each listed entry came from, so the row -> entry
    // indices handed to the extraction flow stay valid.
    m_entries.clear();
    m_map.clear();
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).name.trimmed().isEmpty())
            continue;
        m_entries.append(entries.at(i));
        m_map.append(i);
    }
    m_nodes = buildEntryTree(m_entries);
    m_expanded.clear();
    // root level starts open (like the reference's open_depth=1)
    if (!m_nodes.isEmpty()) {
        for (int ci : m_nodes.at(0).childDirs)
            m_expanded.insert(ci);
    }
    rebuild();
    endResetModel();
    emit countChanged();
}

void EntryTreeModel::rebuild()
{
    m_visible.clear();
    if (!m_nodes.isEmpty())
        appendSubtree(0, 0, &m_visible);
}

void EntryTreeModel::appendSubtree(int node, int depth, QList<VisibleRow> *out)
{
    const EntryTreeNode &n = m_nodes.at(node);
    for (int ci : n.childDirs) {
        out->append({ci, depth + 1, true});
        if (m_expanded.contains(ci))
            appendSubtree(ci, depth + 1, out);
    }
    for (const EntryTreeFile &f : n.files)
        out->append({-1 - f.entryIndex, depth + 1, false});
}

void EntryTreeModel::toggle(int row)
{
    if (row < 0 || row >= m_visible.size())
        return;
    const VisibleRow vr = m_visible.at(row);
    if (!vr.folder)
        return;
    const int node = vr.node;
    if (m_expanded.contains(node)) {
        // collapse: the folder's rows are the contiguous run of deeper
        // rows directly below it - remove exactly that span instead of
        // resetting the model (a reset re-instantiates every delegate
        // and loses the scroll position on large containers)
        int end = row + 1;
        while (end < m_visible.size() && m_visible.at(end).depth > vr.depth)
            ++end;
        if (end > row + 1) {
            beginRemoveRows(QModelIndex(), row + 1, end - 1);
            m_visible.remove(row + 1, end - (row + 1));
            endRemoveRows();
        }
        m_expanded.remove(node);
    } else {
        m_expanded.insert(node);
        // build the subtree rows first so the insert span is known up front
        QList<VisibleRow> sub;
        appendSubtree(node, vr.depth, &sub);
        if (!sub.isEmpty()) {
            beginInsertRows(QModelIndex(), row + 1, row + int(sub.size()));
            for (int i = 0; i < sub.size(); ++i)
                m_visible.insert(row + 1 + i, sub.at(i));
            endInsertRows();
        }
    }
    // the toggled row itself only changed its ExpandedRole
    const QModelIndex self = index(row);
    emit dataChanged(self, self);
    emit countChanged();
}

void EntryTreeModel::expandAll()
{
    beginResetModel();
    for (int i = 1; i < m_nodes.size(); ++i)
        m_expanded.insert(i);
    rebuild();
    endResetModel();
    emit countChanged();
}

void EntryTreeModel::collapseAll()
{
    beginResetModel();
    m_expanded.clear();
    rebuild();
    endResetModel();
    emit countChanged();
}

int EntryTreeModel::entryIndexAt(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return -1;
    const VisibleRow vr = m_visible.at(row);
    if (vr.folder)
        return -1;
    const int idx = -1 - vr.node;
    return idx >= 0 && idx < m_map.size() ? m_map.at(idx) : -1;
}

int EntryTreeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant EntryTreeModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_visible.size())
        return QVariant();
    const VisibleRow vr = m_visible.at(row);
    if (vr.folder) {
        const EntryTreeNode &n = m_nodes.at(vr.node);
        switch (role) {
        case NameRole: return n.name;
        case PathRole: return n.path;
        case SizeRole: return quint64(n.size);
        case SizeTextRole: return fmtSize(n.size);
        case InfoTextRole:
            return n.count == 1 ? QObject::tr("1 file")
                                : QObject::tr("%1 files").arg(n.count);
        case DepthRole: return vr.depth;
        case IsFolderRole: return true;
        case ExpandedRole: return m_expanded.contains(vr.node);
        case EntryIndexRole: return -1;
        default: return QVariant();
        }
    }
    const int entryIndex = -1 - vr.node;
    const Entry &e = m_entries.value(entryIndex);
    switch (role) {
    case NameRole: return e.name.section(u'/', -1, -1);
    case PathRole: return e.name;
    case SizeRole: return e.size;
    case SizeTextRole: return fmtSize(e.size);
    case InfoTextRole: {
        if (!e.codec.isEmpty() && e.codec != QLatin1String("-"))
            return e.codec;
        return QString::number(e.id);
    }
    case DepthRole: return vr.depth;
    case IsFolderRole: return false;
    case ExpandedRole: return false;
    case EntryIndexRole: {
        const int idx = -1 - vr.node;
        return idx >= 0 && idx < m_map.size() ? m_map.at(idx) : -1;
    }
    default: return QVariant();
    }
}

QHash<int, QByteArray> EntryTreeModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {SizeRole, "size"},
        {SizeTextRole, "sizeText"},
        {InfoTextRole, "infoText"},
        {DepthRole, "depth"},
        {IsFolderRole, "isFolder"},
        {ExpandedRole, "expanded"},
        {EntryIndexRole, "entryIndex"},
    };
}

} // namespace psg
