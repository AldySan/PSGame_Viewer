#include "extract.h"

#include "ps3/ps3pkg.h"
#include "ps5/exfat.h"
#include "ps5/pfs.h"
#include "sfo.h"
#include "util.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace psg {

QString sanitizePkgPath(const QString &name)
{
    QStringList parts;
    QString normalized = name;
    normalized.replace(u'\\', u'/');
    for (QString p : normalized.split(u'/')) {
        p = p.trimmed();
        if (p.isEmpty() || p == QLatin1String(".") || p == QLatin1String(".."))
            continue;
        for (const QChar c : QString::fromUtf8("<>:\"|?*"))
            p.remove(c);
        if (!p.isEmpty())
            parts.append(p);
    }
    return parts.join(u'/');
}

QByteArray readEntryBytes(const Entry &e, const QString &containerPath,
                          quint64 limit)
{
    // The read sizes below are `int`; cap at INT_MAX as well so a crafted
    // entry size of 2 GiB or more cannot overflow the cast (extract
    // callers pass a 2 GiB limit, which equals INT_MAX + 1).
    if (e.size == 0 || e.size > qMin<quint64>(limit, quint64(INT_MAX)))
        return QByteArray();
    switch (e.source) {
    case Entry::Source::Ps3Crypto: {
        QFile f(e.containerPath.isEmpty() ? containerPath : e.containerPath);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        return ps3::decryptRange(f, e.ps3DataOff, e.ps3Retail, e.ps3Keymat,
                                 e.ps3EntryOff, e.size);
    }
    case Entry::Source::ExfatChain: {
        const QString cp = e.containerPath.isEmpty() ? containerPath
                                                    : e.containerPath;
        bool ok = false;
        if (cp.endsWith(QLatin1String(".ffpfsc"),
                        Qt::CaseInsensitive)) {
            // FAT chains into the inner exFAT image of a PFS container
            ps5::PfsFile pfs(cp);
            ps5::PfsFile::InnerFile inner;
            QString err;
            if (!pfs.ok() || !pfs.openInner(&inner) || !inner.device) {
                return QByteArray();
            }
            const ExfatImage fs(inner.device, inner.name, &ok, &err);
            if (!ok)
                return QByteArray();
            return e.exfatNoFat ? fs.readNoFat(e.exfatFirst, e.size)
                                : fs.readChain(e.exfatFirst, e.size);
        }
        const ExfatImage fs(cp, &ok);
        if (!ok)
            return QByteArray();
        return e.exfatNoFat ? fs.readNoFat(e.exfatFirst, e.size)
                            : fs.readChain(e.exfatFirst, e.size);
    }
    case Entry::Source::LocalFile: {
        QFile f(e.localPath);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        return f.read(int(qMin<quint64>(e.size, limit)));
    }
    case Entry::Source::Cached:
        return e.cached;
    case Entry::Source::Offset: {
        if (e.offset < 0)
            return QByteArray();
        QFile f(e.containerPath.isEmpty() ? containerPath : e.containerPath);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        f.seek(e.offset);
        return f.read(int(e.size));
    }
    default:
        return QByteArray();
    }
}

ExtractResult extractEntries(const QList<Entry> &entries,
                             const QString &containerPath,
                             const QString &destDir,
                             const std::function<void(const QString &)> &progress)
{
    ExtractResult res;
    for (const Entry &e : entries) {
        const QString rel = sanitizePkgPath(e.name);
        if (rel.isEmpty() || e.size == 0)
            continue;
        const QString outPath = QDir(destDir).filePath(rel);
        QDir().mkpath(QFileInfo(outPath).absolutePath());
        bool written = false;
        switch (e.source) {
        case Entry::Source::LocalFile:
            written = QFile::copy(e.localPath, outPath);
            break;
        case Entry::Source::Cached: {
            QFile w(outPath);
            written = w.open(QIODevice::WriteOnly) && w.write(e.cached) == e.cached.size();
            break;
        }
        case Entry::Source::Offset: {
            if (e.offset < 0)
                break;
            QFile r(e.containerPath.isEmpty() ? containerPath : e.containerPath);
            QFile w(outPath);
            if (!r.open(QIODevice::ReadOnly) || !w.open(QIODevice::WriteOnly))
                break;
            quint64 left = e.size;
            written = true;
            while (left > 0) {
                const QByteArray buf = r.read(int(qMin<quint64>(left, 4 << 20)));
                if (buf.isEmpty() || w.write(buf) != buf.size()) {
                    written = false;
                    break;
                }
                left -= quint64(buf.size());
            }
            break;
        }
        case Entry::Source::Ps3Crypto:
        case Entry::Source::ExfatChain: {
            const QByteArray data = readEntryBytes(
                e, containerPath, quint64(2) << 30);
            if (data.isEmpty())
                break;
            QFile w(outPath);
            written = w.open(QIODevice::WriteOnly) && w.write(data) == data.size();
            break;
        }
        default:
            break;
        }
        if (written)
            ++res.ok;
        else
            ++res.fail;
        if (progress)
            progress(rel);
    }
    return res;
}

