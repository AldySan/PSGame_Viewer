#include "ps3folder.h"

#include "../sfo.h"
#include "../util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace psg::ps3 {
namespace {

// The cover icon: PS3_GAME/ICON0.PNG wherever it sits in the tree —
// exact canonical names first, then any ICON0.PNG by file name.
QString findIconEntry(const QList<Entry> &entries)
{
    for (const QString &want : {QStringLiteral("ICON0.PNG"),
                                QStringLiteral("PS3_GAME/ICON0.PNG")}) {
        for (const Entry &e : entries) {
            if (e.name.compare(want, Qt::CaseInsensitive) == 0)
                return e.name;
        }
    }
    for (const Entry &e : entries) {
        if (QFileInfo(e.name).fileName()
                .compare(QLatin1String("ICON0.PNG"), Qt::CaseInsensitive)
            == 0)
            return e.name;
    }
    return QString();
}

} // namespace

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

    // the whole folder tree, exactly like the disc-image viewers list
    // it: PS3_DISC.SFB, PS3_GAME/PARAM.SFO, USRDIR, TROPDIR — not just
    // the PNG/TRP highlights
    const FolderScan scan = scanLocalFolder(path);

    const QString title = sfo.value(QStringLiteral("TITLE")).toString();
    QString tid = sfo.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty())
        tid = QFileInfo(path).fileName();
    QString ver = sfo.value(QStringLiteral("VERSION")).toString();
    if (ver.isEmpty())
        ver = sfo.value(QStringLiteral("APP_VER")).toString();

    const QString cat = sfo.value(QStringLiteral("CATEGORY")).toString().trimmed();
    QString catLabel;
    if (cat.toUpper() == QLatin1String("DG"))
        catLabel = QStringLiteral("Disc Game (%1)").arg(cat);
    else if (cat.toUpper() == QLatin1String("HG"))
        catLabel = QStringLiteral("HDD Game (%1)").arg(cat);
    else
        catLabel = cat.isEmpty() ? QStringLiteral("-") : cat;

    out.ok = true;
    out.platform = Platform::PS3;
    out.title = title.isEmpty() ? tid : title;
    out.meta = sfo;
    out.size = scan.totalSize;
    out.entries = scan.entries;
    out.iconEntry = findIconEntry(out.entries);
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS3 folder"));
    out.addRow(QStringLiteral("Title ID"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : sfo.value(QStringLiteral("TITLE_ID")).toString());
    out.addRow(QStringLiteral("Region"),
               sfo.isEmpty()
                   ? QStringLiteral("-")
                   : ps3RegionFromTitleId(
                         sfo.value(QStringLiteral("TITLE_ID")).toString()));
    out.addRow(QStringLiteral("Category"), catLabel);
    out.addRow(QStringLiteral("Version"), ver.isEmpty() ? QStringLiteral("-") : ver);
    out.addRow(QStringLiteral("Min. System"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : fmtFw32(sfo.value(QStringLiteral("PS3_SYSTEM_VER"))));
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)")
                   .arg(fmtSize(scan.totalSize))
                   .arg(scan.fileCount));
    return out;
}

} // namespace psg::ps3
