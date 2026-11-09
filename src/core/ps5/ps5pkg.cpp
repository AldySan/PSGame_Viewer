#include "ps5pkg.h"

#include "../bytes.h"
#include "../ps4/ps4pkg.h"
#include "../util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace psg::ps5 {
namespace {

// Parse a QVariantMap out of raw param.json bytes; empty map on failure.
QVariantMap parseParamJson(const QByteArray &raw)
{
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return QVariantMap();
    return doc.object().toVariantMap();
}

} // namespace

std::pair<QString, QList<SpecRow>> paramJsonMeta(const QVariantMap &meta)
{
    const QVariantMap lp = meta.value(QStringLiteral("localizedParameters")).toMap();
    const QString lang = lp.value(QStringLiteral("defaultLanguage")).toString();
    QString title;
    if (!lang.isEmpty()) {
        const QVariantMap langMap = lp.value(lang).toMap();
        if (!langMap.isEmpty())
            title = langMap.value(QStringLiteral("titleName")).toString();
        else
            title = lp.value(QStringLiteral("en-US")).toMap()
                        .value(QStringLiteral("titleName")).toString();
    } else {
        // no default language: first entry that carries a titleName
        for (auto it = lp.cbegin(); it != lp.cend(); ++it) {
            const QString t = it.value().toMap()
                                  .value(QStringLiteral("titleName")).toString();
            if (!t.isEmpty()) {
                title = t;
                break;
            }
        }
    }

    const QString cid = meta.value(QStringLiteral("contentId")).toString();
    const QVariant cn = meta.value(QStringLiteral("applicationCategoryType"));
    QString ptype;
    if (cn.isValid() && !cn.isNull()) {
        const qlonglong n = cn.toLongLong();
        ptype = n == 0 ? QStringLiteral("Application (APP)")
                       : QStringLiteral("Type %1").arg(n);
    } else {
        ptype = QStringLiteral("-");
    }
    const QString drm = meta.value(QStringLiteral("applicationDrmType")).toString();

    QList<SpecRow> extra;
    extra.append({QStringLiteral("Title ID"),
                  meta.value(QStringLiteral("titleId")).toString()});
    extra.append({QStringLiteral("Content ID"), cid});
    extra.append({QStringLiteral("Region"), contentRegion(cid)});
    extra.append({QStringLiteral("Type"), ptype});
    extra.append({QStringLiteral("Content Ver"),
                  meta.value(QStringLiteral("contentVersion")).toString()});
    extra.append({QStringLiteral("Master Ver"),
                  meta.value(QStringLiteral("masterVersion")).toString()});
    extra.append({QStringLiteral("Concept ID"),
                  meta.value(QStringLiteral("conceptId")).toString()});
    extra.append({QStringLiteral("Min. System"),
                  fmtFw(meta.value(QStringLiteral("requiredSystemSoftwareVersion")))});
    extra.append({QStringLiteral("DRM"),
                  drm.isEmpty() ? QStringLiteral("-")
                                : drm.left(1).toUpper() + drm.mid(1)});
    extra.append({QStringLiteral("SDK"),
                  fmtFw(meta.value(QStringLiteral("sdkVersion")))});
    return {title, extra};
}