bool extractEntryTo(const Entry &e, const QString &containerPath,
                    const QString &outFile)
{
    if (e.size == 0)
        return false;
    QDir().mkpath(QFileInfo(outFile).absolutePath());
    // plain offsets stream through so multi-GB ISO files stay cheap
    if (e.source == Entry::Source::Offset && e.offset >= 0) {
        QFile r(e.containerPath.isEmpty() ? containerPath : e.containerPath);
        QFile w(outFile);
        if (!r.open(QIODevice::ReadOnly) || !w.open(QIODevice::WriteOnly))
            return false;
        r.seek(e.offset);
        quint64 left = e.size;
        while (left > 0) {
            const QByteArray buf = r.read(int(qMin<quint64>(left, 4 << 20)));
            if (buf.isEmpty() || w.write(buf) != buf.size())
                return false;
            left -= quint64(buf.size());
        }
        return true;
    }
    const QByteArray data = readEntryBytes(e, containerPath, quint64(2) << 30);
    if (data.isEmpty())
        return false;
    QFile w(outFile);
    return w.open(QIODevice::WriteOnly) && w.write(data) == data.size();
}

QString writeGp4(const GameInfo &info, const QString &extractedDir,
                 const QString &gp4Path)
{
    QString cid = info.meta.value(QStringLiteral("CONTENT_ID")).toString();
    if (cid.isEmpty())
        cid = info.contentId();
    QString tid = info.meta.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty())
        tid = info.titleId();
    QString cat = info.meta.value(QStringLiteral("CATEGORY")).toString();
    if (cat.isEmpty())
        cat = QStringLiteral("gd");
    QString ver = info.meta.value(QStringLiteral("APP_VER")).toString();
    if (ver.isEmpty())
        ver = info.meta.value(QStringLiteral("VERSION")).toString();
    if (ver.isEmpty())
        ver = info.version().isEmpty() ? QStringLiteral("01.00") : info.version();
    QString title = info.meta.value(QStringLiteral("TITLE")).toString();
    if (title.isEmpty())
        title = info.title.isEmpty() ? tid : info.title;

    QStringList files;
    QDirIterator it(extractedDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString fp = it.next();
        QString rel = QDir(extractedDir).relativeFilePath(fp);
        rel.replace(u'\\', u'/');
        if (rel.compare(QFileInfo(gp4Path).fileName(), Qt::CaseInsensitive) == 0)
            continue;
        files.append(rel);
    }
    const auto esc = [](const QString &s) { return s.toHtmlEscaped(); };

    QString gp4;
    gp4 += QStringLiteral("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    gp4 += QStringLiteral("<psproject fmt=\"gp4\" version=\"1000\">\n");
    gp4 += QStringLiteral("  <volume>\n");
    gp4 += QStringLiteral("    <volume_type>pkg_ps4_app</volume_type>\n");
    gp4 += QStringLiteral("    <volume_id>PS4VOLUME</volume_id>\n");
    gp4 += QStringLiteral("    <volume_ts>2024-01-01 00:00:00</volume_ts>\n");
    if (cid.isEmpty())
        cid = QStringLiteral("UP0000-%1_00-0000000000000000").arg(tid);
    gp4 += QStringLiteral("    <package content_id=\"%1\" "
                          "passcode=\"00000000000000000000000000000000\"/>\n")
               .arg(esc(cid));
    gp4 += QStringLiteral("    <chunk_info chunk_count=\"1\" scenario_count=\"1\"/>\n");
    gp4 += QStringLiteral("  </volume>\n");
    gp4 += QStringLiteral("  <contents>\n");
    for (const QString &r : files)
        gp4 += QStringLiteral("    <file targ_path=\"%1\" orig_path=\"%1\"/>\n").arg(esc(r));
    gp4 += QStringLiteral("  </contents>\n");
    gp4 += QStringLiteral("  <chunks>\n    <chunk id=\"0\" label=\"Chunk #0\">\n");
    for (const QString &r : files)
        gp4 += QStringLiteral("      <file idref=\"%1\"/>\n").arg(esc(r));
    gp4 += QStringLiteral("    </chunk>\n  </chunks>\n");
    gp4 += QStringLiteral("  <scenarios default_id=\"0\">\n");
    gp4 += QStringLiteral("    <scenario id=\"0\" type=\"sp\" "
                          "initial_chunk_count=\"1\" label=\"Single Play\">\n");
    gp4 += QStringLiteral("      <chunk idref=\"0\"/>\n");
    gp4 += QStringLiteral("    </scenario>\n  </scenarios>\n");
    gp4 += QStringLiteral("</psproject>\n");

    QFile f(gp4Path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(gp4.toUtf8());
        return gp4Path;
    }
    return QString();
}

} // namespace psg
