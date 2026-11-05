#pragma once

#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>

namespace psg {

// Minimal read-only exFAT: boot sector, FAT walk, directory scan.
// Handles the zeroed-FAT builder quirk by falling through to the next
// contiguous cluster when the FAT entry is empty.
class ExfatImage {
public:
    struct DirItem {
        QString name;
        quint16 attrs = 0;
        quint32 firstCluster = 0;
        quint64 size = 0;
        bool noFat = false;
        bool isDir = false;
    };

    struct Found {
        quint32 firstCluster = 0;
        quint64 size = 0;
        bool noFat = false;
        bool isDir = false;
    };

    explicit ExfatImage(const QString &path, bool *ok = nullptr,
                        QString *error = nullptr);

    quint32 clusterSector(quint32 cluster) const;
    quint64 clusterOffset(quint32 cluster) const;
    bool clusterInImage(quint32 cluster) const;

    // Read a FAT-chained file (or a contiguous noFAT extent).
    QByteArray readChain(quint32 first, quint64 size) const;
    QByteArray readNoFat(quint32 first, quint64 size) const;

    // Directory listings; `want` (lower-cased) stops the scan early once
    // the name is seen, so huge images are not streamed on a miss.
    QList<DirItem> listDir(quint32 first, quint64 size, bool noFat,
                           const QString &want = QString()) const;

    // Case-insensitive path lookup, root-relative.
    bool find(const QStringList &parts, Found *out) const;
    QList<DirItem> listPath(const QStringList &parts) const;

    quint32 rootCluster() const { return m_rootCluster; }
    QString path() const { return m_path; }

private:
    QList<DirItem> parseDir(const QByteArray &raw, const QString &want) const;

    QString m_path;
    mutable QFile m_file;
    quint32 m_bytesPerSector = 0;
    quint32 m_sectorsPerCluster = 0;
    quint32 m_fatSector = 0;
    quint32 m_dataSector = 0;
    quint32 m_rootCluster = 0;
    quint64 m_imageSize = 0;
};

// Trophy packs (.trp/.ucp) anywhere in the image: known trophy dirs
// first (direct lookup), full directory walk as fallback.
struct ExfatPack {
    QString relPath;
    quint32 firstCluster = 0;
    quint64 size = 0;
    bool noFat = false;
};
QList<ExfatPack> exfatScanPacks(const ExfatImage &fs, int maxDirs = 500);

} // namespace psg
