#include "ufs2.h"

#include "../bytes.h"

#include <QFileInfo>

namespace psg::ps5 {
namespace {

// UFS2 superblock fields (struct fs, FreeBSD layout, byte offsets).
constexpr quint32 kSuperOffset = 65536;
constexpr quint32 kSuperRead = 0x560; // sizeof(struct fs) == 1376
constexpr quint32 kUfs2Magic = 0x19540119;
constexpr quint32 kRootIno = 2;

constexpr qsizetype offMagic = 0x55C;
constexpr qsizetype offBsize = 0x30;
constexpr qsizetype offFsize = 0x34;
constexpr qsizetype offFrag = 0x38;
constexpr qsizetype offIpg = 0xB8;
constexpr qsizetype offFpg = 0xBC;
constexpr qsizetype offIblkno = 0x10;
constexpr qsizetype offInopb = 0x78;

constexpr qsizetype inoMode = 0x00;
constexpr qsizetype inoSize = 0x10;
constexpr qsizetype inoDb = 0x70;  // di_db[12]
constexpr qsizetype inoIb = 0xD0;  // di_ib[3]
constexpr qsizetype kInodeBytes = 0x100;

constexpr quint8 kDtDir = 4;
constexpr quint8 kDtReg = 8;

} // namespace

Ufs2File::Ufs2File(const QString &path, bool *ok, QString *error)
    : m_label(path), m_owned(path)
{
    if (!m_owned.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open UFS2 image");
        if (ok) *ok = false;
        return;
    }
    m_dev = &m_owned;
    m_label = QFileInfo(path).fileName();
    init(ok, error);
}

Ufs2File::Ufs2File(QIODevice *device, const QString &label, bool *ok,
                   QString *error)
    : m_label(label), m_dev(device)
{
    init(ok, error);
}

void Ufs2File::init(bool *ok, QString *error)
{
    if (ok) *ok = false;
    const QString where =
        m_label.isEmpty() ? QString() : QStringLiteral("%1: ").arg(m_label);
    const auto fail = [&](const QString &msg) {
        if (error) *error = where + msg;
    };

    if (!m_dev || !m_dev->isOpen()) {
        fail(QStringLiteral("device is not open"));
        return;
    }
    m_imageSize = quint64(m_dev->size());
    QByteArray sb;
    if (!readAt(kSuperOffset, kSuperRead, sb)
        || sb.size() < int(kSuperRead)) {
        if (m_imageSize < kSuperOffset + quint64(kSuperRead)) {
            fail(QStringLiteral("not a UFS2 image (too small)"));
            return;
        }
        fail(QStringLiteral("cannot read the superblock"));
        return;
    }
    if (bytes::rdU32LE(sb, offMagic) != kUfs2Magic) {
        fail(QStringLiteral("not a UFS2 image (magic mismatch)"));
        return;
    }
    m_bsize = bytes::rdU32LE(sb, offBsize);
    m_fsize = bytes::rdU32LE(sb, offFsize);
    m_frag = bytes::rdU32LE(sb, offFrag);
    m_ipg = bytes::rdU32LE(sb, offIpg);
    m_fpg = bytes::rdU32LE(sb, offFpg);
    m_iblkno = bytes::rdU32LE(sb, offIblkno);
    m_inopb = bytes::rdU32LE(sb, offInopb);
    if (m_bsize < 4096 || m_bsize > (1u << 20)
        || (m_bsize & (m_bsize - 1)) != 0) {
        fail(QStringLiteral("bad UFS2 block size"));
        return;
    }
    if (m_fsize == 0 || m_fsize > m_bsize || m_bsize % m_fsize != 0
        || m_frag != m_bsize / m_fsize) {
        fail(QStringLiteral("bad UFS2 fragment geometry"));
        return;
    }
    if (m_inopb != m_bsize / kInodeBytes) {
        fail(QStringLiteral("bad UFS2 inode geometry"));
        return;
    }
    if (m_ipg < 16 || m_ipg > (1u << 20)) {
        fail(QStringLiteral("bad UFS2 inode count per group"));
        return;
    }
    if (m_fpg <= m_iblkno + m_ipg / m_inopb * m_frag || m_fpg > (1u << 24)) {
        fail(QStringLiteral("bad UFS2 group geometry"));
        return;
    }

    // sanity: the root directory must start with "." pointing at inode 2
    const Inode root = inodeAt(kRootIno);
    if (!root.valid || !root.isDir || root.size == 0 || root.size > (4u << 20)) {
        fail(QStringLiteral("bad UFS2 root inode"));
        return;
    }
    const QByteArray first = readRange(root, 0, qMin<quint64>(root.size, 4096));
    if (first.size() < 24) {
        fail(QStringLiteral("cannot read the UFS2 root directory"));
        return;
    }
    const quint32 dotIno = bytes::rdU32LE(first, 0);
    const quint16 dotReclen = quint16(bytes::rdU16LE(first, 4));
    const quint8 dotType = quint8(first.at(6));
    const quint8 dotNamelen = quint8(first.at(7));
    const QString dotName = QString::fromLatin1(first.mid(8, int(dotNamelen)));
    if (dotIno != kRootIno || dotReclen < 12 || dotType != kDtDir
        || dotName != QLatin1String(".")) {
        fail(QStringLiteral("root directory mismatch (unsupported layout)"));
        return;
    }
    m_ok = true;
    if (ok) *ok = true;
}

bool Ufs2File::readAt(quint64 at, quint64 len, QByteArray &out) const
{
    if (len > m_imageSize || at > m_imageSize - len)
        return false;
    if (!m_dev->seek(qint64(at)))
        return false;
    out = m_dev->read(int(len));
    return quint64(out.size()) == len;
}

Ufs2File::Inode Ufs2File::inodeAt(quint32 ino) const
{
    Inode out;
    if (ino == 0)
        return out;
    const quint32 cg = ino / m_ipg;
    const quint32 within = ino % m_ipg;
    const quint64 blockIdx = within / m_inopb;
    const quint64 frag =
        quint64(m_fpg) * cg + m_iblkno + blockIdx * m_frag;
    const quint64 at = frag * m_fsize + quint64(within % m_inopb) * kInodeBytes;
    QByteArray raw;
    if (!readAt(at, kInodeBytes, raw) || raw.size() < int(kInodeBytes))
        return out;
    out.mode = bytes::rdU16LE(raw, inoMode);
    out.size = bytes::rdU64LE(raw, inoSize);
    for (int i = 0; i < 12; ++i)
        out.db[i] = bytes::rdU64LE(raw, inoDb + 8 * i);
    for (int i = 0; i < 3; ++i)
        out.ib[i] = bytes::rdU64LE(raw, inoIb + 8 * i);
    out.isDir = (out.mode & 0040000) != 0;
    out.valid = out.size != 0 && (out.db[0] != 0 || out.ib[0] != 0);
    return out;
}

bool Ufs2File::fragForLbn(const Inode &ino, quint64 lbn, quint64 &frag) const
{
    if (lbn < 12) {
        frag = ino.db[lbn];
        return frag != 0;
    }
    const quint64 perBlock = m_bsize / 8; // i64 pointers per block
    const quint64 first = lbn - 12;
    if (first < perBlock) {
        if (ino.ib[0] == 0)
            return false;
        QByteArray blk;
        if (!readAt(ino.ib[0] * m_fsize, m_bsize, blk))
            return false;
        frag = bytes::rdU64LE(blk, 8 * first);
        return frag != 0;
    }
    const quint64 second = first - perBlock;
    if (second < perBlock * perBlock) {
        if (ino.ib[1] == 0)
            return false;
        QByteArray lvl1;
        if (!readAt(ino.ib[1] * m_fsize, m_bsize, lvl1))
            return false;
        const quint64 outer = second / perBlock;
        const quint64 inner = second % perBlock;
        const quint64 ptr = bytes::rdU64LE(lvl1, 8 * outer);
        if (ptr == 0)
            return false;
        QByteArray lvl2;
        if (!readAt(ptr * m_fsize, m_bsize, lvl2))
            return false;
        frag = bytes::rdU64LE(lvl2, 8 * inner);
        return frag != 0;
    }
    return false; // triple indirect is out of scope for viewer payloads
}

QByteArray Ufs2File::readDirBlock(const Inode &ino, quint64 index) const
{
    quint64 frag = 0;
    if (!fragForLbn(ino, index, frag))
        return QByteArray();
    if (frag * m_fsize >= m_imageSize)
        return QByteArray();
    const quint64 avail = (m_imageSize - frag * m_fsize) / m_fsize;
    const quint64 want = qMin<quint64>(m_bsize / m_fsize, avail);
    QByteArray out;
    if (!readAt(frag * m_fsize, want * m_fsize, out))
        return QByteArray();
    return out;
}

QByteArray Ufs2File::readRange(const Inode &ino, quint64 from, quint64 len) const
{
    if (from >= ino.size || len == 0)
        return QByteArray();
    const quint64 remaining = qMin(len, ino.size - from);
    QByteArray out;
    quint64 done = 0;
    while (done < remaining) {
        const quint64 lbn = (from + done) / m_bsize;
        const quint64 inOff = (from + done) % m_bsize;
        const quint64 blockLen = qMin<quint64>(m_bsize - inOff,
                                                remaining - done);
        const QByteArray blk = readDirBlock(ino, lbn);
        if (blk.isEmpty())
            break;
        const quint64 take = qMin<quint64>(blockLen, blk.size() - inOff);
        if (take == 0)
            break;
        out += blk.mid(int(inOff), int(take));
        done += take;
    }
    return out;
}

bool Ufs2File::find(const QStringList &parts, Found *out) const
{
    quint32 ino = kRootIno;
    for (int depth = 0; depth < parts.size(); ++depth) {
        const QString &part = parts.at(depth);
        if (part.isEmpty())
            continue;
        const Inode node = inodeAt(ino);
        if (!node.valid || !node.isDir)
            return false;
        const QList<DirEntry> items = listDir(ino);
        bool found = false;
        for (const DirEntry &it : items) {
            if (it.name.compare(part, Qt::CaseInsensitive) != 0)
                continue;
            if (depth == parts.size() - 1) {
                if (out) {
                    out->ino = it.ino;
                    out->size = it.size;
                    out->isDir = it.isDir;
                }
                return it.ino != 0;
            }
            if (!it.isDir)
                return false;
            ino = it.ino;
            found = true;
            break;
        }
        if (!found)
            return false;
    }
    return false;
}

QList<Ufs2File::DirEntry> Ufs2File::listDir(quint32 ino) const
{
    QList<DirEntry> out;
    const Inode node = inodeAt(ino);
    if (!node.valid || !node.isDir || node.size == 0 || node.size > (4u << 20))
        return out;
    const QByteArray raw = readRange(node, 0, node.size);
    qsizetype at = 0;
    while (at + 8 <= raw.size()) {
        const quint32 child = bytes::rdU32LE(raw, at + 0);
        const quint16 reclen = quint16(bytes::rdU16LE(raw, at + 4));
        const quint8 type = quint8(raw.at(at + 6));
        const quint8 namelen = quint8(raw.at(at + 7));
        if (reclen < 12 || at + reclen > raw.size() || namelen > reclen - 8)
            break;
        const QString name = QString::fromLatin1(raw.mid(at + 8, int(namelen)));
        at += reclen;
        if (child == 0 || name.isEmpty() || name == QLatin1String(".")
            || name == QLatin1String(".."))
            continue;
        DirEntry e;
        e.name = name;
        e.ino = child;
        e.type = type;
        e.isDir = type == kDtDir;
        if (!e.isDir) {
            const Inode childIno = inodeAt(child);
            e.size = childIno.valid ? childIno.size : 0;
            e.isDir = childIno.valid && childIno.isDir;
        }
        out.append(e);
    }
    return out;
}

QByteArray Ufs2File::readFile(const Found &f, quint64 limit) const
{
    if (f.isDir || f.size == 0 || f.size > limit)
        return QByteArray();
    const Inode ino = inodeAt(f.ino);
    if (!ino.valid)
        return QByteArray();
    return readRange(ino, 0, f.size);
}

} // namespace psg::ps5
