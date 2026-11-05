#include "trophies.h"

#include "aes.h"
#include "bytes.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>

namespace psg::trophies {
namespace {

// Public trophy key (psdevwiki); the per-title key is derived by AES-CBC
// encrypting the NP communication ID with it.
const uchar kTrophyKey[16] = {0x21, 0xF4, 0x1A, 0x6B, 0xAD, 0x8A, 0x1D, 0x3E,
                              0xCA, 0x7A, 0xD5, 0x86, 0xC1, 0x01, 0xB7, 0xA9};

QByteArray keyForNpid(const QByteArray &npid16)
{
    const QByteArray key(reinterpret_cast<const char *>(kTrophyKey), 16);
    const QByteArray iv(16, 0x00);
    return aes::cbcEncrypt(key, iv, npid16);
}

} // namespace

QList<TrpInner> parseTrp(const QByteArray &data)
{
    QList<TrpInner> out;
    if (data.size() < 0x38 || data.left(4) != QByteArrayLiteral("\xDC\xA2\x4D\x00"))
        return out;
    const quint32 count = bytes::rdU32BE(data, 4);
    const quint64 total = bytes::rdU64BE(data, 8);
    const quint32 esize = bytes::rdU32BE(data, 0x14);
    if (count < 1 || count > 10000 || total != quint64(data.size()))
        return out;
    if (esize != 0x40)
        return out;
    const qsizetype hsize = 0x38; // entries start after the 32-byte digest
    for (quint32 i = 0; i < count; ++i) {
        const qsizetype eo = hsize + qsizetype(i) * esize;
        if (eo + esize > data.size())
            return {};
        TrpInner t;
        t.offset = bytes::rdU64BE(data, eo + 8);
        t.size = bytes::rdU64BE(data, eo + 16);
        const QByteArray nm = bytes::untilNul(data.mid(eo + 40, 24));
        t.name = QString::fromLatin1(nm);
        if (t.name.isEmpty() || t.offset + t.size > quint64(data.size()))
            return {};
        out.append(t);
    }
    return out;
}

QList<CarvedIcon> carveTrpIcons(const QByteArray &data)
{
    QList<CarvedIcon> out;
    if (data.size() < 100)
        return out;
    qsizetype at = 0;
    const QByteArray pngSig = QByteArrayLiteral("\x89PNG\r\n\x1a\n");
    while (out.size() <= 2000) {
        const qsizetype s = data.indexOf(pngSig, at);
        if (s < 0)
            break;
        at = s + 1;
        if (s + 33 > data.size())
            continue;
        if (data.mid(s + 12, 4) != QByteArrayLiteral("IHDR"))
            continue;
        const quint32 w = bytes::rdU32BE(data, s + 16);
        const quint32 h = bytes::rdU32BE(data, s + 20);
        if (w == 0 || h == 0 || w > 2048 || h > 2048)
            continue;
        const qsizetype en = data.indexOf("IEND", s) + 8;
        if (en <= 8 || en - s > 10000000 || en > data.size())
            continue;
        CarvedIcon c;
        c.offset = quint64(s);
        c.size = quint64(en - s);
        c.width = int(w);
        c.height = int(h);
        out.append(c);
    }
    return out;
}

QList<TrpInner> parseUcp(const QByteArray &data)
{
    QList<TrpInner> out;
    if (data.size() < 0x80 || data.left(4) != QByteArrayLiteral("\xB2\x28\xC6\x0A"))
        return out;
    qsizetype base = 0x40;
    while (base + 64 <= data.size() && out.size() <= 10000) {
        const quint32 off = bytes::rdU32BE(data, base + 4);
        const quint32 size = bytes::rdU32BE(data, base + 12);
        const QByteArray nm = bytes::untilNul(data.mid(base + 32, 32));
        if (nm.isEmpty())
            break; // padding / end of table
        const QString name = QString::fromLatin1(nm);
        if (name.isEmpty())
            break;
        if (size == 0 && off == 0) {
            // tombstone/empty slot (e.g. zero-size icon0): keep name only
            base += 64;
            continue;
        }
        if (quint64(off) + size > quint64(data.size()) || size == 0)
            break;
        TrpInner t;
        t.name = name;
        t.offset = off;
        t.size = size;
        out.append(t);
        base += 64;
    }
    return out;
}

QByteArray decryptEsfm(const QByteArray &blob, const QString &npCommId)
{
    if (blob.size() < 32 || blob.size() % 16 != 0)
        return QByteArray();
    QByteArray npid = npCommId.toUtf8().left(16);
    npid.resize(16, '\x00');
    const QByteArray ckey = keyForNpid(npid);
    const QByteArray iv = blob.left(16);
    const QByteArray pt = aes::cbcDecrypt(ckey, iv, blob.mid(16));
    if (pt.isEmpty())
        return QByteArray();
    // PKCS#7 padding check
    const uchar pad = uchar(pt.at(pt.size() - 1));
    if (pad < 1 || pad > 16)
        return QByteArray();
    for (int i = pt.size() - int(pad); i < pt.size(); ++i) {
        if (uchar(pt.at(i)) != pad)
            return QByteArray();
    }
    const QByteArray body = pt.left(pt.size() - int(pad));
    if (!body.left(200).toLower().contains("trophy"))
        return QByteArray();
    int printable = 0;
    for (char c : body) {
        const uchar u = uchar(c);
        if ((u >= 32 && u < 127) || u == 9 || u == 10 || u == 13)
            ++printable;
    }
    if (qint64(printable) * 10 < qint64(body.size()) * 7)
        return QByteArray();
    return body;
}

QPair<QString, QByteArray> bruteforceNpid(const QByteArray &blob,
                                          int start, int end)
{
    if (blob.size() < 32)
        return {};
    const QByteArray ct = blob.mid(16, 16);
    const QByteArray iv = blob.left(16);
    for (int i = start; i < end; ++i) {
        QByteArray npid = QStringLiteral("NPWR%1_00").arg(i, 5, 10, QChar(u'0'))
                               .toUtf8();
        npid.resize(16, '\x00');
        const QByteArray ckey = keyForNpid(npid);
        const QByteArray first = aes::cbcDecrypt(ckey, iv, ct);
        if (first.isEmpty() || first.at(0) != '<')
            continue;
        const QString id = QString::fromLatin1(npid).section(u'\x00', 0, 0);
        const QByteArray xml = decryptEsfm(blob, id);
        if (!xml.isEmpty())
            return {id, xml};
    }
    return {};
}

QList<Trophy> parseTrophyXml(const QByteArray &xml)
{
    QList<Trophy> out;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        if (r.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (r.name() != QLatin1String("trophy"))
            continue;
        Trophy t;
        t.id = r.attributes().value(QStringLiteral("id")).toString();
        const QString ttype = r.attributes().value(QStringLiteral("ttype")).toString();
        t.grade = ttype.toUpper();
        t.hidden = r.attributes().value(QStringLiteral("hidden"))
                       .toString().toLower()
                   == QLatin1String("yes");
        while (r.readNextStartElement()) {
            if (r.name() == QLatin1String("name"))
                t.name = r.readElementText().trimmed();
            else if (r.name() == QLatin1String("detail"))
                t.detail = r.readElementText().trimmed();
            else
                r.skipCurrentElement();
        }
        out.append(t);
    }
    return out;
}

UcpResult ucpTrophies(const QByteArray &data)
{
    UcpResult res;
    const QList<TrpInner> files = parseUcp(data);
    if (files.isEmpty())
        return res;

    QHash<QString, QByteArray> blobs;
    for (const TrpInner &f : files)
        blobs.insert(f.name, data.mid(int(f.offset), int(f.size)));

    QStringList metas;
    for (auto it = blobs.cbegin(); it != blobs.cend(); ++it) {
        if (it.key().startsWith(QLatin1String("tropmeta_"))
            && it.key().endsWith(QLatin1String(".json")))
            metas.append(it.key());
    }
    metas.sort();

    QString pick;
    if (blobs.contains(QStringLiteral("tropmeta_en-US.json")))
        pick = QStringLiteral("tropmeta_en-US.json");
    else if (blobs.contains(QStringLiteral("tropmeta_en-GB.json")))
        pick = QStringLiteral("tropmeta_en-GB.json");
    else if (!metas.isEmpty())
        pick = metas.first();

    if (!pick.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(blobs.value(pick));
        if (doc.isObject()) {
            const QVariantMap o = doc.object().toVariantMap();
            res.npCommId = o.value(QStringLiteral("trophyNpCommId")).toString();
            const QVariantMap md = o.value(QStringLiteral("metadata")).toMap();
            res.title = md.value(QStringLiteral("titleMetadata"))
                            .toMap().value(QStringLiteral("name")).toString();
            const QVariantList trs =
                md.value(QStringLiteral("trophyMetadata")).toList();
            for (const QVariant &v : trs) {
                const QVariantMap t = v.toMap();
                Trophy tro;
                tro.id = t.value(QStringLiteral("id")).toString();
                tro.name = t.value(QStringLiteral("name")).toString();
                tro.detail = t.value(QStringLiteral("detail")).toString();
                res.trophies.append(tro);
            }
        }
    }
    for (auto it = blobs.cbegin(); it != blobs.cend(); ++it) {
        if (it.key().endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
            && it.value().left(4) == QByteArrayLiteral("\x89PNG"))
            res.icons.insert(it.key(), it.value());
    }
    return res;
}

} // namespace psg::trophies
