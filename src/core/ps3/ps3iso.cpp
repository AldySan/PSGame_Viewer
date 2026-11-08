#include "ps3iso.h"

#include "../iso9660.h"
#include "../sfo.h"
#include "../util.h"

#include <QFile>
#include <QFileInfo>

namespace psg::ps3 {

GameInfo parsePs3Iso(const QString &path)
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

    bool hasPs3GameSfo = false, hasDiscSfb = false, hasPs3GameDir = false;
    for (const IsoEntry &e : ents) {
        const QString u = e.name.toUpper();
        if (u == QLatin1String("PS3_GAME/PARAM.SFO"))
            hasPs3GameSfo = true;
        else if (u == QLatin1String("PS3_DISC.SFB"))
            hasDiscSfb = true;
        else if (u == QLatin1String("PS3_GAME"))
            hasPs3GameDir = true;
    }
    if (!hasPs3GameSfo && !hasDiscSfb && !hasPs3GameDir) {
        out.error = QStringLiteral("not a PS3 disc ISO");
        return out;
    }

    QVariantMap sfo;
    for (const IsoEntry &e : ents) {
        if (e.name.compare(QLatin1String("PS3_GAME/PARAM.SFO"), Qt::CaseInsensitive) == 0
            && e.size > 0 && e.size < 1000000
            && f.seek(qint64(e.offset))) {
            const QByteArray raw = f.read(int(e.size));
            if (raw.left(4) == QByteArrayLiteral("\x00PSF"))
                sfo = parseSfo(raw);
            break;
        }
    }

    const QString title = sfo.value(QStringLiteral("TITLE")).toString();
    QString tid = sfo.value(QStringLiteral("TITLE_ID")).toString();
    if (tid.isEmpty()) {
        tid = !vol.isEmpty() ? vol : QFileInfo(path).completeBaseName();
    }
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

    QString update;
    for (const IsoEntry &e : ents) {
        if (e.name.startsWith(QLatin1String("PS3_UPDATE/"), Qt::CaseInsensitive)
            && e.size > 0) {
            update = QStringLiteral("%1 (%2)")
                         .arg(QFileInfo(e.name).fileName(), fmtSize(e.size));
            break;
        }
    }

    out.ok = true;
    out.platform = Platform::PS3;
    out.title = title.isEmpty() ? tid : title;
    out.meta = sfo;
    out.iconEntry = QStringLiteral("icon0.png");
    out.addRow(QStringLiteral("Platform"), QStringLiteral("PS3 ISO (disc)"));
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
    out.addRow(QStringLiteral("App Ver"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : valueOrDash(sfo, QStringLiteral("APP_VER")));
    out.addRow(QStringLiteral("Min. System"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : fmtFw32(sfo.value(QStringLiteral("PS3_SYSTEM_VER"))));
    out.addRow(QStringLiteral("Parental Level"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : valueOrDash(sfo, QStringLiteral("PARENTAL_LEVEL")));
    out.addRow(QStringLiteral("Resolution"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : valueOrDash(sfo, QStringLiteral("RESOLUTION")));
    out.addRow(QStringLiteral("Sound Format"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : valueOrDash(sfo, QStringLiteral("SOUND_FORMAT")));
    out.addRow(QStringLiteral("Trophy ID"),
               sfo.isEmpty() ? QStringLiteral("-")
                             : valueOrDash(sfo, QStringLiteral("NP_COMMUNICATION_ID")));
    out.addRow(QStringLiteral("Update"), update.isEmpty() ? QStringLiteral("-") : update);
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
        if (en.name.compare(QLatin1String("PS3_GAME/ICON0.PNG"), Qt::CaseInsensitive) == 0)
            out.iconEntry = en.name;
    }
    return out;
}

} // namespace psg::ps3
