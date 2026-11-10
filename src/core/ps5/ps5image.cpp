#include "ps5image.h"

#include "../bytes.h"
#include "../util.h"
#include "exfat.h"
#include "pfs.h"
#include "ps5pkg.h"
#include "ufs2.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace psg::ps5 {
namespace {

// Build the common PS5 result rows/entries once param.json is known.
// With `cacheImages` set (PFS inner images), the icon entries carry
// their bytes inline so the container view can close after parsing;
// trophy packs stay lazy chain reads resolved through the container.
GameInfo exfatResult(const ExfatImage &fs, const QString &path,
                     quint64 size, const QString &platformLabel,
                     const QString &innerName, bool cacheImages = false)
{
    GameInfo out;
    out.path = path;
    out.size = size;
    out.platform = Platform::PS5;
    out.iconEntry = QStringLiteral("icon0.png");

    QVariantMap meta;
    QString title;
    QList<SpecRow> extra;
    ExfatImage::Found pj;
    if (fs.find({QStringLiteral("sce_sys"), QStringLiteral("param.json")}, &pj)
        && !pj.isDir && pj.size < 100000) {
        const QByteArray raw = pj.noFat
            ? fs.readNoFat(pj.firstCluster, pj.size)
            : fs.readChain(pj.firstCluster, pj.size);
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isObject()) {
            meta = doc.object().toVariantMap();
            std::tie(title, extra) = paramJsonMeta(meta);
        }
    }

    out.ok = true;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.meta = meta;
    out.addRow(QStringLiteral("Platform"), platformLabel);
    out.addRow(QStringLiteral("Size"), fmtSize(size));
    if (!innerName.isEmpty())
        out.addRow(QStringLiteral("Inner file"), innerName);
    out.rows += extra;

    // With `cacheImages` the icon bytes are read through the view now;
    // the entry then carries them inline. Otherwise the entry stays a
    // lazy FAT-chain read resolved through the container on demand.
    const auto fillImageEntry = [&](Entry &e, quint32 first,
                                    bool noFat) -> bool {
        if (cacheImages) {
            const QByteArray data =
                noFat ? fs.readNoFat(first, e.size)
                      : fs.readChain(first, e.size);
            if (data.isEmpty()
                || data.left(8) != QByteArrayLiteral("\x89PNG\r\n\x1a\n"))
                return false;
            e.source = Entry::Source::Cached;
            e.cached = data;
        } else {
            e.source = Entry::Source::ExfatChain;
            e.exfatFirst = first;
            e.exfatNoFat = noFat;
        }
        e.containerPath = path;
        return true;
    };

    // icon0.png first, then every other PNG in sce_sys for the gallery.
    // A crafted image may list thousands of PNG entries; stop caching
    // past 64 images / 256 MB so parsing cannot exhaust memory.
    constexpr int kMaxCachedImages = 64;
    constexpr quint64 kMaxCachedBytes = 256ull << 20;
    int cachedImages = 0;
    quint64 cachedBytes = 0;
    quint32 eid = 0;
    ExfatImage::Found icon;
    if (fs.find({QStringLiteral("sce_sys"), QStringLiteral("icon0.png")},
                &icon)
        && !icon.isDir && icon.size > 0 && icon.size < 32000000) {
        Entry e;
        e.id = eid++;
        e.name = QStringLiteral("icon0.png");
        e.size = icon.size;
        if (fillImageEntry(e, icon.firstCluster, icon.noFat)) {
            out.entries.append(e);
            if (e.source == Entry::Source::Cached) {
                ++cachedImages;
                cachedBytes += quint64(e.cached.size());
            }
        }
    }
    QSet<QString> seen{{QStringLiteral("icon0.png")}};
    const QList<ExfatImage::DirItem> sceSys =
        fs.listPath({QStringLiteral("sce_sys")});
    for (const ExfatImage::DirItem &it : sceSys) {
        if (it.isDir || seen.contains(it.name.toLower())
            || !it.name.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
            || it.size == 0 || it.size > 32000000)
            continue;
        if (cachedImages >= kMaxCachedImages
            || cachedBytes >= kMaxCachedBytes)
            break;
        seen.insert(it.name.toLower());
        Entry e;
        e.id = eid++;
        e.name = it.name;
        e.size = it.size;
        if (fillImageEntry(e, it.firstCluster, it.noFat)) {
            out.entries.append(e);
            if (e.source == Entry::Source::Cached) {
                ++cachedImages;
                cachedBytes += quint64(e.cached.size());
            }
        }
    }

