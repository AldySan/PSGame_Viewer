#include "ps3pkg.h"

#include "../aes.h"
#include "../bytes.h"
#include "../sfo.h"
#include "../util.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace psg::ps3 {
namespace {

// Fixed NPDRM key (public, documented on ps3devwiki).
const char kPs3KeyHex[] = "2e7b71d7c9c9a14ea3221f188828b8f8";

constexpr qsizetype kHdrSize = 128;

QByteArray ps3Key()
{
    return QByteArray::fromHex(kPs3KeyHex);
}

// 128-bit big-endian increment used by the retail counter mode.
void addU128(uchar *v, quint64 add)
{
    quint64 carry = add;
    for (int i = 15; i >= 8 && carry; --i) {
        const quint64 cur = v[i];
        const quint64 sum = cur + (carry & 0xFF);
        v[i] = uchar(sum & 0xFF);
        carry = (carry >> 8) + (sum >> 8);
    }
    for (int i = 7; i >= 0 && carry; --i) {
        const quint32 cur = v[i];
        const quint32 sum = cur + quint32(carry & 0xFF);
        v[i] = uchar(sum & 0xFF);
        carry = (carry >> 8) + (sum >> 8);
    }
}

// Debug PKG keystream block: SHA1(qa0, qa0, qa1, qa1, 0*24, block_index)[:16]
QByteArray debugKeystream(const QByteArray &qa, quint64 blockIndex)
{
    uchar buf[64] = {};
    const int qaLen = qMin(qa.size(), 16);
    const int half = qMin(qaLen, 8);
    if (half > 0) {
        memcpy(buf + 0, qa.constData(), size_t(half));
        memcpy(buf + 8, qa.constData(), size_t(half));
    }
    if (qaLen > 8) {
        const int rest = qaLen - 8;
        memcpy(buf + 16, qa.constData() + 8, size_t(rest));
        memcpy(buf + 24, qa.constData() + 8, size_t(rest));
    }
    for (int i = 0; i < 8; ++i)
        buf[56 + i] = uchar((blockIndex >> (8 * (7 - i))) & 0xFF);
    return QCryptographicHash::hash(
        QByteArray(reinterpret_cast<const char *>(buf), sizeof(buf)),
        QCryptographicHash::Sha1).left(16);
}

// QByteArray -> QString with trailing NULs stripped (NPDRM name padding).
QString trimmedUtf8(const QByteArray &b)
{
    int end = b.size();
    while (end > 0 && b.at(end - 1) == '\x00')
        --end;
    return QString::fromUtf8(b.constData(), end);
}

struct ItemRecord {
    quint32 nameOff = 0;
    quint32 nameSize = 0;
    quint64 fileOff = 0;
    quint64 fileSize = 0;
    quint32 flags = 0;
};

} // namespace

QByteArray decryptRange(QFile &f, quint64 dataOff, bool retail,
                        const QByteArray &keymat, quint64 pos, quint64 size)
{
    if (size == 0 || size > quint64(INT_MAX))
        return QByteArray();
    const quint64 bs = pos & ~quint64(0xF);
    const quint64 pre = pos - bs;
    const quint64 nb = (pre + size + 15) / 16;
    if (nb > (1ull << 24))
        return QByteArray();
    f.seek(qint64(dataOff + bs));
    const QByteArray enc = f.read(int(nb * 16));
    if (enc.size() < int(nb * 16))
        return QByteArray();
    QByteArray out(int(nb * 16), Qt::Uninitialized);
    if (!retail) {
        const quint64 bi = bs / 16;
        for (quint64 i = 0; i < nb; ++i) {
            const QByteArray ks = debugKeystream(keymat, bi + i);
            for (int j = 0; j < 16; ++j)
                out[int(i * 16 + j)] = enc.at(int(i * 16 + j)) ^ ks.at(j);
        }
    } else {
        if (keymat.size() != 16)
            return QByteArray();
        const aes::Aes128 aes(ps3Key());
        uchar ctr[16];
        memcpy(ctr, keymat.constData(), 16);
        addU128(ctr, bs / 16);
        for (quint64 i = 0; i < nb; ++i) {
            uchar ks[16];
            aes.encryptBlock(ctr, ks);
            for (int j = 0; j < 16; ++j)
                out[int(i * 16 + j)] = enc.at(int(i * 16 + j)) ^ ks[j];
            addU128(ctr, 1);
        }
    }
    return out.mid(int(pre), int(size));
}

