#include "iso9660.h"

#include "bytes.h"

#include <QSet>

namespace psg {
namespace {

constexpr quint32 kSector = 2048;
constexpr quint32 kMaxDirBytes = 64u << 20; // 64 MB sanity cap per directory

} // namespace

QString Iso9660::cleanName(const QByteArray &raw)
{
    // "PARAM.SFO;1" -> "PARAM.SFO"
    QString s = QString::fromUtf8(raw);
    const int semi = s.indexOf(u';');
    if (semi >= 0)
        s.truncate(semi);
    if (s.endsWith(u'.'))
        s.chop(1);
    return s.trimmed();
}

bool Iso9660::open(QFile &f, QString *volumeLabel, quint32 *rootLba,
                   quint32 *rootSize, QString *error)
{
    if (!f.isOpen() && !f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open file");
        return false;
    }
    if (f.size() < qint64(17) * kSector) {
        if (error) *error = QStringLiteral("too small for ISO");
        return false;
    }
    if (!f.seek(qint64(16) * kSector)) {
        if (error) *error = QStringLiteral("cannot seek to ISO descriptor");
        return false;
    }
    const QByteArray pvd = f.read(kSector);
    if (pvd.size() < int(kSector) || uchar(pvd.at(0)) != 0x01
        || pvd.mid(1, 5) != QByteArrayLiteral("CD001")) {
        if (error) *error = QStringLiteral("not an ISO9660 image");
        return false;
    }
    if (volumeLabel)
        *volumeLabel = QString::fromLatin1(pvd.mid(40, 32)).trimmed();
    if (pvd.size() < 190 || uchar(pvd.at(156)) < 34) {
        if (error) *error = QStringLiteral("bad ISO root record");
        return false;
    }
    const quint32 rootExt = bytes::rdU32LE(pvd, 156 + 2);
    const quint32 rootSizeBytes = bytes::rdU32LE(pvd, 156 + 10);
    if (!rootExt || !rootSizeBytes || rootSizeBytes > kMaxDirBytes) {
        if (error) *error = QStringLiteral("bad ISO root directory");
        return false;
    }
    if (rootLba) *rootLba = rootExt;
    if (rootSize) *rootSize = rootSizeBytes;
    return true;
}

QList<IsoEntry> Iso9660::walk(QFile &f, quint32 rootLba, quint32 rootSize,
                              int maxEntries)
{
    struct Frame {
        quint32 ext;
        quint32 size;
        QString prefix;
        int depth;
    };
    QList<IsoEntry> entries;
    QSet<QString> seenDirs; // "lba:path" — same dedupe key as the reference

    QList<Frame> stack = {{rootLba, rootSize, QString(), 0}};
    while (!stack.isEmpty()) {
        const Frame fr = stack.takeLast();
        if (fr.depth > 16 || !fr.ext || !fr.size || fr.size > kMaxDirBytes)
            continue;
        const QString key = QString::number(fr.ext) + u':' + fr.prefix;
        if (seenDirs.contains(key))
            continue;
        seenDirs.insert(key);
        if (!f.seek(qint64(fr.ext) * kSector))
            continue; // seek past EOF would silently read a stale position
        const QByteArray data = f.read(fr.size);
        if (data.size() < int(fr.size))
            continue;

        qsizetype off = 0;
        QList<Frame> subdirs; // walked right after this directory (pre-order)
        while (off < data.size()) {
            if (data.at(off) == 0x00) {
                // records do not cross sector boundaries
                const qsizetype nxt =
                    ((off / qsizetype(kSector)) + 1) * qsizetype(kSector);
                if (nxt >= data.size())
                    break;
                off = nxt;
                continue;
            }
            const int len = uchar(data.at(off));
            if (len < 33 || off + len > data.size())
                break;
            const QByteArray rec = data.mid(off, len);
            off += len;

            const uchar flags = uchar(rec.at(25));
            const quint32 rex = bytes::rdU32LE(rec, 2);
            const quint32 rsz = bytes::rdU32LE(rec, 10);
            const int nlen = uchar(rec.at(32));
            const QByteArray nmRaw = rec.mid(33, nlen);
            if (nmRaw == QByteArrayLiteral("\x00")
                || nmRaw == QByteArrayLiteral("\x01"))
                continue;
            const QString nm = cleanName(nmRaw);
            if (nm.isEmpty())
                continue;
            const QString full = fr.prefix.isEmpty()
                ? nm : fr.prefix + QLatin1Char('/') + nm;
            if (flags & 0x02) {
                subdirs.append({rex, rsz, full, fr.depth + 1});
            } else {
                if (entries.size() >= maxEntries)
                    return entries;
                entries.append({full, rsz, quint64(rex) * kSector, false});
            }
        }
        // push reversed so the first subdirectory is processed first
        for (int i = subdirs.size() - 1; i >= 0; --i)
            stack.append(subdirs.at(i));
    }
    return entries;
}

} // namespace psg