    // trophy packs for the Trophies tab, read on demand
    for (const ExfatPack &p : exfatScanPacks(fs)) {
        Entry e;
        e.id = eid++;
        e.name = p.relPath;
        e.size = p.size;
        e.source = Entry::Source::ExfatChain;
        e.exfatFirst = p.firstCluster;
        e.exfatNoFat = p.noFat;
        e.containerPath = path;
        out.entries.append(e);
    }
    return out;
}

// AMPR/LZ4 asset containers (LIZARD-style PS5 app dumps).
const char *const kAmprMagics[] = {"AMPRPAK4", "AMPRDAT3", "AMPRIDX3",
                                   "AMPRCRC1", "AMPRCFG1"};
const char kUnitySig[] = "UnityFS\x00";
const char *const kUnityMarkers[] = {"2022.3.57f1", "CAB-", "AssetBundle",
                                     ".resS", "globalgamemanagers"};

QString unityCompAt(const QByteArray &buf, qsizetype pos)
{
    // UnityFS header: signature(8) + ver(4) + readerVersion.. + flags u32BE
    qsizetype p = pos + 8 + 4;
    const int e1 = buf.indexOf('\x00', p);
    if (e1 < 0)
        return QString();
    const int e2 = buf.indexOf('\x00', e1 + 1);
    if (e2 < 0)
        return QString();
    p = qsizetype(e2) + 1 + 8 + 4 + 4;
    if (p + 4 > buf.size())
        return QString();
    const quint32 fl = bytes::rdU32BE(buf, p);
    switch (fl & 0x3F) {
    case 0: return QStringLiteral("raw");
    case 1: return QStringLiteral("LZMA");
    case 2: return QStringLiteral("LZ4");
    case 3: return QStringLiteral("LZ4HC");
    default: return QString();
    }
}

// Strided probe of an AMPR .pak: "LZ4HC" / "LZ4" / "LZMA" / "Unity" / "".
QString probePakCodec(const QString &filePath, quint64 size)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QStringList comps;
    bool seenLz4 = false;
    bool seenUnity = false;

    QList<quint64> offs;
    quint64 win = size;
    if (size <= (32ull << 20)) {
        offs = {0};
    } else {
        const quint64 step = 16ull << 20;
        win = 1ull << 20;
        for (quint64 o = 0; o + win <= size; o += step)
            offs.append(o);
        if (offs.isEmpty() || offs.last() != size - win)
            offs.append(size - win);
    }
    const qsizetype winLen = qsizetype(win + 256);
    for (quint64 off : offs) {
        f.seek(qint64(off));
        const QByteArray buf = f.read(winLen);
        if (buf.isEmpty())
            continue;
        qsizetype i = 0;
        while (comps.size() < 3) {
            i = buf.indexOf(kUnitySig, i);
            if (i < 0)
                break;
            const QString c = unityCompAt(buf, i);
            if (!c.isEmpty()) {
                comps.append(c);
                if (c == QLatin1String("LZ4HC"))
                    return c;
            }
            ++i;
        }
        const QByteArray low = buf.toLower();
        if (low.contains("lz4"))
            seenLz4 = true;
        if (!seenUnity) {
            for (const char *m : kUnityMarkers) {
                if (buf.contains(m)) {
                    seenUnity = true;
                    break;
                }
            }
        }
    }
    if (comps.contains(QLatin1String("LZ4")))
        return QStringLiteral("LZ4");
    if (comps.contains(QLatin1String("LZMA")))
        return QStringLiteral("LZMA");
    if (seenLz4)
        return QStringLiteral("LZ4");
    if (seenUnity || !comps.isEmpty())
        return QStringLiteral("Unity");
    return QString();
}

