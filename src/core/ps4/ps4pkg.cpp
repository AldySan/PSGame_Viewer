#include "ps4pkg.h"

#include "../bytes.h"
#include "../sfo.h"
#include "../ps5/ps5pkg.h"
#include "../util.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <utility>

namespace psg::ps4 {
namespace {

QString entryName(const QString &namesBlob, quint32 off)
{
    const int end = namesBlob.indexOf(QChar(u'\x00'), int(off));
    if (end < 0)
        return QString();
    return namesBlob.mid(int(off), end - int(off));
}

} // namespace

QString cntPackageType(quint32 flags)
{
    if (flags & 0x80000000u)
        return QStringLiteral("OFC (Official)");
    return QStringLiteral("FPKG (Fake)");
}

bool readCntHeader(QFile &f, quint64 base, CntHeader *hdr, QString *error)
{
    if (!f.isOpen() && !f.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("cannot open file");
        return false;
    }
    f.seek(qint64(base));
    const QByteArray raw = f.read(0x80);
    if (raw.size() < 0x80 || raw.left(4) != QByteArrayLiteral("\x7f" "CNT")) {
        if (error) *error = QStringLiteral("bad CNT header");
        return false;
    }
    hdr->flags = bytes::rdU32BE(raw, 0x04);
    hdr->entryCount = bytes::rdU32BE(raw, 0x10);
    hdr->sysEntryCount = quint16(bytes::rdU16BE(raw, 0x14));
    hdr->entryTableOff = bytes::rdU32BE(raw, 0x18);
    hdr->bodyOff = bytes::rdU64BE(raw, 0x20);
    {
        const QByteArray cidRaw = raw.mid(0x40, 48);
        hdr->contentId = QString::fromLatin1(
            cidRaw.left(int(cidRaw.indexOf('\x00'))));
    }
    if (hdr->entryCount > 1000000) {
        if (error) *error = QStringLiteral("entry count out of range");
        return false;
    }
    return true;
}

bool readCntEntries(QFile &f, quint64 base, const CntHeader &hdr,
                    QList<CntEntry> *entries, QString *namesBlob,
                    QString *error)
{
    entries->clear();
    if (!hdr.entryCount || !hdr.entryTableOff) {
        if (error) *error = QStringLiteral("no entry table");
        return false;
    }
    f.seek(qint64(base + hdr.entryTableOff));
    const QByteArray table = f.read(int(hdr.entryCount) * 32);
    if (table.size() < int(hdr.entryCount) * 32) {
        if (error) *error = QStringLiteral("truncated entry table");
        return false;
    }
    for (quint32 i = 0; i < hdr.entryCount; ++i) {
        const qsizetype o = qsizetype(i) * 32;
        CntEntry e;
        e.id = bytes::rdU32BE(table, o);
        e.nameOff = bytes::rdU32BE(table, o + 4);
        e.flags1 = bytes::rdU32BE(table, o + 8);
        e.flags2 = bytes::rdU32BE(table, o + 12);
        e.dataOff = bytes::rdU32BE(table, o + 16);
        e.dataSize = bytes::rdU32BE(table, o + 20);
        entries->append(e);
    }
    // name table lives in the 0x200 container entry
    for (const CntEntry &e : *entries) {
        if (e.id == 0x200 && e.dataSize > 0 && e.dataSize < 10000000) {
            f.seek(qint64(base + e.dataOff));
            *namesBlob = QString::fromLatin1(f.read(int(e.dataSize)));
            return true;
        }
    }
    *namesBlob = QString();
    return true;
}

GameInfo parsePs4Pkg(const QString &path, const QString &containerPath,
                     quint64 magicBase)
{
    GameInfo out;
    out.path = path;

    QFile f(containerPath);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("cannot open file");
        return out;
    }
    out.size = f.size();

    CntHeader hdr;
    QString hdrError;
    if (!readCntHeader(f, magicBase, &hdr, &hdrError)) {
        out.error = hdrError;
        return out;
    }
    QList<CntEntry> cnt;
    QString namesBlob;
    QString entError;
    if (!readCntEntries(f, magicBase, hdr, &cnt, &namesBlob, &entError)) {
        out.error = entError;
        return out;
    }

    const auto findEntry = [&](quint32 id, const char *name) -> const CntEntry * {
        for (const CntEntry &e : cnt) {
            if (e.id == id)
                return &e;
            if (name && e.nameOff != 0xffffffff
                && entryName(namesBlob, e.nameOff) == QLatin1String(name))
                return &e;
        }
        return nullptr;
    };

    // param.sfo (id 0x1000) first, then param.json (id 0x2000) as fallback
    QVariantMap sfo, meta;
    const CntEntry *sfoEnt = findEntry(0x1000, "param.sfo");
    if (sfoEnt && sfoEnt->dataSize > 0 && sfoEnt->dataSize < 1000000) {
        f.seek(qint64(magicBase + sfoEnt->dataOff));
        const QByteArray raw = f.read(int(sfoEnt->dataSize));
        sfo = parseSfo(raw);
    }
    const CntEntry *pjEnt = findEntry(0x2000, "param.json");
    if (sfo.contains(QStringLiteral("_error")) || sfo.isEmpty()) {
        sfo.clear();
        if (pjEnt && pjEnt->dataSize > 0 && pjEnt->dataSize < 100000) {
            f.seek(qint64(magicBase + pjEnt->dataOff));
            const QByteArray raw = f.read(int(pjEnt->dataSize));
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (doc.isObject())
                meta = doc.object().toVariantMap();
        }
    }

