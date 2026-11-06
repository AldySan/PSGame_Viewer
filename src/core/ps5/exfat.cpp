#include "exfat.h"

#include "../bytes.h"

#include <QFileInfo>
#include <QtEndian>

#include <optional>

namespace psg {
namespace {

constexpr quint64 kDirReadCap = 1ull << 24; // 16 MB cap for chained dirs
constexpr quint64 kNoFatReadCap = 1ull << 20; // 1 MB cap for noFAT dirs
// Per-read cap for file payloads (trophy packs are admitted up to 300 MB,
// and readChain/readNoFat must honor them; still bounded, still validated
// against the image size).
constexpr quint64 kFileReadCap = 320000000;
// exFAT spec limits: BytesPerSectorShift <= 12, SectorsPerClusterShift <= 25.
constexpr int kMaxSectorShift = 12;
constexpr int kMaxClusterShift = 25;

} // namespace

ExfatImage::ExfatImage(const QString &path, bool *ok, QString *error)
    : m_path(path), m_file(path)
{
    if (!m_file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        if (error) *error = QStringLiteral("cannot open image");
        return;
    }
    construct(ok, error);
}

ExfatImage::ExfatImage(QIODevice *device, const QString &label, bool *ok,
                       QString *error)
    : m_path(label), m_dev(device)
{
    if (!m_dev || !m_dev->isOpen()
        || !(m_dev->openMode() & QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        if (error) *error = QStringLiteral("cannot open image");
        return;
    }
    construct(ok, error);
}

void ExfatImage::construct(bool *ok, QString *error)
{
    if (ok) *ok = false;
    m_imageSize = quint64(dev()->size());
    const QByteArray bs = dev()->read(512);
    if (bs.size() < 512 || bs.mid(3, 8) != QByteArrayLiteral("EXFAT   ")
        || uchar(bs.at(510)) != 0x55 || uchar(bs.at(511)) != 0xAA) {
        if (error) *error = QStringLiteral("not an exFAT image");
        return;
    }
    // guard the shifts: "1 << n" with n >= 32 is undefined behaviour, so a
    // malformed boot sector must be rejected before the shifts happen
    if (uchar(bs.at(108)) > kMaxSectorShift
        || uchar(bs.at(109)) > kMaxClusterShift) {
        if (error) *error = QStringLiteral("bad exFAT boot sector (shift)");
        return;
    }
    m_bytesPerSector = quint32(1) << uchar(bs.at(108));
    m_sectorsPerCluster = quint32(1) << uchar(bs.at(109));
    m_fatSector = bytes::rdU32LE(bs, 80);
    m_dataSector = bytes::rdU32LE(bs, 88);
    m_rootCluster = bytes::rdU32LE(bs, 96);
    if (m_bytesPerSector == 0 || m_sectorsPerCluster == 0) {
        if (error) *error = QStringLiteral("bad exFAT boot sector");
        return;
    }
    if (ok) *ok = true;
}

quint32 ExfatImage::clusterSector(quint32 cluster) const
{
    return m_dataSector + (cluster - 2) * m_sectorsPerCluster;
}

quint64 ExfatImage::clusterOffset(quint32 cluster) const
{
    return quint64(clusterSector(cluster)) * m_bytesPerSector;
}

bool ExfatImage::clusterInImage(quint32 cluster) const
{
    return cluster >= 2 && clusterOffset(cluster) < m_imageSize;
}

QByteArray ExfatImage::readNoFat(quint32 first, quint64 size) const
{
    if (size == 0 || size > kFileReadCap)
        return QByteArray();
    if (!clusterInImage(first))
        return QByteArray();
    dev()->seek(qint64(clusterOffset(first)));
    return dev()->read(int(size));
}

QByteArray ExfatImage::readChain(quint32 first, quint64 size) const
{
    if (size == 0 || size > kFileReadCap)
        return QByteArray();
    const quint64 clusterBytes =
        quint64(m_bytesPerSector) * m_sectorsPerCluster;
    QByteArray out;
    quint32 clus = first;
    while (clus >= 2 && clusterInImage(clus) && quint64(out.size()) < size) {
        dev()->seek(qint64(clusterOffset(clus)));
        const quint64 want = qMin(clusterBytes, size - quint64(out.size()));
        out += dev()->read(int(want));
        // FAT entry: >= 0xFFFFFFF8 marks the end of the chain
        quint32 next = 0xFFFFFFFF;
        dev()->seek(qint64(quint64(m_fatSector) * m_bytesPerSector
                           + quint64(clus) * 4));
        if (dev()->read(reinterpret_cast<char *>(&next), 4) == 4)
            next = qFromLittleEndian(next);
        else
            next = 0xFFFFFFFF;
        // Builder writes files contiguously but leaves the FAT zeroed:
        // fall through to the next cluster instead of stopping.
        clus = (next >= 0xFFFFFFF8 || next == 0) ? clus + 1 : next;
    }
    if (quint64(out.size()) > size)
        out.truncate(int(size));
    return out;
}

QList<ExfatImage::DirItem> ExfatImage::parseDir(const QByteArray &raw,
                                                const QString &want) const
{
    QList<DirItem> items;
    struct Pending {
        quint16 attrs = 0;
        QStringList names;
        int nsec = 1;
        bool noFat = false;
        quint32 first = 0;
        quint64 size = 0;
        bool haveStream = false;
    };
    std::optional<Pending> pending;

    qsizetype i = 0;
    while (i + 32 <= raw.size()) {
        const uchar etype = uchar(raw.at(i));
        if (etype == 0x00)
            break;
        if (etype == 0x85 && i + 32 <= raw.size()) {
            Pending p;
            p.nsec = int(uchar(raw.at(i + 1)));
            p.attrs = quint16(bytes::rdU16LE(raw, i + 4));
            pending = p;
        } else if (etype == 0xC0 && pending) {
            const quint16 flags = quint16(bytes::rdU16LE(raw, i + 2));
            pending->noFat = (flags & 0x02) != 0;
            pending->first = bytes::rdU32LE(raw, i + 20);
            pending->size = bytes::rdU64LE(raw, i + 24);
            pending->haveStream = true;
        } else if (etype == 0xC1 && pending) {
            QString seg;
            for (int c = 0; c < 15; ++c) {
                const quint16 ch = quint16(bytes::rdU16LE(raw, i + 2 + c * 2));
                if (ch == 0x0000)
                    break;
                seg.append(QChar(ch));
            }
            pending->names.append(seg);
            if (pending->names.size() >= qMax(1, pending->nsec - 1)) {
                DirItem it;
                it.name = pending->names.join(QString());
                it.attrs = pending->attrs;
                it.firstCluster = pending->first;
                it.size = pending->size;
                it.noFat = pending->noFat;
                it.isDir = (pending->attrs & 0x10) != 0;
                items.append(it);
                pending.reset();
                if (!want.isEmpty() && it.name.compare(want, Qt::CaseInsensitive) == 0)
                    break;
            }
        }
        i += 32;
    }
    return items;
}

QList<ExfatImage::DirItem> ExfatImage::listDir(quint32 first, quint64 size,
                                               bool noFat,
                                               const QString &want) const
{
    if (noFat) {
        dev()->seek(qint64(clusterOffset(first)));
        const QByteArray raw = dev()->read(int(qMin<quint64>(size, kNoFatReadCap)));
        return parseDir(raw, want);
    }
    const quint64 clusterBytes =
        quint64(m_bytesPerSector) * m_sectorsPerCluster;
    const quint64 cap = qMin(size, kDirReadCap);
    QByteArray out;
    quint32 clus = first;
    while (clus >= 2 && clusterInImage(clus) && quint64(out.size()) < cap) {
        dev()->seek(qint64(clusterOffset(clus)));
        const quint64 want2 = qMin(clusterBytes, size - quint64(out.size()));
        // `size` comes from directory entries and clusterBytes from the
        // boot sector; both are attacker-controlled and can exceed INT_MAX,
        // so clamp before the int cast (a negative read size is UB-adjacent)
        out += dev()->read(int(qMin<quint64>(want2, quint64(INT_MAX))));
        if (!want.isEmpty()) {
            const QList<DirItem> items = parseDir(out, want);
            for (const DirItem &it : items) {
                if (it.name.compare(want, Qt::CaseInsensitive) == 0)
                    return items;
            }
        }
        quint32 next = 0xFFFFFFFF;
        dev()->seek(qint64(quint64(m_fatSector) * m_bytesPerSector
                           + quint64(clus) * 4));
        if (dev()->read(reinterpret_cast<char *>(&next), 4) == 4)
            next = qFromLittleEndian(next);
        else
            next = 0xFFFFFFFF;
        clus = (next >= 0xFFFFFFF8 || next == 0) ? clus + 1 : next;
    }
    if (quint64(out.size()) > size)
        out.truncate(int(size));
    return parseDir(out, want);
}

bool ExfatImage::find(const QStringList &parts, Found *out) const
{
    quint32 clus = m_rootCluster;
    quint64 size = 1ull << 30;
    bool noFat = false;
    for (int depth = 0; depth < parts.size(); ++depth) {
        const QString &part = parts.at(depth);
        if (depth == 0
            && (part.isEmpty() || part.endsWith(QLatin1String(".exfat"),
                                                Qt::CaseInsensitive)))
            continue;
        const bool last = depth == parts.size() - 1;
        const QString want = last ? QString() : part.toLower();
        bool found = false;
        const QList<DirItem> items = listDir(clus, size, noFat, want);
        for (const DirItem &it : items) {
            if (it.name.compare(part, Qt::CaseInsensitive) != 0)
                continue;
            if (last) {
                if (out) {
                    out->firstCluster = it.firstCluster;
                    out->size = it.size;
                    out->noFat = it.noFat;
                    out->isDir = it.isDir;
                }
                return true;
            }
            if (!it.isDir)
                return false;
            clus = it.firstCluster;
            size = it.size;
            noFat = it.noFat;
            found = true;
            break;
        }
        if (!found)
            return false;
    }
    return false;
}

QList<ExfatImage::DirItem> ExfatImage::listPath(const QStringList &parts) const
{
    quint32 clus = m_rootCluster;
    quint64 size = 1ull << 30;
    bool noFat = false;
    for (int depth = 0; depth < parts.size(); ++depth) {
        const QString &part = parts.at(depth);
        if (depth == 0
            && (part.isEmpty() || part.endsWith(QLatin1String(".exfat"),
                                                Qt::CaseInsensitive)))
            continue;
        const bool last = depth == parts.size() - 1;
        const QString want = last ? QString() : part.toLower();
        bool found = false;
        const QList<DirItem> items = listDir(clus, size, noFat, want);
        for (const DirItem &it : items) {
            if (it.name.compare(part, Qt::CaseInsensitive) != 0)
                continue;
            if (last) {
                if (!it.isDir)
                    return {};
                return listDir(it.firstCluster, it.size, it.noFat);
            }
            if (!it.isDir)
                return {};
            clus = it.firstCluster;
            size = it.size;
            noFat = it.noFat;
            found = true;
            break;
        }
        if (!found)
            return {};
    }
    return {};
}

QList<ExfatPack> exfatScanPacks(const ExfatImage &fs, int maxDirs)
{
    QList<ExfatPack> out;
    const auto addFrom = [&](const QStringList &cand) {
        const QList<ExfatImage::DirItem> items = fs.listPath(cand);
        for (const ExfatImage::DirItem &it : items) {
            if (it.isDir)
                continue;
            const QString ln = it.name.toLower();
            if ((ln.endsWith(QLatin1String(".trp"))
                 || ln.endsWith(QLatin1String(".ucp")))
                && it.size > 0 && it.size < 300000000) {
                ExfatPack p;
                p.relPath = (QStringList(cand) << it.name).join(u'/');
                p.firstCluster = it.firstCluster;
                p.size = it.size;
                p.noFat = it.noFat;
                out.append(p);
            }
        }
    };
    addFrom({QStringLiteral("sce_sys"), QStringLiteral("trophy")});
    addFrom({QStringLiteral("sce_sys"), QStringLiteral("trophy2")});
    addFrom({QStringLiteral("trophy")});
    addFrom({QStringLiteral("trophy2")});
    if (!out.isEmpty())
        return out;

    struct Frame {
        QStringList parts;
        quint32 cluster;
        quint64 size;
        bool noFat;
    };
    QList<Frame> stack = {{QStringList(), fs.rootCluster(), 1ull << 30, false}};
    int seen = 0;
    while (!stack.isEmpty() && seen < maxDirs) {
        const Frame fr = stack.takeLast();
        ++seen;
        const QList<ExfatImage::DirItem> items =
            fs.listDir(fr.cluster, fr.size, fr.noFat);
        for (const ExfatImage::DirItem &it : items) {
            if (it.name.isEmpty() || it.name == QLatin1String(".")
                || it.name == QLatin1String(".."))
                continue;
            if (it.isDir) {
                if (fr.parts.size() < 5)
                    stack.append({fr.parts + QStringList(it.name),
                                  it.firstCluster, it.size, it.noFat});
            } else {
                const QString ln = it.name.toLower();
                if ((ln.endsWith(QLatin1String(".trp"))
                     || ln.endsWith(QLatin1String(".ucp")))
                    && it.size > 0 && it.size < 300000000) {
                    ExfatPack p;
                    p.relPath = (fr.parts + QStringList(it.name)).join(u'/');
                    p.firstCluster = it.firstCluster;
                    p.size = it.size;
                    p.noFat = it.noFat;
                    out.append(p);
                }
            }
        }
    }
    return out;
}

} // namespace psg
