#include "detect.h"

#include "bytes.h"
#include "ps2/ps2disc.h"
#include "ps3/ps3folder.h"
#include "ps3/ps3iso.h"
#include "ps3/ps3pkg.h"
#include "ps4/ps4pkg.h"
#include "ps5/exfat.h"
#include "ps5/ps5image.h"
#include "ps5/ps5pkg.h"
#include "util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace psg::detect {
namespace {

GameInfo parseDiscIso(const QString &path)
{
    GameInfo r = ps3::parsePs3Iso(path);
    if (r.ok)
        return r;
    GameInfo r2 = ps2::parsePs2Iso(path);
    if (r2.ok)
        return r2;
    if (r.error.contains(QLatin1String("not a PS"))
        && r2.error.contains(QLatin1String("not a PS")))
        r.error = QStringLiteral("not a PS2/PS3 disc ISO");
    return r2.error == QLatin1String("not a PS2 disc ISO") ? r : r2;
}

// Part N>0 of a split set: reuse the sibling _0 header to confirm, then
// relabel (PS4) or stub (PS5).
GameInfo splitPartStub(const QString &path)
{
    static const QRegularExpression re(
        QStringLiteral("^(.*)_(\\d+)\\.pkg$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m =
        re.match(QFileInfo(path).fileName());
    if (!m.hasMatch())
        return GameInfo();
    const QString p0 = QFileInfo(path).dir().filePath(m.captured(1) + QStringLiteral("_0.pkg"));
    QFile f0(p0);
    if (!f0.open(QIODevice::ReadOnly))
        return GameInfo();
    const QByteArray mg = f0.read(4);
    if (mg == QByteArrayLiteral("\x7f" "CNT")) {
        // PS4 split part: metadata lives in _0 — parse it, relabel
        GameInfo r = ps4::parsePs4Pkg(p0, p0, 0);
        if (!r.ok)
            return r;
        const QStringList sibs = splitSetSiblings(path);
        int idx = 0;
        for (int i = 0; i < sibs.size(); ++i) {
            if (QFileInfo(sibs.at(i)).fileName() == QFileInfo(path).fileName()) {
                idx = i;
                break;
            }
        }
        quint64 total = 0;
        for (const QString &s : sibs)
            total += quint64(QFileInfo(s).size());
        const quint64 ownSize = quint64(QFileInfo(path).size());
        for (SpecRow &row : r.rows) {
            if (row.label == QLatin1String("Size")) {
                row.value = sibs.size() > 1
                    ? QStringLiteral("%1 (joined %2)")
                          .arg(fmtSize(ownSize), fmtSize(total))
                    : fmtSize(ownSize);
            }
        }
        r.size = ownSize;
        r.addRow(QStringLiteral("Part"),
                 sibs.size() > 1
                     ? QStringLiteral("%1 of %2").arg(idx + 1).arg(sibs.size())
                     : QStringLiteral("single"));
        // entries refer to the part-0 container
        for (Entry &e : r.entries)
            e.containerPath = p0;
        r.path = path;
        return r;
    }
    if (mg != QByteArrayLiteral("\x7f" "FIH"))
        return GameInfo();
    const QByteArray hdr = mg + f0.read(252);
    return ps5::parsePs5RetailStub(path, hdr);
}

GameInfo parsePkgMagic(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        GameInfo bad;
        bad.path = path;
        bad.error = QStringLiteral("cannot open file");
        return bad;
    }
    const quint64 size = quint64(f.size());
    const QByteArray magic = f.read(4);
    if (magic == QByteArrayLiteral("\x01" "CD0"))
        return parseDiscIso(path);
    if (magic == QByteArrayLiteral("\x7f" "PKG"))
        return ps3::parsePs3Pkg(path);
    if (magic == QByteArrayLiteral("\x7f" "FIH")
        || magic == QByteArrayLiteral("\x7f" "LIH")) {
        f.seek(0);
        const QByteArray hdr = f.read(256);
        if (hdr.size() < 256) {
            GameInfo bad;
            bad.path = path;
            bad.error = QStringLiteral("truncated FIH/LIH header");
            return bad;
        }
        return ps5::parsePs5Pkg(path, path, f, hdr,
                                magic == QByteArrayLiteral("\x7f" "LIH"));
    }
    if (magic == QByteArrayLiteral("\x7f" "CNT"))
        return ps4::parsePs4Pkg(path, path, 0);
    // PS5 exFAT image check
    f.seek(0);
    const QByteArray probe = f.read(11);
    if (probe.size() >= 11
        && probe.mid(3, 8) == QByteArrayLiteral("EXFAT   "))
        return ps5::parseExfatImage(path);
    // split retail part without header? resolve via sibling _0.
    GameInfo stub = splitPartStub(path);
    if (stub.ok || !stub.error.isEmpty())
        return stub;
    GameInfo bad;
    bad.path = path;
    bad.error = QStringLiteral("unknown magic %1")
                    .arg(QString::fromLatin1(magic.toHex()));
    return bad;
}

} // namespace

QStringList splitSetSiblings(const QString &path)
{
    const QString base = QFileInfo(path).fileName();
    if (!base.endsWith(QLatin1String(".pkg"), Qt::CaseInsensitive))
        return {path};
    static const QRegularExpression re(
        QStringLiteral("^(.*)_(\\d+)\\.pkg$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(base);
    if (!m.hasMatch())
        return {path};
    static const QRegularExpression numRe(
        QStringLiteral("_(\\d+)\\.pkg$"),
        QRegularExpression::CaseInsensitiveOption);
    const QDir dir = QFileInfo(path).dir();
    QStringList sibs;
    const QStringList candidates = dir.entryList(QDir::Files);
    for (const QString &c : candidates) {
        if (!c.startsWith(m.captured(1) + u'_'))
            continue;
        if (numRe.match(c).hasMatch())
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
        const QString canon = QFileInfo(path).absoluteFilePath();
        for (const QString &p : full) {
            if (QFileInfo(p).absoluteFilePath().compare(canon,
                                                        Qt::CaseInsensitive)
                == 0)
                return full;
        }
    }
    return {path};
}

GameInfo openAny(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir()) {
        if (!ps3::ps3FolderBase(path).isEmpty())
            return ps3::parsePs3Folder(path);
        return ps5::parseAppFolder(path);
    }
    if (!fi.isFile()) {
        GameInfo bad;
        bad.path = path;
        bad.error = QStringLiteral("file not found");
        return bad;
    }
    const QString low = path.toLower();
    if (low.endsWith(QLatin1String(".iso")))
        return parseDiscIso(path);
    if (low.endsWith(QLatin1String(".ffpfsc")))
        return ps5::parseFfpfscImage(path);
    if (low.endsWith(QLatin1String(".ffpkg")))
        return ps5::parseFfpkgImage(path);
    return parsePkgMagic(path);
}

} // namespace psg::detect