GameInfo parsePs3Pkg(const QString &path)
{
    GameInfo out;
    out.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("cannot open file");
        return out;
    }
    out.size = f.size();

    const QByteArray hdr = f.read(kHdrSize);
    if (hdr.size() < kHdrSize) {
        out.error = QStringLiteral("too small for PS3 PKG");
        return out;
    }
    if (hdr.left(4) != QByteArrayLiteral("\x7f" "PKG")) {
        out.error = QStringLiteral("bad PS3 magic");
        return out;
    }
    const quint16 rev = quint16(bytes::rdU16BE(hdr, 4));
    const quint16 typ = quint16(bytes::rdU16BE(hdr, 6));
    const quint32 n = bytes::rdU32BE(hdr, 0x14);
    const quint64 doff = bytes::rdU64BE(hdr, 0x20);
    const quint64 dsz = bytes::rdU64BE(hdr, 0x28);
    const QString cid = QString::fromLatin1(
        hdr.mid(0x30, 48).left(int(hdr.mid(0x30, 48).indexOf('\x00'))));
    const QByteArray qa = hdr.mid(0x60, 16);
    const QByteArray riv = hdr.mid(0x70, 16);
    if (typ != 1) {
        out.error = QStringLiteral("not PS3 NPDRM (type 0x%1)")
                        .arg(typ, 0, 16);
        return out;
    }
    const bool retail = (rev == 0x8000);
    const QByteArray keymat = retail ? riv : qa;

    QVariantMap sfo;
    QList<ItemRecord> recs;
    if (n && n < 1000000 && doff && dsz) {
        const QByteArray tab = decryptRange(f, doff, retail, keymat, 0, quint64(n) * 32);
        if (tab.size() == int(n * 32)) {
            for (quint32 i = 0; i < n; ++i) {
                ItemRecord r;
                const qsizetype o = qsizetype(i) * 32;
                r.nameOff = bytes::rdU32BE(tab, o);
                r.nameSize = bytes::rdU32BE(tab, o + 4);
                r.fileOff = bytes::rdU64BE(tab, o + 8);
                r.fileSize = bytes::rdU64BE(tab, o + 16);
                r.flags = bytes::rdU32BE(tab, o + 24);
                if (r.nameSize > dsz || r.nameOff + r.nameSize > dsz
                    || r.fileOff + r.fileSize > dsz) {
                    continue;
                }
                recs.append(r);
            }
            for (const ItemRecord &r : recs) {
                if (r.fileSize == 0 || r.fileSize >= 1000000)
                    continue;
                const QString nm = trimmedUtf8(
                    decryptRange(f, doff, retail, keymat, r.nameOff, r.nameSize));
                if (nm.endsWith(QLatin1String("PARAM.SFO"), Qt::CaseInsensitive)) {
                    const QByteArray raw =
                        decryptRange(f, doff, retail, keymat, r.fileOff, r.fileSize);
                    if (raw.left(4) == QByteArrayLiteral("\x00PSF"))
                        sfo = parseSfo(raw);
                    break;
                }
            }
        }
    }

    const QString title = sfo.value(QStringLiteral("TITLE")).toString();
    QString tid = sfo.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty())
        tid = titleIdFromCid(cid);
    QString ver;
    if (!sfo.isEmpty())
        ver = sfo.value(QStringLiteral("VERSION")).toString().isEmpty()
            ? sfo.value(QStringLiteral("APP_VER")).toString()
            : sfo.value(QStringLiteral("VERSION")).toString();

    out.ok = true;
    out.platform = Platform::PS3;
    out.title = !title.isEmpty() ? title
              : (!tid.isEmpty() ? tid : QFileInfo(path).fileName());
    out.meta = sfo;
    out.iconEntry = QStringLiteral("ICON0.PNG");
    out.addRow(QStringLiteral("Platform"),
               QStringLiteral("PS3 NPDRM (%1)")
                   .arg(retail ? QStringLiteral("retail") : QStringLiteral("debug")));
    out.addRow(QStringLiteral("Content ID"), cid.isEmpty() ? QStringLiteral("-") : cid);
    out.addRow(QStringLiteral("Title ID"), tid.isEmpty() ? QStringLiteral("-") : tid);
    out.addRow(QStringLiteral("Region"),
               cid.contains(u'-')
                   ? contentRegion(cid)
                   : ps3RegionFromTitleId(tid));
    out.addRow(QStringLiteral("Version"), ver.isEmpty() ? QStringLiteral("-") : ver);
    out.addRow(QStringLiteral("Min. System"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : fmtFw32(sfo.value(QStringLiteral("PS3_SYSTEM_VER"))));
    out.addRow(QStringLiteral("Size"), fmtSize(out.size));

    int entryCount = 0;
    for (int i = 0; i < recs.size(); ++i) {
        if (recs.at(i).nameSize == 0)
            continue;
        const QString nm = trimmedUtf8(
            decryptRange(f, doff, retail, keymat,
                         recs.at(i).nameOff, recs.at(i).nameSize));
        if (nm.isEmpty())
            continue;
        Entry e;
        e.id = quint32(i);
        e.name = nm;
        e.size = recs.at(i).fileSize;
        e.source = Entry::Source::Ps3Crypto;
        e.ps3Retail = retail;
        e.ps3Keymat = keymat;
        e.ps3DataOff = doff;
        e.ps3EntryOff = recs.at(i).fileOff;
        e.isDir = (recs.at(i).flags & 0x04) != 0;
        out.entries.append(e);
        ++entryCount;
    }
    if (entryCount == 0) {
        out.addRow(QStringLiteral("Files"),
                   QStringLiteral("%1 (encrypted)").arg(n));
        out.addRow(QStringLiteral("Note"), QStringLiteral("file table unreadable"));
    } else {
        out.addRow(QStringLiteral("Files"), QString::number(entryCount));
    }
    for (const Entry &e : out.entries) {
        if (e.name.compare(QLatin1String("ICON0.PNG"), Qt::CaseInsensitive) == 0) {
            out.iconEntry = e.name;
            break;
        }
    }
    return out;
}

} // namespace psg::ps3
