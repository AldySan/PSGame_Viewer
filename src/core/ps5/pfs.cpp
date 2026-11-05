#include "pfs.h"

#include "../bytes.h"

#include <QHash>

#include <zlib.h>

namespace psg::ps5 {
namespace {

// PFSC framing constants (mkpfs "PFSCHdr" layout).
constexpr quint32 kPfsMagic = 20130315;
constexpr quint32 kPfscMagic = 0x43534650;
constexpr quint32 kPfscLogicalBlock = 0x10000;
constexpr quint32 kPfscHeaderSize = 0x30;
constexpr quint32 kPfscOffsetsOffset = 0x400;
constexpr quint32 kPfscInitialDataOffset = 0x10000;
constexpr quint32 kInodeSize = 0xA8;
constexpr int kInodeInternal = 0x20000;
constexpr int kInodeCompressed = 0x1;
constexpr int kMaxWalkDepth = 8;
constexpr quint64 kBlockCacheCap = 128ull << 20; // decompressed block cache

constexpr int kDirentFile = 2;
constexpr int kDirentDirectory = 3;

// A plain window over [offset, offset+length) of another device.
class SubDevice : public QIODevice
{
public:
    SubDevice(QIODevice *backing, quint64 offset, quint64 length)
        : m_backing(backing), m_offset(offset), m_length(length)
    {
        open(ReadOnly);
    }

    bool isSequential() const override { return false; }
    qint64 size() const override { return qint64(m_length); }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (!m_backing || pos() > qint64(m_length))
            return -1;
        const qint64 want = qMin<qint64>(maxSize, qint64(m_length) - pos());
        if (want <= 0)
            return 0;
        if (!m_backing->seek(qint64(m_offset) + pos()))
            return -1;
        return m_backing->read(data, want);
    }

    qint64 writeData(const char *, qint64) override { return -1; }

private:
    QIODevice *m_backing = nullptr;
    quint64 m_offset = 0;
    quint64 m_length = 0;
};

// Seekable view over a PFSC stream: 64 KiB logical blocks, each stored
// either raw (span == block size) or as a zlib stream (span < block size).
// Blocks are decompressed on first read and cached up to kBlockCacheCap.
class PfscView : public QIODevice
{
public:
    PfscView(QIODevice *backing, quint64 payloadOffset, quint64 payloadLength)
        : m_backing(backing), m_payloadOffset(payloadOffset),
          m_payloadLength(payloadLength)
    {
    }

    bool openView(QString *error)
    {
        if (m_payloadLength < kPfscInitialDataOffset) {
            if (error) *error = QStringLiteral("PFSC payload too small");
            return false;
        }
        QByteArray head;
        if (!readAt(0, kPfscHeaderSize, head)
            || head.size() < int(kPfscHeaderSize)) {
            if (error) *error = QStringLiteral("truncated PFSC header");
            return false;
        }
        const quint32 magic = bytes::rdU32LE(head, 0);
        const quint32 unk4 = bytes::rdU32LE(head, 4);
        const quint32 unk8 = bytes::rdU32LE(head, 8);
        const quint32 lbs = bytes::rdU32LE(head, 12);
        const quint64 lbs2 = bytes::rdU64LE(head, 16);
        const quint64 offTbl = bytes::rdU64LE(head, 24);
        const quint64 dataOff = bytes::rdU64LE(head, 32);
        const quint64 dataLen = bytes::rdU64LE(head, 40);
        if (magic != kPfscMagic) {
            if (error) *error = QStringLiteral("not a PFSC stream");
            return false;
        }
        if (unk4 != 0 || unk8 != 6) {
            if (error) *error = QStringLiteral("unsupported PFSC header");
            return false;
        }
        if (lbs != kPfscLogicalBlock || lbs2 != quint64(lbs)
            || offTbl != kPfscOffsetsOffset
            || dataOff < kPfscInitialDataOffset) {
            if (error) *error = QStringLiteral("unsupported PFSC layout");
            return false;
        }
        if (dataLen == 0 || dataLen % kPfscLogicalBlock != 0) {
            if (error) *error = QStringLiteral("bad PFSC logical size");
            return false;
        }
        m_dataOffset = dataOff;
        m_logicalSize = dataLen;
        m_blockCount = quint32(dataLen / kPfscLogicalBlock);
        if (m_blockCount > (1u << 20)) {
            if (error) *error = QStringLiteral("PFSC stream too large");
            return false;
        }

        const quint64 tableBytes = quint64(m_blockCount + 1) * 8;
        QByteArray table;
        if (!readAt(kPfscOffsetsOffset, tableBytes, table)
            || quint64(table.size()) != tableBytes) {
            if (error) *error = QStringLiteral("truncated PFSC block table");
            return false;
        }
        m_offsets.reserve(int(m_blockCount) + 1);
        for (quint32 i = 0; i <= m_blockCount; ++i)
            m_offsets.append(bytes::rdU64LE(table, qsizetype(i * 8)));
        bool valid = m_offsets.first() == m_dataOffset;
        for (int i = 1; valid && i < m_offsets.size(); ++i) {
            if (m_offsets.at(i) < m_offsets.at(i - 1)
                || m_offsets.at(i) > m_payloadLength)
                valid = false;
        }
        if (!valid) {
            if (error) *error = QStringLiteral("invalid PFSC block offsets");
            return false;
        }
        open(ReadOnly);
        return true;
    }

