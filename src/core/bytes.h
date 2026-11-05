#pragma once

// Big/little-endian scalar readers over a QByteArray, with bounds checks.
// All parsers in psgcore go through these so malformed input can never
// read out of bounds.

#include <QtGlobal>

namespace psg::bytes {

inline bool inRange(const QByteArray &b, qsizetype off, qsizetype len)
{
    return off >= 0 && len >= 0 && off <= b.size() && len <= b.size() - off;
}

inline quint32 rdU16BE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 2)) {
        if (ok) *ok = false;
        return 0;
    }
    const uchar *p = reinterpret_cast<const uchar *>(b.constData()) + off;
    if (ok) *ok = true;
    return (quint32(p[0]) << 8) | quint32(p[1]);
}

inline quint32 rdU32BE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 4)) {
        if (ok) *ok = false;
        return 0;
    }
    const uchar *p = reinterpret_cast<const uchar *>(b.constData()) + off;
    if (ok) *ok = true;
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16)
         | (quint32(p[2]) << 8) | quint32(p[3]);
}

inline quint64 rdU64BE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 8)) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return (quint64(rdU32BE(b, off)) << 32) | rdU32BE(b, off + 4);
}

inline quint32 rdU16LE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 2)) {
        if (ok) *ok = false;
        return 0;
    }
    const uchar *p = reinterpret_cast<const uchar *>(b.constData()) + off;
    if (ok) *ok = true;
    return quint32(p[0]) | (quint32(p[1]) << 8);
}

inline quint32 rdU32LE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 4)) {
        if (ok) *ok = false;
        return 0;
    }
    const uchar *p = reinterpret_cast<const uchar *>(b.constData()) + off;
    if (ok) *ok = true;
    return quint32(p[0]) | (quint32(p[1]) << 8)
         | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

inline quint64 rdU64LE(const QByteArray &b, qsizetype off, bool *ok = nullptr)
{
    if (!inRange(b, off, 8)) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return quint64(rdU32LE(b, off)) | (quint64(rdU32LE(b, off + 4)) << 32);
}

// Bytes up to the first NUL (NUL-padded fixed-width fields).
inline QByteArray untilNul(const QByteArray &b)
{
    const int e = b.indexOf('\x00');
    return e < 0 ? b : b.left(e);
}

} // namespace psg::bytes
