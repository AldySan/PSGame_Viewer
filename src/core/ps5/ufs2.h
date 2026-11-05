#pragma once

#include "../types.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>
#include <QStringList>

namespace psg::ps5 {

// Read-only UFS2 image, the filesystem behind .ffpkg files (PS5 dumps):
// superblock at 64 KiB, inodes, directories and file contents. Layout
// follows the FreeBSD UFS2 on-disk format (little endian).
class Ufs2File
{
public:
    struct Found {
        quint32 ino = 0;
        quint64 size = 0;
        bool isDir = false;
    };

    struct DirEntry {
        QString name;
        quint32 ino = 0;
        quint8 type = 0;
        quint64 size = 0;
        bool isDir = false;
    };

    // Open a UFS2 image from disk.
    Ufs2File(const QString &path, bool *ok = nullptr,
             QString *error = nullptr);
    // Open over an existing (open, readable) device; the device is not
    // owned. `label` is only used in error messages.
    Ufs2File(QIODevice *device, const QString &label, bool *ok = nullptr,
             QString *error = nullptr);

    bool ok() const { return m_ok; }
    QString error() const { return m_error; }

    // Case-insensitive root-relative lookup, like ExfatImage::find().
    bool find(const QStringList &parts, Found *out) const;
    QList<DirEntry> listDir(quint32 ino) const;

    // Whole-file read with an upper bound (empty above the bound).
    QByteArray readFile(const Found &f, quint64 limit = 32000000) const;

private:
    struct Inode {
        quint16 mode = 0;
        quint64 size = 0;
        quint64 db[12] = {};
        quint64 ib[3] = {};
        bool isDir = false;
        bool valid = false;
    };

    void init(bool *ok, QString *error);
    Inode inodeAt(quint32 ino) const;
    bool readAt(quint64 at, quint64 len, QByteArray &out) const;
    bool fragForLbn(const Inode &ino, quint64 lbn, quint64 &frag) const;
    QByteArray readDirBlock(const Inode &ino, quint64 index) const;
    QByteArray readRange(const Inode &ino, quint64 from, quint64 len) const;

    QString m_label;
    QFile m_owned;
    QIODevice *m_dev = nullptr;

    // superblock geometry
    quint32 m_bsize = 0;
    quint32 m_fsize = 0;
    quint32 m_frag = 0;
    quint32 m_ipg = 0;
    quint32 m_fpg = 0;
    quint32 m_iblkno = 0;
    quint32 m_inopb = 0;
    quint64 m_imageSize = 0;

    bool m_ok = false;
    QString m_error;
};

} // namespace psg::ps5
