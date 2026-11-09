#include "ps4folder.h"

#include "../sfo.h"
#include "../util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace psg::ps4 {
namespace {

QString findDirCI(const QDir &dir, const QString &name)
{
    const QStringList dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &d : dirs) {
        if (d.compare(name, Qt::CaseInsensitive) == 0)
            return dir.filePath(d);
    }
    return QString();
}

QString findFileCI(const QDir &dir, const QString &name)
{
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &fn : files) {
        if (fn.compare(name, Qt::CaseInsensitive) == 0)
            return dir.filePath(fn);
    }
    return QString();
}

} // namespace

bool isPs4AppFolder(const QString &path)
{
    const QString sce = findDirCI(QDir(path), QStringLiteral("sce_sys"));
    if (sce.isEmpty())
        return false;
    return !findFileCI(QDir(sce), QStringLiteral("param.sfo")).isEmpty();
}

GameInfo parsePs4Folder(const QString &path)
{
    GameInfo out;
    out.path = path;
    if (!isPs4AppFolder(path)) {
        out.error = QStringLiteral("not a PS4 app folder");
        return out;
    }

    QVariantMap sfo;
    const QString sceSys = findDirCI(QDir(path), QStringLiteral("sce_sys"));
    QFile sf(findFileCI(QDir(sceSys), QStringLiteral("param.sfo")));
    if (sf.open(QIODevice::ReadOnly)) {
        const QByteArray raw = sf.read(1000000);
        if (raw.left(4) == QByteArrayLiteral("\x00PSF"))
            sfo = parseSfo(raw);
    }
    if (sfo.isEmpty()) {
        out.error = QStringLiteral("unreadable sce_sys/param.sfo");
        return out;
    }

    // the whole dump: eboot.bin, sce_module/, sce_sys/ — like the PKG
    // viewer lists its entries
    const FolderScan scan = scanLocalFolder(path);

    const QString title = sfo.value(QStringLiteral("TITLE")).toString();
    const QString cid = sfo.value(QStringLiteral("CONTENT_ID")).toString();
    const QString cat = sfo.value(QStringLiteral("CATEGORY")).toString();
    // update folders (gp): VERSION names the base it applies to,
    // APP_VER is the actual patch version
    QString ver = sfo.value(QStringLiteral("VERSION")).toString();
    if (cat.toLower() == QLatin1String("gp")
        && sfo.contains(QStringLiteral("APP_VER")))
        ver = sfo.value(QStringLiteral("APP_VER")).toString();

    QString tid = sfo.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty())
        tid = titleIdFromCid(cid);
    if (tid.isEmpty())
        tid = titleIdFromFileName(path);

    out.ok = true;
    out.platform = Platform::PS4;
    out.title = title.isEmpty() ? QFileInfo(path).fileName() : title;
    out.meta = sfo;
    out.size = scan.totalSize;
    out.entries = scan.entries;
    out.patchTid = tid;
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS4 app folder"));
    out.addRow(QStringLiteral("Title ID"), tid);
    out.addRow(QStringLiteral("Content ID"),
               cid.isEmpty() ? QStringLiteral("-") : cid);
    out.addRow(QStringLiteral("Region"),
               cid.isEmpty() ? QStringLiteral("-") : contentRegion(cid));
    out.addRow(QStringLiteral("Type"), ps4PkgType(cat));
    out.addRow(QStringLiteral("Version"), ver.isEmpty() ? QStringLiteral("-") : ver);
    out.addRow(QStringLiteral("Min. System"),
               fmtFw32(sfo.value(QStringLiteral("SYSTEM_VER"))));
    const QString sdk = ps4SdkVersion(sfo);
    if (!sdk.isEmpty())
        out.addRow(QStringLiteral("SDK"), sdk);
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)")
                   .arg(fmtSize(scan.totalSize))
                   .arg(scan.fileCount));

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

} // namespace psg::ps4
