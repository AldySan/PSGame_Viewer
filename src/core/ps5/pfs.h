#pragma once

#include "../types.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>

namespace psg::ps5 {

// Read-only PFS image, the container behind .ffpfsc files: header, inode
// table, dirents, and a seekable view over the single inner file. Inner
// payloads stored with the compressed flag use the PFSC framing (64 KiB
// logical blocks, zlib per block) and are decoded on demand; raw payloads
// are served directly. Only the unsigned layout is supported; encrypted
// and signed images are rejected with a clear message.
//
// The on-disk layout follows the mkpfs image format (PS5 version 2).
class PfsFile
{
public:
    // The one regular, non-internal file of the image. `device` is a view
    // over its logical bytes and stays valid while the PfsFile lives.
    struct InnerFile {
        QString name;
        QIODevice *device = nullptr;
    };

    explicit PfsFile(const QString &path);
    ~PfsFile();

    PfsFile(const PfsFile &) = delete;
    PfsFile &operator=(const PfsFile &) = delete;

    bool ok() const { return m_ok; }
    QString error() const { return m_error; }

    // Locate the single inner file and open its payload view.
    // Fails (and sets error()) for encrypted/signed images, scattered
    // payloads, and images that hold no or several inner files.
    bool openInner(InnerFile *out);

private:
    struct Inode {
        quint16 mode = 0;
        quint32 flags = 0;
        quint64 size = 0;        // real file size
        quint64 logicalSize = 0; // padded stream length when compressed
        quint32 blocks = 0;      // stored payload, in image blocks
        qint32 db[12] = {};
        bool isDir = false;
        bool isFile = false;
        bool isCompressed = false;
        bool internal = false;
    };

    bool parseHeader();
    bool parseInodes();
    void walk(quint32 ino, const QString &rel, int depth);
    bool openPayload(const Inode &ino, QIODevice **device);

    QString m_path;
    QFile m_file;
    bool m_ok = false;
    QString m_error;

    // header
    quint32 m_blockSize = 0;
    quint32 m_mode = 0;
    int m_inodeCount = 0;
    int m_inodeBlockCount = 0;

    // walk state
    QList<Inode> m_inodes;
    struct Candidate {
        QString name;
        quint32 ino = 0;
    };
    QList<Candidate> m_files;
    bool m_walked = false;

    // payload view owned by this PfsFile
    QIODevice *m_view = nullptr;
};

} // namespace psg::ps5