    bool isSequential() const override { return false; }
    qint64 size() const override { return qint64(m_logicalSize); }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        if (m_logicalSize == 0)
            return 0;
        const qint64 start = pos();
        qint64 done = 0;
        while (done < maxSize && start + done < qint64(m_logicalSize)) {
            const quint32 block = quint32((start + done) / kPfscLogicalBlock);
            const QByteArray &bytes = blockBytes(block);
            if (bytes.isEmpty())
                break;
            const quint32 inOff = quint32((start + done) % kPfscLogicalBlock);
            const qint64 want =
                qMin<qint64>(maxSize - done, bytes.size() - inOff);
            if (want <= 0)
                break;
            memcpy(data + done, bytes.constData() + inOff, size_t(want));
            done += want;
        }
        return done;
    }

    qint64 writeData(const char *, qint64) override { return -1; }

private:
    bool readAt(quint64 at, quint64 len, QByteArray &out) const
    {
        if (len > quint64(INT_MAX)) // int(len) below must stay positive
            return false;
        if (at > m_payloadLength || len > m_payloadLength - at)
            return false;
        if (!m_backing->seek(qint64(m_payloadOffset + at)))
            return false;
        out = m_backing->read(int(len));
        return quint64(out.size()) == len;
    }

    // Stored block -> 64 KiB logical bytes (empty on any mismatch).
    QByteArray blockBytes(quint32 block)
    {
        const auto it = m_cache.constFind(block);
        if (it != m_cache.cend())
            return it.value();

        QByteArray out;
        const quint64 begin = m_offsets.at(int(block));
        const quint64 end = m_offsets.at(int(block) + 1);
        if (end >= begin && end <= m_payloadLength) {
            const quint64 span = end - begin;
            QByteArray stored;
            if (readAt(begin, span, stored)) {
                if (span == kPfscLogicalBlock) {
                    out = stored; // raw block
                } else if (span < kPfscLogicalBlock) {
                    out.resize(int(kPfscLogicalBlock));
                    uLongf outLen = uLongf(kPfscLogicalBlock);
                    if (uncompress(reinterpret_cast<Bytef *>(out.data()),
                                   &outLen,
                                   reinterpret_cast<const Bytef *>(
                                       stored.constData()),
                                   uLong(stored.size())) == Z_OK
                        && outLen == kPfscLogicalBlock) {
                        // keep the decoded bytes
                    } else {
                        out.clear();
                    }
                }
            }
        }
        if (!out.isEmpty()) {
            m_cacheBytes += quint64(out.size());
            while (m_cacheBytes > kBlockCacheCap && !m_cache.isEmpty()) {
                const auto first = m_cache.begin();
                m_cacheBytes -= quint64(first.value().size());
                m_cache.erase(first);
            }
        }
        m_cache.insert(block, out);
        return out;
    }

    QIODevice *m_backing = nullptr;
    quint64 m_payloadOffset = 0;
    quint64 m_payloadLength = 0;
    quint64 m_dataOffset = 0;
    quint64 m_logicalSize = 0;
    quint32 m_blockCount = 0;
    QList<quint64> m_offsets;
    QHash<quint32, QByteArray> m_cache;
    quint64 m_cacheBytes = 0;
};

} // namespace

PfsFile::~PfsFile()
{
    delete m_view;
}

