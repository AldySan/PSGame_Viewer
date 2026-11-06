#include "sfo.h"

#include "bytes.h"

namespace psg {

QVariantMap parseSfo(const QByteArray &data)
{
    QVariantMap out;
    if (data.size() < 4 || data.left(4) != QByteArrayLiteral("\x00PSF")) {
        out.insert(QStringLiteral("_error"),
                   QStringLiteral("bad magic %1")
                       .arg(QString::fromLatin1(data.left(4).toHex())));
        return out;
    }
    // header: magic, version(u32), key-table offset, data offset, entry count
    const quint32 keyOff = bytes::rdU32LE(data, 8);
    const quint32 dataOff = bytes::rdU32LE(data, 12);
    const quint32 count = bytes::rdU32LE(data, 16);
    if (count > 100000) { // sanity cap, real SFOs stay far below this
        out.insert(QStringLiteral("_error"), QStringLiteral("entry count out of range"));
        return out;
    }
    for (quint32 i = 0; i < count; ++i) {
        const qsizetype eo = 20 + qsizetype(i) * 16;
        if (!bytes::inRange(data, eo, 16))
            break;
        const quint32 keyRel = bytes::rdU16LE(data, eo);
        const quint32 fmt = bytes::rdU16LE(data, eo + 2);
        const quint32 dlen = bytes::rdU32LE(data, eo + 4);
        const quint32 dmax = bytes::rdU32LE(data, eo + 8);
        const quint32 doff = bytes::rdU32LE(data, eo + 12);

        const qsizetype ks = qsizetype(keyOff) + keyRel;
        const int ke = data.indexOf('\x00', ks);
        const QString key = QString::fromUtf8(
            data.mid(ks, ke < 0 ? -1 : ke - ks));

        const qsizetype valOff = qsizetype(dataOff) + doff;
        if (!bytes::inRange(data, valOff, qsizetype(dmax > 0 ? dmax : 1)))
            continue;

        if (fmt == 0x0204) {
            int ve = data.indexOf('\x00', valOff);
            if (ve < 0 || ve > valOff + int(dmax))
                ve = valOff + int(dlen);
            out.insert(key, QString::fromUtf8(data.mid(valOff, ve - valOff)));
        } else if (fmt == 0x0404) {
            out.insert(key, bytes::rdU32LE(data, valOff));
        } else {
            out.insert(key, QString::fromLatin1(
                       data.mid(valOff, int(dlen)).toHex(' ')));
        }
    }
    return out;
}

} // namespace psg