struct AmprFile {
    QString name;
    quint64 size = 0;
    QString magic;
    QString codec;
};

QList<AmprFile> scanAmpr(const QString &path)
{
    QList<AmprFile> out;
    const QDir dir(path);
    const QStringList files = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &fn : files) {
        const QString fp = dir.filePath(fn);
        const QString ln = fn.toLower();
        const bool nameHit = ln.startsWith(QLatin1String("ampr_"))
            || ln.endsWith(QLatin1String(".pak"))
            || ln.endsWith(QLatin1String(".index"))
            || ln.endsWith(QLatin1String(".crc"))
            || ln.endsWith(QLatin1String(".runtime"));
        if (!nameHit)
            continue;
        QFile fh(fp);
        if (!fh.open(QIODevice::ReadOnly))
            continue;
        const QByteArray magic = fh.read(8);
        bool known = false;
        for (const char *m : kAmprMagics) {
            if (magic == QByteArray(m))
                known = true;
        }
        if (!known)
            continue;
        AmprFile a;
        a.name = fn;
        a.size = quint64(QFileInfo(fp).size());
        a.magic = QString::fromLatin1(magic);
        a.codec = (magic == QByteArrayLiteral("AMPRDAT3"))
            ? probePakCodec(fp, a.size)
            : QString();
        out.append(a);
    }
    return out;
}

// Common builder for UFS2-backed results (.ffpkg and nested .ffpfsc):
// sce_sys/param.json + icon0.png + every other sce_sys PNG, with the
// icon bytes cached inline (small files, the device closes after parse).
GameInfo ufs2Result(const Ufs2File &fs, const QString &path, quint64 size,
                     const QString &platformLabel, const QString &innerName)
{
    GameInfo out;
    out.path = path;
    out.size = size;
    out.platform = Platform::PS5;
    out.iconEntry = QStringLiteral("icon0.png");

    QVariantMap meta;
    QString title;
    QList<SpecRow> extra;
    Ufs2File::Found pj;
    if (fs.find({QStringLiteral("sce_sys"), QStringLiteral("param.json")}, &pj)
        && !pj.isDir && pj.size > 0 && pj.size < 100000) {
        const QByteArray raw = fs.readFile(pj, 100000);
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isObject()) {
            meta = doc.object().toVariantMap();
            std::tie(title, extra) = paramJsonMeta(meta);
        }
    }

    out.ok = true;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.meta = meta;
    out.addRow(QStringLiteral("Platform"), platformLabel);
    out.addRow(QStringLiteral("Size"), fmtSize(size));
    if (!innerName.isEmpty())
        out.addRow(QStringLiteral("Inner file"), innerName);
    out.rows += extra;

    // icon0.png first, then the other sce_sys PNGs in name order (with
    // the same crafted-file budget as the exFAT path)
    constexpr int kMaxCachedImages = 64;
    constexpr quint64 kMaxCachedBytes = 256ull << 20;
    int cachedImages = 0;
    quint64 cachedBytes = 0;
    quint32 eid = 0;
    Ufs2File::Found icon;
    if (fs.find({QStringLiteral("sce_sys"), QStringLiteral("icon0.png")},
                &icon)
        && !icon.isDir && icon.size > 0 && icon.size < 32000000) {
        const QByteArray data = fs.readFile(icon, 32000000);
        if (!data.isEmpty()
            && data.left(8) == QByteArrayLiteral("\x89PNG\r\n\x1a\n")) {
            Entry e;
            e.id = eid++;
            e.name = QStringLiteral("icon0.png");
            e.size = quint64(data.size());
            e.source = Entry::Source::Cached;
            e.cached = data;
            out.entries.append(e);
            ++cachedImages;
            cachedBytes += quint64(data.size());
        }
    }
    Ufs2File::Found sceSysDir;
    if (fs.find({QStringLiteral("sce_sys")}, &sceSysDir) && sceSysDir.isDir) {
        QList<Ufs2File::DirEntry> sceSys = fs.listDir(sceSysDir.ino);
        // icon0.png first, then the remaining PNGs in name order
        std::sort(sceSys.begin(), sceSys.end(),
                  [](const Ufs2File::DirEntry &a, const Ufs2File::DirEntry &b) {
                      const bool aIcon = a.name == QLatin1String("icon0.png");
                      const bool bIcon = b.name == QLatin1String("icon0.png");
                      if (aIcon != bIcon)
                          return aIcon;
                      return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                  });
        for (const Ufs2File::DirEntry &it : sceSys) {
            if (it.isDir || it.name == QLatin1String("icon0.png")
                || !it.name.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)
                || it.size == 0 || it.size > 32000000)
                continue;
            if (cachedImages >= kMaxCachedImages
                || cachedBytes >= kMaxCachedBytes)
                break;
            const QByteArray data = fs.readFile({it.ino, it.size, false},
                                                32000000);
            if (data.isEmpty()
                || data.left(8) != QByteArrayLiteral("\x89PNG\r\n\x1a\n"))
                continue;
            Entry e;
            e.id = eid++;
            e.name = it.name;
            e.size = quint64(data.size());
            e.source = Entry::Source::Cached;
            e.cached = data;
            out.entries.append(e);
            ++cachedImages;
            cachedBytes += quint64(data.size());
        }
    }
    return out;
}

} // namespace