    QString title;
    QList<SpecRow> extra;
    QString ver4;
    QString patchTid;
    if (!sfo.isEmpty()) {
        title = sfo.value(QStringLiteral("TITLE")).toString();
        QString cid4 = sfo.value(QStringLiteral("CONTENT_ID")).toString();
        if (cid4.isEmpty())
            cid4 = hdr.contentId;
        const QString cat4 = sfo.value(QStringLiteral("CATEGORY")).toString();
        // update PKGs (gp): VERSION names the base it applies to,
        // APP_VER is the actual patch version
        ver4 = sfo.value(QStringLiteral("VERSION")).toString();
        if (cat4.toLower() == QLatin1String("gp")
            && sfo.contains(QStringLiteral("APP_VER")))
            ver4 = sfo.value(QStringLiteral("APP_VER")).toString();

        int langs = 0;
        for (auto it = sfo.cbegin(); it != sfo.cend(); ++it) {
            if ((it.key() == QLatin1String("TITLE")
                 || it.key().startsWith(QLatin1String("TITLE_")))
                && !it.value().toString().trimmed().isEmpty())
                ++langs;
        }
        QString built = QStringLiteral("-");
        static const QRegularExpression re(
            QStringLiteral("c_date=(\\d{4})(\\d{2})(\\d{2})"));
        const QRegularExpressionMatch m = re.match(
            sfo.value(QStringLiteral("PUBTOOLINFO")).toString());
        if (m.hasMatch())
            built = m.captured(1) + u'-' + m.captured(2) + u'-' + m.captured(3);

        QString tidRow = sfo.value(QStringLiteral("TITLE_ID")).toString();
        if (tidRow.isEmpty())
            tidRow = titleIdFromCid(cid4);
        extra.append({QStringLiteral("Title ID"), tidRow});
        extra.append({QStringLiteral("Content ID"), cid4});
        extra.append({QStringLiteral("Region"), contentRegion(cid4)});
        extra.append({QStringLiteral("Type"), ps4PkgType(cat4)});
        extra.append({QStringLiteral("Version"), ver4});
        if (cat4.toLower() == QLatin1String("gp")
            && sfo.contains(QStringLiteral("VERSION")))
            extra.append({QStringLiteral("Base Version"),
                          sfo.value(QStringLiteral("VERSION")).toString()});
        extra.append({QStringLiteral("Min. System"),
                      fmtFw32(sfo.value(QStringLiteral("SYSTEM_VER")))});
        const QString sdk = ps4SdkVersion(sfo);
        if (!sdk.isEmpty())
            extra.append({QStringLiteral("SDK"), sdk});
        extra.append({QStringLiteral("Languages"),
                      langs > 0 ? QString::number(langs) : QStringLiteral("-")});
        extra.append({QStringLiteral("Built"), built});
        if (!(hdr.flags & 0x80000000u))
            extra.append({QStringLiteral("Passcode"),
                          QStringLiteral("zeros (FPKG default)")});
        patchTid = sfo.value(QStringLiteral("TITLE_ID")).toString();
        out.meta = sfo;
    } else if (!meta.isEmpty()) {
        std::tie(title, extra) = ps5::paramJsonMeta(meta);
        patchTid = meta.value(QStringLiteral("titleId")).toString();
        out.meta = meta;
    }

    out.ok = true;
    out.platform = Platform::PS4;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.iconEntry = QStringLiteral("icon0.png");
    const QString kind = !sfo.isEmpty() ? QStringLiteral("PS4 (CNT metadata)")
                                        : QStringLiteral("CNT metadata");
    out.addRow(QStringLiteral("Platform"), kind);
    out.addRow(QStringLiteral("Package"), cntPackageType(hdr.flags));
    out.addRow(QStringLiteral("Size"), fmtSize(out.size));
    out.addRow(QStringLiteral("Entries"),
               QStringLiteral("%1 (%2 sys)").arg(hdr.entryCount).arg(hdr.sysEntryCount));
    out.rows += extra;
    if (!std::any_of(out.rows.cbegin(), out.rows.cend(),
                     [](const SpecRow &r) { return r.label == QLatin1String("Content ID"); }))
        out.rows.insert(2, {QStringLiteral("Content ID"),
                            sfo.isEmpty() ? hdr.contentId
                                          : sfo.value(QStringLiteral("CONTENT_ID")).toString()});

    // Title ID row must never silently disappear for partial downloads
    const auto hasTid = std::any_of(out.rows.cbegin(), out.rows.cend(),
                                    [](const SpecRow &r) {
                                        return r.label == QLatin1String("Title ID")
                                               && !r.value.trimmed().isEmpty()
                                               && r.value != QLatin1String("-");
                                    });
    if (!hasTid) {
        QString ftid = titleIdFromCid(hdr.contentId);
        if (ftid.isEmpty())
            ftid = titleIdFromFileName(path);
        if (!ftid.isEmpty())
            out.rows.insert(2, {QStringLiteral("Title ID"), ftid});
    }

    out.patchTid = patchTid.isEmpty() ? titleIdFromCid(hdr.contentId)
                                      : patchTid;
    if (out.patchTid.isEmpty())
        out.patchTid = titleIdFromFileName(path);
    out.ownVersion = sfo.isEmpty() ? QString() : ver4;

    for (const CntEntry &ce : cnt) {
        Entry e;
        e.id = ce.id;
        e.name = entryName(namesBlob, ce.nameOff);
        e.size = ce.dataSize;
        e.source = Entry::Source::Offset;
        e.offset = qint64(magicBase + ce.dataOff);
        e.containerPath = containerPath;
        out.entries.append(e);
    }
    return out;
}

} // namespace psg::ps4