PfsFile::PfsFile(const QString &path)
    : m_path(path), m_file(path)
{
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("cannot open PFS image");
        return;
    }
    if (!parseHeader())
        return;
    if (!parseInodes())
        return;
    if (m_inodes.isEmpty() || !m_inodes.at(0).isDir) {
        m_error = QStringLiteral("not a PFS image (bad root inode)");
        return;
    }
    m_ok = true;
}

bool PfsFile::parseHeader()
{
    const QByteArray head = m_file.read(0x48);
    if (head.size() < 0x48) {
        m_error = QStringLiteral("truncated PFS header");
        return false;
    }
    const qint64 version = qint64(bytes::rdU64LE(head, 0));
    const quint32 magic = quint32(bytes::rdU64LE(head, 8) & 0xFFFFFFFFu);
    if (magic != kPfsMagic) {
        m_error = QStringLiteral("not a PFS image (magic mismatch)");
        return false;
    }
    if (version != 1 && version != 2) {
        m_error = QStringLiteral("unsupported PFS version %1").arg(version);
        return false;
    }
    m_mode = bytes::rdU16LE(head, 0x1C);
    if (m_mode & 0x4) {
        m_error = QStringLiteral("encrypted PFS images are not supported");
        return false;
    }
    if (m_mode & 0x1) {
        m_error = QStringLiteral("signed PFS images are not supported");
        return false;
    }
    m_blockSize = bytes::rdU32LE(head, 0x20);
    const qint64 nblock = qint64(bytes::rdU64LE(head, 0x28));
    m_inodeCount = int(bytes::rdU64LE(head, 0x30));
    const int inodeBlocks = int(bytes::rdU64LE(head, 0x40));
    if (m_blockSize < 4096 || m_blockSize > (1u << 20)
        || (m_blockSize & (m_blockSize - 1)) != 0) {
        m_error = QStringLiteral("bad PFS block size");
        return false;
    }
    if (m_inodeCount <= 0 || m_inodeCount > 4096) {
        m_error = QStringLiteral("bad PFS inode count");
        return false;
    }
    if (inodeBlocks <= 0 || inodeBlocks > 64) {
        m_error = QStringLiteral("bad PFS inode block count");
        return false;
    }
    if (nblock < 1 || quint64(nblock) * m_blockSize > quint64(m_file.size())) {
        m_error = QStringLiteral("truncated PFS image");
        return false;
    }
    m_inodeBlockCount = inodeBlocks;
    return true;
}

bool PfsFile::parseInodes()
{
    const int inodesPerBlock = int(m_blockSize / kInodeSize);
    if (inodesPerBlock <= 0) {
        m_error = QStringLiteral("bad PFS inode geometry");
        return false;
    }
    m_inodes.reserve(m_inodeCount);
    for (int b = 0; b < m_inodeBlockCount && m_inodes.size() < m_inodeCount;
         ++b) {
        const quint64 off = quint64(m_blockSize) * (1 + quint64(b));
        if (!m_file.seek(qint64(off)))
            return false;
        const QByteArray block = m_file.read(qint64(m_blockSize));
        if (block.size() < int(m_blockSize)) {
            m_error = QStringLiteral("truncated PFS inode table");
            return false;
        }
        for (int i = 0; i < inodesPerBlock && m_inodes.size() < m_inodeCount;
             ++i) {
            const qsizetype at = qsizetype(i * kInodeSize);
            Inode ino;
            const quint32 flags = bytes::rdU32LE(block, at + 4);
            ino.mode = bytes::rdU16LE(block, at + 0);
            ino.size = bytes::rdU64LE(block, at + 8);
            ino.logicalSize = bytes::rdU64LE(block, at + 0x10);
            ino.blocks = bytes::rdU32LE(block, at + 0x60);
            for (int d = 0; d < 12; ++d)
                ino.db[d] = qint32(bytes::rdU32LE(block, at + 0x64 + 4 * d));
            ino.isFile = (ino.mode & 0x8000) != 0;
            ino.isDir = (ino.mode & 0x4000) != 0;
            ino.isCompressed = (flags & kInodeCompressed) != 0;
            ino.internal = (flags & kInodeInternal) != 0;
            m_inodes.append(ino);
        }
    }
    if (m_inodes.size() < m_inodeCount) {
        m_error = QStringLiteral("truncated PFS inode table");
        return false;
    }
    return true;
}

