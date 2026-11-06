#include "ps3folder.h"

#include "../sfo.h"
#include "../util.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace psg::ps3 {

QString ps3FolderBase(const QString &path)
{
    const QDir dir(path);
    if (QFile::exists(dir.filePath(QStringLiteral("PS3_GAME/PARAM.SFO"))))
        return dir.filePath(QStringLiteral("PS3_GAME"));
    if (QFile::exists(dir.filePath(QStringLiteral("PARAM.SFO")))) {
        static const char *markers[] = {"USRDIR", "TROPDIR", "PS3_GAME",
                                        "PS3_UPDATE"};
        for (const char *m : markers) {
            if (QFile::exists(dir.filePath(QString::fromLatin1(m))))
                return path;
        }
    }
    return QString();
}

GameInfo parsePs3Folder(const QString &path)
{
    GameInfo out;
    out.path = path;
    const QString base = ps3FolderBase(path);
    if (base.isEmpty()) {
        out.error = QStringLiteral("not a PS3 game folder");
        return out;
    }

    QVariantMap sfo;
    QFile sfoFile(QDir(base).filePath(QStringLiteral("PARAM.SFO")));
    if (sfoFile.open(QIODevice::ReadOnly)) {
        const QByteArray raw = sfoFile.read(1000000);
        if (raw.left(4) == QByteArrayLiteral("\x00PSF"))
            sfo = parseSfo(raw);
    }

    quint64 total = 0;
    int nfiles = 0;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++nfiles;
        total += quint64(it.fileInfo().size());
    }
    out.size = total;

    const QString title = sfo.value(QStringLiteral("TITLE")).toString();
    QString tid = sfo.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty())
        tid = QFileInfo(path).fileName();
    QString ver = sfo.value(QStringLiteral("VERSION")).toString();
    if (ver.isEmpty())
        ver = sfo.value(QStringLiteral("APP_VER")).toString();

    out.ok = true;
    out.platform = Platform::PS3;
    out.title = title.isEmpty() ? tid : title;
    out.meta = sfo;
    out.iconEntry = QStringLiteral("icon0.png");
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS3 folder"));
    out.addRow(QStringLiteral("Title ID"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : sfo.value(QStringLiteral("TITLE_ID")).toString());
    out.addRow(QStringLiteral("Region"),
               sfo.isEmpty()
                   ? QStringLiteral("-")
                   : ps3RegionFromTitleId(
                         sfo.value(QStringLiteral("TITLE_ID")).toString()));
    out.addRow(QStringLiteral("Version"), ver.isEmpty() ? QStringLiteral("-") : ver);
    out.addRow(QStringLiteral("Min. System"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : fmtFw32(sfo.value(QStringLiteral("PS3_SYSTEM_VER"))));
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)").arg(fmtSize(total)).arg(nfiles));

    // PNG gallery from the base folder
    quint32 eid = 0;
    const QStringList baseFiles = QDir(base).entryList(
        QDir::Files, QDir::Name);
    for (const QString &fn : baseFiles) {
        if (!fn.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
            continue;
        const QString fp = QDir(base).filePath(fn);
        const qint64 sz = QFileInfo(fp).size();
        if (sz <= 0)
            continue;
        Entry e;
        e.id = eid++;
        e.name = fn;
        e.size = quint64(sz);
        e.source = Entry::Source::LocalFile;
        e.localPath = fp;
        out.entries.append(e);
    }

    // Trophy packs (TROPDIR/NPWRxxxxx_00/TROPHY.TRP) for the Trophies tab
    const QDir tropdir(QDir(base).filePath(QStringLiteral("TROPDIR")));
    const QStringList npDirs = tropdir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &np : npDirs) {
        const QDir npd(tropdir.filePath(np));
        const QStringList trps = npd.entryList(QDir::Files, QDir::Name);
        for (const QString &fn : trps) {
            if (!fn.endsWith(QLatin1String(".trp"), Qt::CaseInsensitive))
                continue;
            const QString fp = npd.filePath(fn);
            const qint64 sz = QFileInfo(fp).size();
            if (sz <= 0 || sz >= 300000000)
                continue;
            Entry e;
            e.id = eid++;
            e.name = QStringLiteral("TROPDIR/%1/%2").arg(np, fn);
            e.size = quint64(sz);
            e.source = Entry::Source::LocalFile;
            e.localPath = fp;
            out.entries.append(e);
        }
    }

    for (const Entry &e : out.entries) {
        if (e.name.compare(QLatin1String("ICON0.PNG"), Qt::CaseInsensitive) == 0) {
            out.iconEntry = e.name;
            break;
        }
    }
    return out;
}

} // namespace psg::ps3
