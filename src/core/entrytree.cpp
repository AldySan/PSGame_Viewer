#include "entrytree.h"

#include <QHash>

namespace psg {
namespace {

// Case-insensitive sort like the reference UI.
bool nameLessThan(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

} // namespace

QList<EntryTreeNode> buildEntryTree(const QList<Entry> &entries)
{
    QList<EntryTreeNode> pool;
    pool.append(EntryTreeNode()); // root
    // name -> pool index, parallel to `pool`: the child lookup below is
    // O(1) instead of a linear scan, which went quadratic on containers
    // with thousands of sibling folders (50M string compares at 10k dirs)
    QList<QHash<QString, int>> childLookup;
    childLookup.append(QHash<QString, int>());

    const auto nodeFor = [&](int parent, const QString &part) -> int {
        const auto it = childLookup.at(parent).constFind(part);
        if (it != childLookup.at(parent).cend())
            return it.value();
        EntryTreeNode n;
        n.name = part;
        n.path = pool.at(parent).path.isEmpty()
                     ? part
                     : pool.at(parent).path + u'/' + part;
        const int idx = pool.size();
        pool.append(n);
        childLookup.append(QHash<QString, int>());
        pool[parent].childDirs.append(idx);
        childLookup[parent].insert(part, idx);
        return idx;
    };

    for (int idx = 0; idx < entries.size(); ++idx) {
        QString name = entries.at(idx).name.trimmed();
        if (name.isEmpty())
            continue;
        name.replace(u'\\', u'/');
        const bool isDir = name.endsWith(u'/');
        QStringList parts;
        for (const QString &p : name.split(u'/')) {
            if (p.isEmpty() || p == QLatin1String(".") || p == QLatin1String(".."))
                continue;
            parts.append(p);
        }
        if (parts.isEmpty())
            continue;
        const quint64 size = entries.at(idx).size;

        int node = 0;
        for (int i = 0; i < parts.size() - 1; ++i) {
            node = nodeFor(node, parts.at(i));
            if (!isDir) { // folders are not files: keep counts honest
                pool[node].size += size;
                pool[node].count += 1;
            }
        }
        const QString leaf = parts.last();
        if (isDir) {
            if (!childLookup.at(node).contains(leaf))
                nodeFor(node, leaf);
            continue;
        }
        EntryTreeFile f;
        f.name = leaf;
        f.entryIndex = idx;
        f.size = size;
        f.codec = entries.at(idx).codec;
        pool[node].files.append(f);
    }

    // case-insensitive ordering of children and files
    for (EntryTreeNode &n : pool) {
        std::sort(n.childDirs.begin(), n.childDirs.end(),
                  [&pool](int a, int b) {
                      return nameLessThan(pool.at(a).name, pool.at(b).name);
                  });
        std::sort(n.files.begin(), n.files.end(),
                  [](const EntryTreeFile &a, const EntryTreeFile &b) {
                      return nameLessThan(a.name, b.name);
                  });
    }
    return pool;
}

} // namespace psg