void PfsFile::walk(quint32 ino, const QString &rel, int depth)
{
    // note: inode 0 is the root directory in this layout
    if (depth > kMaxWalkDepth || ino >= quint32(m_inodes.size()))
        return;
    const Inode &node = m_inodes.at(int(ino));
    if (!node.isDir || node.blocks <= 0 || node.blocks > 64 || node.db[0] <= 0)
        return;
    if (quint64(node.db[0]) * m_blockSize >= quint64(m_file.size()))
        return;

    const int n = qMin<int>(int(node.blocks), 12);
    for (int b = 0; b < n; ++b) {
        if (node.db[b] <= 0)
            break;
        const quint64 off = quint64(node.db[b]) * m_blockSize;
        if (!m_file.seek(qint64(off)))
            return;
        const QByteArray block = m_file.read(qint64(m_blockSize));
        if (block.size() < int(m_blockSize))
            return;
        qsizetype at = 0;
        while (at + 16 <= block.size()) {
            const quint32 child = bytes::rdU32LE(block, at + 0);
            const qint32 type = qint32(bytes::rdU32LE(block, at + 4));
            const qint32 nameLen = qint32(bytes::rdU32LE(block, at + 8));
            const qint32 entSize = qint32(bytes::rdU32LE(block, at + 12));
            if (entSize < 16 || at + entSize > block.size()
                || nameLen < 0 || nameLen > entSize - 16)
                break;
            const QString name =
                QString::fromUtf8(block.mid(at + 16, nameLen));
            at += entSize;
            if (child == 0 || name.isEmpty() || name == QLatin1String(".")
                || name == QLatin1String(".."))
                continue;
            if (type == kDirentFile) {
                if (child < quint32(m_inodes.size())
                    && m_inodes.at(int(child)).isFile
                    && !m_inodes.at(int(child)).internal) {
                    m_files.append({rel.isEmpty() ? name : rel + u'/' + name,
                                    child});
                }
            } else if (type == kDirentDirectory) {
                walk(child, rel.isEmpty() ? name : rel + u'/' + name,
                     depth + 1);
            }
        }
    }
}

bool PfsFile::openPayload(const Inode &ino, QIODevice **device)
{
    if (ino.blocks <= 0 || ino.blocks > 4096 || ino.db[0] <= 0)
        return false;
    const quint64 storedOff = quint64(ino.db[0]) * m_blockSize;
    const quint64 storedLen = quint64(ino.blocks) * m_blockSize;
    if (storedOff >= quint64(m_file.size())
        || storedLen > quint64(m_file.size()) - storedOff)
        return false;
    if (!ino.isCompressed) {
        if (ino.size == 0 || ino.size > storedLen)
            return false;
        *device = new SubDevice(&m_file, storedOff, ino.size);
        return true;
    }
    // compressed payload: the PFSC stream serves the padded logical stream
    if (ino.size == 0 || ino.size > ino.logicalSize)
        return false;
    auto *view = new PfscView(&m_file, storedOff, storedLen);
    QString err;
    if (!view->openView(&err) || view->size() < qint64(ino.size)) {
        delete view;
        return false;
    }
    *device = view;
    return true;
}

bool PfsFile::openInner(InnerFile *out)
{
    if (!m_ok) {
        if (m_error.isEmpty())
            m_error = QStringLiteral("PFS image not parsed");
        return false;
    }
    if (!m_walked) {
        walk(0, QString(), 0);
        m_walked = true;
    }
    if (m_files.isEmpty()) {
        m_error = QStringLiteral("PFS image contains no inner file");
        return false;
    }
    if (m_files.size() > 1) {
        m_error = QStringLiteral("PFS image contains %1 inner files")
                      .arg(m_files.size());
        return false;
    }
    const Candidate &cand = m_files.first();
    const Inode &ino = m_inodes.at(int(cand.ino));
    QIODevice *device = nullptr;
    if (!openPayload(ino, &device)) {
        m_error = QStringLiteral("unsupported PFS payload layout");
        return false;
    }
    delete m_view;
    m_view = device;
    // uroot is the filesystem root: its children are named root-relative
    out->name = cand.name.startsWith(QStringLiteral("uroot/"),
                                     Qt::CaseInsensitive)
        ? cand.name.mid(6)
        : cand.name;
    out->device = m_view;
    return true;
}

} // namespace psg::ps5