GameInfo parseExfatImage(const QString &path)
{
    bool ok = false;
    QString err;
    const ExfatImage fs(path, &ok, &err);
    if (!ok) {
        GameInfo bad;
        bad.path = path;
        bad.error = err;
        return bad;
    }
    return exfatResult(fs, path, quint64(QFileInfo(path).size()),
                       QStringLiteral("PS5 exFAT image"), QString());
}

GameInfo parseFfpfscImage(const QString &path)
{
    const quint64 size = quint64(QFileInfo(path).size());
    GameInfo bad;
    bad.path = path;

    PfsFile pfs(path);
    if (!pfs.ok()) {
        bad.error = pfs.error();
        return bad;
    }
    PfsFile::InnerFile inner;
    if (!pfs.openInner(&inner)) {
        bad.error = pfs.error();
        return bad;
    }
    if (inner.name.toLower().endsWith(QLatin1String(".ffpkg"))) {
        bool ok = false;
        QString err;
        const Ufs2File fs(inner.device, inner.name, &ok, &err);
        if (!ok) {
            bad.error = QStringLiteral("inner file: %1").arg(err);
            return bad;
        }
        return ufs2Result(fs, path, size, QStringLiteral("PS5 ffpfsc image"),
                          inner.name);
    }
    bool ok = false;
    QString err;
    const ExfatImage fs(inner.device, inner.name, &ok, &err);
    if (!ok) {
        bad.error = QStringLiteral("inner file: %1").arg(err);
        return bad;
    }
    return exfatResult(fs, path, size, QStringLiteral("PS5 ffpfsc image"),
                       inner.name, /*cacheImages=*/true);
}

GameInfo parseFfpkgImage(const QString &path)
{
    const quint64 size = quint64(QFileInfo(path).size());
    GameInfo bad;
    bad.path = path;

    bool ok = false;
    QString err;
    const Ufs2File fs(path, &ok, &err);
    if (!ok) {
        bad.error = err;
        return bad;
    }
    return ufs2Result(fs, path, size, QStringLiteral("PS5 ffpkg image"),
                      QString());
}

