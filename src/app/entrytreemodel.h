#pragma once

#include <QAbstractListModel>
#include <QList>

#include "core/entrytree.h"

namespace psg {

// Flat, lazily expanded view over the entry folder tree for the Files
// tab: one row per visible node, folders toggled in place.
class EntryTreeModel : public QAbstractListModel
{
    Q_OBJECT
    // NOT constant: the count follows setEntries() while the app runs
    Q_PROPERTY(int totalEntries READ totalEntries NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        PathRole = Qt::UserRole + 1,
        SizeRole,
        SizeTextRole,
        InfoTextRole,
        DepthRole,
        IsFolderRole,
        ExpandedRole,
        EntryIndexRole,
    };

    explicit EntryTreeModel(QObject *parent = nullptr);

    void setEntries(const QList<Entry> &entries);
    Q_INVOKABLE void toggle(int row);
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE int entryIndexAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int totalEntries() const { return m_entries.size(); }
    const QList<Entry> &entries() const { return m_entries; }

signals:
    void countChanged();

private:
    struct VisibleRow {
        int node = 0;
        int depth = 0;
        bool folder = false;
    };
    void rebuild();
    void appendSubtree(int node, int depth, QList<VisibleRow> *out);

    QList<Entry> m_entries;
    // row-entry index (into m_entries) -> index in the caller's full list
    QList<int> m_map;
    QList<EntryTreeNode> m_nodes;
    QList<VisibleRow> m_visible;
    QSet<int> m_expanded;
};

} // namespace psg
