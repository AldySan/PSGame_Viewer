#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace psg {

enum class Platform {
    Unknown,
    PS2,
    PS3,
    PS4,
    PS5,
};

QString platformName(Platform p);

// One key/value line of the spec card, in display order.
struct SpecRow {
    QString label;
    QString value;
};

// A file (or pseudo-folder) inside a container. `source` selects which of
// the location fields is meaningful, mirroring the various container
// layouts the viewers understand.
struct Entry {
    enum class Source {
        None,       // no data readable (folder stubs, encrypted leftovers)
        Offset,     // plain absolute offset inside `containerPath`
        LocalFile,  // real file on disk (folder dumps)
        Cached,     // bytes held in memory
        Ps3Crypto,  // PS3 NPDRM data stream (retail AES-CTR / debug SHA1)
        ExfatChain, // exFAT cluster chain (first/nofat)
    };

    quint32 id = 0;
    QString name;
    quint64 size = 0;
    Source source = Source::None;

    qint64 offset = -1;     // Source::Offset
    QString localPath;      // Source::LocalFile
    QByteArray cached;      // Source::Cached
    bool ps3Retail = false; // Source::Ps3Crypto
    QByteArray ps3Keymat;
    quint64 ps3DataOff = 0;
    quint64 ps3EntryOff = 0;
    quint32 exfatFirst = 0; // Source::ExfatChain
    bool exfatNoFat = false;

    QString codec; // AMPR asset codec label ("LZ4", "LZ4HC", ...)
    bool isDir = false;

    QString containerPath; // file the offsets/crypto refer to (differs from
                           // the opened path for split-part relabeling)
};

// Result of parsing one game file or folder.
struct GameInfo {
    bool ok = false;
    QString error;
    Platform platform = Platform::Unknown;
    QString path;
    quint64 size = 0;
    QString title;
    QList<SpecRow> rows;
    QList<Entry> entries;
    QVariantMap meta;
    QString iconEntry;
    QString patchTid;
    QString ownVersion;
    QString ps2Serial;
    QString storeCid;
    QStringList amprLines;

    QString row(const QString &label, const QString &fallback = QString()) const;
    bool hasRow(const QString &label) const;
    void setRow(const QString &label, const QString &value);
    void addRow(const QString &label, const QString &value);

    // Convenience accessors for the rename engine and the library DB.
    QString titleId() const { return row(QStringLiteral("Title ID")); }
    QString contentId() const { return row(QStringLiteral("Content ID")); }
    QString region() const { return row(QStringLiteral("Region")); }
    QString version() const;
};

} // namespace psg