GameInfo parsePs5RetailStub(const QString &path, const QByteArray &fihHeader)
{
    GameInfo out;
    out.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("cannot open file");
        return out;
    }
    const quint64 size = f.size();
    out.size = size;

    static const QRegularExpression cidRe(
        QStringLiteral("([A-Z]{2}\\d{3,4}-[A-Z]{4}\\d{5}_[0-9A-Z\\-]+)"));
    const QString base = QFileInfo(path).fileName();
    const QRegularExpressionMatch cm = cidRe.match(base);
    const QString cid = cm.hasMatch() ? cm.captured(1) : QString();
    const QString tid = cid.isEmpty() ? QString() : titleIdFromCid(cid);

    // split retail set: base_0.pkg, base_1.pkg, ...
    QStringList parts = {path};
    static const QRegularExpression partRe(
        QStringLiteral("^(.*)_\\d+\\.pkg$"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch pm = partRe.match(base);
    if (pm.hasMatch()) {
        const QDir dir = QFileInfo(path).dir();
        const QString stem = pm.captured(1);
        static const QRegularExpression numRe(
            QStringLiteral("_(\\d+)\\.pkg$"), QRegularExpression::CaseInsensitiveOption);
        QStringList sibs;
        const QStringList candidates = dir.entryList(QDir::Files);
        for (const QString &c : candidates) {
            if (!c.startsWith(stem + u'_'))
                continue;
            const QRegularExpressionMatch nm = numRe.match(c);
            if (nm.hasMatch())
                sibs.append(c);
        }
        std::sort(sibs.begin(), sibs.end(), [&](const QString &a, const QString &b) {
            return numRe.match(a).captured(1).toInt()
                 < numRe.match(b).captured(1).toInt();
        });
        QStringList full;
        for (const QString &s : sibs)
            full.append(dir.filePath(s));
        if (full.size() > 1) {
            parts = full;
            quint64 total = 0;
            for (const QString &s : parts)
                total += quint64(QFileInfo(s).size());
            if (total > size)
                out.size = total; // joined size
        }
    }
    int idx = 0;
    for (int i = 0; i < parts.size(); ++i) {
        if (QFileInfo(parts.at(i)).fileName() == base) {
            idx = i;
            break;
        }
    }
    const bool multi = parts.size() > 1;
    const bool official = fihHeader.size() > 5 && uchar(fihHeader.at(5)) == 0x80;

    out.ok = true;
    out.platform = Platform::PS5;
    out.title = tid.isEmpty() ? base : tid;
    out.storeCid = cid;
    out.patchTid = tid;
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS5 retail (encrypted)"));
    out.addRow(QStringLiteral("Package"),
               official ? QStringLiteral("OFC (Official)")
                        : QStringLiteral("FPKG (Fake)"));
    out.addRow(QStringLiteral("Content ID"), cid.isEmpty() ? QStringLiteral("-") : cid);
    out.addRow(QStringLiteral("Title ID"), tid.isEmpty() ? QStringLiteral("-") : tid);
    out.addRow(QStringLiteral("Region"), contentRegion(cid));
    out.addRow(QStringLiteral("Part"),
               multi ? QStringLiteral("%1 of %2").arg(idx + 1).arg(parts.size())
                     : QStringLiteral("single"));
    out.addRow(QStringLiteral("Size"),
               multi ? QStringLiteral("%1 (joined %2)").arg(fmtSize(size), fmtSize(out.size))
                     : fmtSize(size));
    out.addRow(QStringLiteral("Note"),
               QStringLiteral("metadata encrypted; title/files unavailable"));
    return out;
}

GameInfo parsePs5Pkg(const QString &path, const QString &containerPath,
                     QFile &f, const QByteArray &hdr, bool isLih)
{
    GameInfo out;
    out.path = path;
    out.size = f.size();

    // FIH stores the CNT container offset at 0x58, LIH (backport wrapper)
    // at 0x30; both then parse as standard CNT.
    const quint64 emb = bytes::rdU64LE(hdr, isLih ? 0x30 : 0x58);
    const uchar signedByte = uchar(hdr.at(5));
    const quint64 pfsOff = bytes::rdU64LE(hdr, 0x10);
    const quint64 pfsSize = bytes::rdU64LE(hdr, 0x18);

    f.seek(qint64(emb));
    const QByteArray chdr = f.read(0x80);
    if (chdr.left(4) != QByteArrayLiteral("\x7f" "CNT")) {
        if (isLih) {
            out.error = QStringLiteral("LIH container is not CNT");
            return out;
        }
        return parsePs5RetailStub(path, hdr);
    }

    ps4::CntHeader cnt;
    QString cntError;
    if (!ps4::readCntHeader(f, emb, &cnt, &cntError)) {
        out.error = cntError;
        return out;
    }
    QList<ps4::CntEntry> cntEntries;
    QString namesBlob;
    QString entError;
    if (!ps4::readCntEntries(f, emb, cnt, &cntEntries, &namesBlob, &entError)) {
        out.error = entError;
        return out;
    }

    QVariantMap meta;
    QString title;
    QList<SpecRow> extra;
    const auto entryNameAt = [&](quint32 nameOff) -> QString {
        // qsizetype, not int: crafted u32 offsets >= 2^31 cast negative
        const qsizetype start = qsizetype(nameOff);
        if (start >= namesBlob.size())
            return QString();
        const qsizetype end = namesBlob.indexOf(QChar(u'\x00'), start);
        if (end < 0)
            return QString();
        return namesBlob.mid(start, end - start);
    };
    for (const ps4::CntEntry &e : cntEntries) {
        if ((e.id == 0x2000 || entryNameAt(e.nameOff) == QLatin1String("param.json"))
            && e.dataSize > 0 && e.dataSize < 100000) {
            f.seek(qint64(emb + e.dataOff));
            meta = parseParamJson(f.read(int(e.dataSize)));
            if (!meta.isEmpty())
                std::tie(title, extra) = paramJsonMeta(meta);
            break;
        }
    }

    out.ok = true;
    out.platform = Platform::PS5;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.meta = meta;
    out.iconEntry = QStringLiteral("icon0.png");
    out.patchTid = meta.value(QStringLiteral("titleId")).toString();
    out.ownVersion = meta.value(QStringLiteral("contentVersion")).toString();

    if (isLih) {
        // LIH is a scene backport wrapper (no retail issuer): label the
        // container honestly, keep the CNT rows below.
        out.addRow(QStringLiteral("Platform"), QStringLiteral("PS5 (LIH backport)"));
        out.addRow(QStringLiteral("Package"), QStringLiteral("Backport (LIH)"));
        out.addRow(QStringLiteral("Signature"), QStringLiteral("debug / fake"));
        out.addRow(QStringLiteral("Size"), fmtSize(out.size));
        out.addRow(QStringLiteral("Entries"), QString::number(cntEntries.size()));
    } else {
        out.addRow(QStringLiteral("Platform"), QStringLiteral("PS5 (finalized FIH)"));
        out.addRow(QStringLiteral("Package"),
                   signedByte == 0x80 ? QStringLiteral("OFC (Official)")
                                      : QStringLiteral("FPKG (Fake)"));
        out.addRow(QStringLiteral("Signature"),
                   signedByte == 0x80 ? QStringLiteral("official")
                                      : QStringLiteral("debug / fake"));
        out.addRow(QStringLiteral("Size"), fmtSize(out.size));
        out.addRow(QStringLiteral("PFS image"),
                   QStringLiteral("%1 @ 0x%2")
                       .arg(fmtSize(pfsSize))
                       .arg(pfsOff, 0, 16));
        out.addRow(QStringLiteral("Entries"), QString::number(cntEntries.size()));
    }
    out.rows += extra;
    if (std::none_of(out.rows.cbegin(), out.rows.cend(),
                     [](const SpecRow &r) {
                         return r.label == QLatin1String("Content ID");
                     }))
        out.rows.insert(2, {QStringLiteral("Content ID"), cnt.contentId});

    for (const ps4::CntEntry &ce : cntEntries) {
        Entry e;
        e.id = ce.id;
        e.name = entryNameAt(ce.nameOff);
        e.size = ce.dataSize;
        e.source = Entry::Source::Offset;
        e.offset = qint64(emb + ce.dataOff);
        e.containerPath = containerPath;
        out.entries.append(e);
    }
    return out;
}

} // namespace psg::ps5
