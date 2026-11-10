#include "ps2folder.h"

#include "ps2disc.h"
#include "../util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace psg::ps2 {
namespace {

QString findFileCI(const QDir &dir, const QString &name)
{
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &fn : files) {
        if (fn.compare(name, Qt::CaseInsensitive) == 0)
            return dir.filePath(fn);
    }
    return QString();
}

QVariantMap readSystemCnf(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QVariantMap();
    const QByteArray raw = f.read(100000);
    if (raw.isEmpty())
        return QVariantMap();
    return parseSystemCnf(raw);
}

} // namespace

QString ps2FolderBase(const QString &path)
{
    const QDir dir(path);
    const QString cnfPath = findFileCI(dir, QStringLiteral("SYSTEM.CNF"));
    if (cnfPath.isEmpty())
        return QString();
    const QVariantMap cnf = readSystemCnf(cnfPath);
    const QString boot = cnf.value(QStringLiteral("BOOT2")).toString().trimmed();
    if (!boot.startsWith(QLatin1String("cdrom0:"), Qt::CaseInsensitive))
        return QString();
    return path;
}

GameInfo parsePs2Folder(const QString &path)
{
    GameInfo out;
    out.path = path;
    const QString base = ps2FolderBase(path);
    if (base.isEmpty()) {
        out.error = QStringLiteral("not a PS2 game folder");
        return out;
    }

    const QVariantMap cnf =
        readSystemCnf(findFileCI(QDir(base), QStringLiteral("SYSTEM.CNF")));

    // the whole extracted disc, exactly like the ISO viewer lists it
    const FolderScan scan = scanLocalFolder(path);

    const QString boot = cnf.value(QStringLiteral("BOOT2")).toString().trimmed();
    QString serial = ps2NormalizeSerial(boot);
    if (serial.isEmpty())
        serial = ps2NormalizeSerial(QFileInfo(path).fileName());
    const QString ver = cnf.value(QStringLiteral("VER")).toString().trimmed();
    const QString vmode = cnf.value(QStringLiteral("VMODE")).toString().trimmed();
    QString bootDisp = boot;
    for (const QString &pfx : {QStringLiteral("cdrom0:\\"), QStringLiteral("cdrom0:")})
        if (bootDisp.startsWith(pfx, Qt::CaseInsensitive))
            bootDisp.remove(0, pfx.size());
    bootDisp = bootDisp.section(u';', 0, 0);
    const QString title = ps2TitleFromFileName(path);
    const QString tid = serial.isEmpty() ? QFileInfo(path).completeBaseName()
                                         : serial;

    out.ok = true;
    out.platform = Platform::PS2;
    out.title = title.isEmpty() ? tid : title;
    out.meta = cnf;
    out.ps2Serial = serial;
    out.size = scan.totalSize;
    out.entries = scan.entries;
    out.addRow(QStringLiteral("Platform"),
               QStringLiteral("PS2 folder (disc dump)"));
    out.addRow(QStringLiteral("Title ID"),
               serial.isEmpty() ? QStringLiteral("-") : serial);
    out.addRow(QStringLiteral("Region"), ps2Region(serial, vmode));
    out.addRow(QStringLiteral("Version"),
               ver.isEmpty() || ver == QLatin1String("-")
                   ? QStringLiteral("-")
                   : QStringLiteral("v") + ver);
    out.addRow(QStringLiteral("VMODE"), vmode.isEmpty() ? QStringLiteral("-") : vmode);
    out.addRow(QStringLiteral("Boot"), bootDisp.isEmpty() ? QStringLiteral("-") : bootDisp);
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)")
                   .arg(fmtSize(scan.totalSize))
                   .arg(scan.fileCount));
    return out;
}

} // namespace psg::ps2
