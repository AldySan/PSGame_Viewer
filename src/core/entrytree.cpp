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

    const auto nodeFor = [&](int parent, const QString &part) -> int {
        EntryTreeNode &p = pool[parent];
        for (int ci : p.childDirs) {
            if (pool.at(ci).name == part)
                return ci;
        }
        EntryTreeNode n;
        n.name = part;
        n.path = p.path.isEmpty() ? part : p.path + u'/' + part;
        const int idx = pool.size();
        pool.append(n);
        pool[parent].childDirs.append(idx);
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
            bool exists = false;
            for (int ci : pool[node].childDirs) {
                if (pool.at(ci).name == leaf) {
                    exists = true;
                    break;
                }
            }
            if (!exists)
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
