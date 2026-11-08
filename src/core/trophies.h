#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace psg::trophies {

struct TrpInner {
    QString name;
    quint64 offset = 0;
    quint64 size = 0;
};

struct Trophy {
    QString id;
    QString name;
    QString detail;
    QString grade; // P / G / S / B
    bool hidden = false;
    QString iconName;
};

struct CarvedIcon {
    quint64 offset = 0;
    quint64 size = 0;
    int width = 0;
    int height = 0;
};

// Inner files of a trophy00.trp / TROPHY.TRP (PS3/PS4/PS5) in the real
// retail container layout. Strict validation: empty on doubt.
QList<TrpInner> parseTrp(const QByteArray &data);

// Extract an NP communication id (e.g. "NPWR00091_00") from an entry
// path like "TROPDIR/NPWR00091_00/TROPHY.TRP" — the TROPDIR folder name
// is the id the ESFM key is derived from. Empty when the path has none.
QString npCommIdFromPath(const QString &path);

// Raw PNGs hidden in a TRP (trophy icons + title banners), carved by magic.
QList<CarvedIcon> carveTrpIcons(const QByteArray &data);

// PS5 trophy00.ucp archive records.
QList<TrpInner> parseUcp(const QByteArray &data);

// Decrypt one ESFM file to XML bytes (AES-CBC with the per-title key
// derived from the NP communication ID). Empty on failure.
QByteArray decryptEsfm(const QByteArray &blob, const QString &npCommId);

// Brute-force "NPWRxxxxx_00" for an ESFM blob when the ID is unknown.
// Returns the ID and the decrypted XML, or an empty pair. `shouldAbort`
// is polled between candidates so a long scan can be cancelled (the
// seconds-long scan must not block a GUI thread shutdown).
QPair<QString, QByteArray> bruteforceNpid(
    const QByteArray &blob, int start = 0, int end = 100000,
    const std::function<bool()> &shouldAbort = {});

// trophy conf XML (TROPCONF / decrypted SFM) -> trophy list.
QList<Trophy> parseTrophyXml(const QByteArray &xml);

// PS5 UCP bytes -> (npid, title, trophies, icon blobs).
struct UcpResult {
    QString npCommId;
    QString title;
    QList<Trophy> trophies;
    QHash<QString, QByteArray> icons;
};
UcpResult ucpTrophies(const QByteArray &data);

// UCP icon blobs in display order: keys naturally sorted ("2.png" before
// "10.png"), so the i-th icon belongs to the i-th trophy.
QList<QByteArray> ucpIconsSorted(const UcpResult &res);

} // namespace psg::trophies