GameInfo parseAppFolder(const QString &path)
{
    GameInfo out;
    out.path = path;
    out.platform = Platform::PS5;

    QVariantMap meta;
    QString title;
    QList<SpecRow> extra;
    const QString pjPath = QDir(path).filePath(QStringLiteral("sce_sys/param.json"));
    QFile pjFile(pjPath);
    if (pjFile.open(QIODevice::ReadOnly)) {
        const QByteArray raw = pjFile.read(100000);
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isObject()) {
            meta = doc.object().toVariantMap();
            std::tie(title, extra) = paramJsonMeta(meta);
        }
    }

    // a PS5 app dump always carries sce_sys/param.json; without it this
    // is not a PS5 folder — and must not be labeled one (that used to
    // swallow PS2/PS4 dumps and arbitrary folders alike)
    if (meta.isEmpty()) {
        out.error = QStringLiteral(
            "not a PS5 app folder (no sce_sys/param.json)");
        return out;
    }

    // the whole dump tree: sce_sys, eboot.bin, sce_module, data — like
    // the image viewers list their contents
    const FolderScan scan = scanLocalFolder(path);

    out.ok = true;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.meta = meta;
    out.size = scan.totalSize;
    out.entries = scan.entries;
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS5 app folder"));
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)")
                   .arg(fmtSize(scan.totalSize))
                   .arg(scan.fileCount));

    const QList<AmprFile> ampr = scanAmpr(path);
    if (!ampr.isEmpty()) {
        quint64 amprTotal = 0;
        int lz4 = 0, lz4hc = 0;
        for (const AmprFile &a : ampr) {
            amprTotal += a.size;
            if (a.codec == QLatin1String("LZ4"))
                ++lz4;
            else if (a.codec == QLatin1String("LZ4HC"))
                ++lz4hc;
        }
        QString label = QStringLiteral("%1 AMPR containers (%2)")
                            .arg(ampr.size())
                            .arg(fmtSize(amprTotal));
        QStringList codecs;
        if (lz4)
            codecs << (lz4 > 1 ? QStringLiteral("LZ4 x%1").arg(lz4)
                               : QStringLiteral("LZ4"));
        if (lz4hc)
            codecs << (lz4hc > 1 ? QStringLiteral("LZ4HC x%1").arg(lz4hc)
                                 : QStringLiteral("LZ4HC"));
        if (!codecs.isEmpty())
            label += QStringLiteral(" · ") + codecs.join(QStringLiteral(" · "));
        out.addRow(QStringLiteral("Assets"), label);
        for (const AmprFile &a : ampr) {
            out.amprLines.append(
                QStringLiteral("%1 = %2%3")
                    .arg(a.name, a.magic,
                         a.codec.isEmpty() ? QString()
                                           : QStringLiteral(" / ") + a.codec));
        }
    }
    out.rows += extra;

    // codec labels for the AMPR asset containers, on their tree entries
    for (const AmprFile &a : ampr) {
        for (Entry &e : out.entries) {
            if (e.name.compare(a.name, Qt::CaseInsensitive) == 0) {
                e.codec = a.codec.isEmpty() ? QStringLiteral("-") : a.codec;
                break;
            }
        }
    }

    // cover icon: sce_sys/icon0.png wherever it sits in the tree
    for (const Entry &e : out.entries) {
        if (e.name.compare(QLatin1String("sce_sys/icon0.png"),
                           Qt::CaseInsensitive)
            == 0) {
            out.iconEntry = e.name;
            break;
        }
    }
    if (out.iconEntry.isEmpty()) {
        for (const Entry &e : out.entries) {
            if (QFileInfo(e.name).fileName()
                    .compare(QLatin1String("icon0.png"), Qt::CaseInsensitive)
                == 0) {
                out.iconEntry = e.name;
                break;
            }
        }
    }
    return out;
}

} // namespace psg::ps5
