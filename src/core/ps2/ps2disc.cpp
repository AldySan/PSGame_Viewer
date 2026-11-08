#include "ps2disc.h"

#include "../iso9660.h"
#include "../util.h"

#include <QFile>
#include <QFileInfo>

namespace psg::ps2 {
namespace {

QVariantMap parseSystemCnf(const QByteArray &raw)
{
    QVariantMap cnf;
    QString txt = QString::fromUtf8(raw);
    txt.replace(u'\r', u'\n');
    const QStringList lines = txt.split(u'\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || !line.contains(u'='))
            continue;
        const int eq = line.indexOf(u'=');
        cnf.insert(line.left(eq).trimmed().toUpper(), line.mid(eq + 1).trimmed());
    }
    return cnf;
}

} // namespace

GameInfo parsePs2Iso(const QString &path)
{
    GameInfo out;
    out.path = path;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out.error = QStringLiteral("cannot open file");
        return out;
    }
    out.size = f.size();

    QString vol;
    quint32 rootLba = 0, rootSize = 0;
    QString isoError;
    if (!Iso9660::open(f, &vol, &rootLba, &rootSize, &isoError)) {
        out.error = isoError;
        return out;
    }
    const QList<IsoEntry> ents = Iso9660::walk(f, rootLba, rootSize);
    if (ents.isEmpty()) {
        out.error = QStringLiteral("empty ISO");
        return out;
    }
    bool hasSystemCnf = false;
    for (const IsoEntry &e : ents) {
        if (e.name.compare(QLatin1String("SYSTEM.CNF"), Qt::CaseInsensitive) == 0)
            hasSystemCnf = true;
    }
    if (!hasSystemCnf) {
        out.error = QStringLiteral("not a PS2 disc ISO");
        return out;
    }

    QVariantMap cnf;
    for (const IsoEntry &e : ents) {
        if (e.name.compare(QLatin1String("SYSTEM.CNF"), Qt::CaseInsensitive) == 0
            && e.size > 0 && e.size < 100000
            && f.seek(qint64(e.offset))) {
            cnf = parseSystemCnf(f.read(int(e.size)));
            break;
        }
    }
    const QString boot = cnf.value(QStringLiteral("BOOT2")).toString().trimmed();
    if (!boot.startsWith(QLatin1String("cdrom0:"), Qt::CaseInsensitive)) {
        out.error = QStringLiteral("not a PS2 disc ISO");
        return out;
    }

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
    const QString title = ps2TitleFromFileName(path, vol);
    const QString tid = serial.isEmpty() ? QFileInfo(path).completeBaseName() : serial;

    out.ok = true;
    out.platform = Platform::PS2;
    out.title = title.isEmpty() ? tid : title;
    out.meta = cnf;
    out.ps2Serial = serial;
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS2 ISO (disc)"));
    out.addRow(QStringLiteral("Title ID"), serial.isEmpty() ? QStringLiteral("-") : serial);
    out.addRow(QStringLiteral("Region"), ps2Region(serial, vmode));
    out.addRow(QStringLiteral("Version"),
               ver.isEmpty() || ver == QLatin1String("-")
                   ? QStringLiteral("-")
                   : QStringLiteral("v") + ver);
    out.addRow(QStringLiteral("VMODE"), vmode.isEmpty() ? QStringLiteral("-") : vmode);
    out.addRow(QStringLiteral("Boot"), bootDisp.isEmpty() ? QStringLiteral("-") : bootDisp);
    out.addRow(QStringLiteral("Volume"), vol.isEmpty() ? QStringLiteral("-") : vol);
    out.addRow(QStringLiteral("Size"),
               QStringLiteral("%1 (%2 files)").arg(fmtSize(out.size)).arg(ents.size()));

    for (const IsoEntry &e : ents) {
        Entry en;
        en.id = quint32(out.entries.size());
        en.name = e.name;
        en.size = e.size;
        en.source = Entry::Source::Offset;
        en.offset = qint64(e.offset);
        out.entries.append(en);
    }
    return out;
}

} // namespace psg::ps2
