#include "entrytreemodel.h"

#include "core/util.h"

namespace psg {
namespace {

struct NodeRow {
    int node;
    int depth;
    bool folder;
};

} // namespace

EntryTreeModel::EntryTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void EntryTreeModel::setEntries(const QList<Entry> &entries)
{
    beginResetModel();
    m_entries = entries;
    m_nodes = buildEntryTree(entries);
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
        insertSubtree(0, 0);
}

void EntryTreeModel::insertSubtree(int node, int depth)
{
    const EntryTreeNode &n = m_nodes.at(node);
    for (int ci : n.childDirs) {
        m_visible.append({ci, depth + 1, true});
        if (m_expanded.contains(ci))
            insertSubtree(ci, depth + 1);
    }
    for (const EntryTreeFile &f : n.files)
        m_visible.append({-1 - f.entryIndex, depth + 1, false});
}

void EntryTreeModel::toggle(int row)
{
    if (row < 0 || row >= m_visible.size())
        return;
    const VisibleRow vr = m_visible.at(row);
    if (!vr.folder)
        return;
    const int node = vr.node;
    if (m_expanded.contains(node))
        m_expanded.remove(node);
    else
        m_expanded.insert(node);
    beginResetModel();
    rebuild();
    endResetModel();
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

void EntryTreeModel::expandToDepth(int depth)
{
    beginResetModel();
    m_expanded.clear();
    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes.at(i).path.count(u'/') < depth)
            m_expanded.insert(i);
    }
    if (!m_nodes.isEmpty()) {
        // keep root children open
        for (int ci : m_nodes.at(0).childDirs)
            m_expanded.insert(ci);
    }
    rebuild();
    endResetModel();
    emit countChanged();
}

int EntryTreeModel::entryIndexAt(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return -1;
    const VisibleRow vr = m_visible.at(row);
    return vr.folder ? -1 : -1 - vr.node;
}

QString EntryTreeModel::pathAt(int row) const
{
    const int idx = entryIndexAt(row);
    if (idx < 0 || idx >= m_entries.size())
        return m_visible.value(row).node < m_nodes.size()
            ? m_nodes.at(m_visible.at(row).node).path
            : QString();
    return m_entries.at(idx).name;
}

bool EntryTreeModel::isFolderAt(int row) const
{
    return row >= 0 && row < m_visible.size() && m_visible.at(row).folder;
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
    case EntryIndexRole: return entryIndex;
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
