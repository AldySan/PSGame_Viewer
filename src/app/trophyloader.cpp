#include "trophyloader.h"

#include "core/extract.h"
#include "core/trophies.h"

namespace psg {

void TrophyLoader::load(quint64 token, const QString &containerPath,
                        const QList<Entry> &entries, const QVariantMap &meta)
{
    using trophies::bruteforceNpid;
    using trophies::carveTrpIcons;
    using trophies::decryptEsfm;
    using trophies::parseTrp;
    using trophies::parseTrophyXml;
    using trophies::ucpIconsSorted;
    using trophies::ucpTrophies;

    // NOTE: m_abort is deliberately NOT cleared here. requestAbort() is
    // only called from the controller destructor; if a load() was queued
    // before shutdown, resetting the flag here would resurrect the job
    // and block ~ViewerController() on m_trophyThread->wait() for the
    // full pack read (seconds of apparent hang on close).

    // a newer open already published its token: this queued job is for
    // a superseded file - drop it before any I/O (a loader driven by
    // hand-picked tokens, like the unit tests, always passes: nothing
    // newer was ever published)
    if (token < m_latestToken.load(std::memory_order_relaxed))
        return;

    for (const Entry &e : entries) {
        if (m_abort.load(std::memory_order_relaxed))
            return; // shutting down: the controller is going away
        const QString lower = e.name.toLower();
        if (!lower.endsWith(QLatin1String(".trp"))
            && !lower.endsWith(QLatin1String(".ucp")))
            continue;
        if (e.size == 0 || e.size >= 300000000)
            continue;
        const QByteArray data = readEntryBytes(e, containerPath, 300000000);
        if (data.isEmpty())
            continue;

        if (lower.endsWith(QLatin1String(".ucp"))) {
            const trophies::UcpResult res = ucpTrophies(data);
            // carved PNGs in display order: the i-th trophy gets the i-th
            // icon (same convention as the TRP branch)
            const QList<QByteArray> iconBytes = ucpIconsSorted(res);
            TrophyPack pack;
            for (int i = 0; i < res.trophies.size(); ++i) {
                const trophies::Trophy &t = res.trophies.at(i);
                TrophyListModel::Item it;
                it.id = t.id;
                it.name = t.name;
                it.detail = t.detail;
                it.grade = t.grade;
                it.hidden = t.hidden;
                it.iconIndex = i < iconBytes.size() ? i : -1;
                pack.items.append(it);
            }
            pack.icons = iconBytes;
            emit loaded(token, pack);
            return;
        }

        const auto inner = parseTrp(data);
        QByteArray esfm;
        for (const auto &f : inner) {
            if (f.name.compare(QLatin1String("TROPCONF.SFM"),
                               Qt::CaseInsensitive) == 0) {
                esfm = data.mid(int(f.offset), int(f.size));
                break;
            }
        }
        if (esfm.isEmpty())
            continue;
        // PS4 packs carry the communication id in PARAM.SFO
        // (NP_COMMUNICATION_ID), PS5 images carry it in param.json
        // (npCommunicationId) — accept both shapes.
        QString npid = meta.value(
            QStringLiteral("NP_COMMUNICATION_ID")).toString();
        if (npid.isEmpty())
            npid = meta.value(QStringLiteral("npCommunicationId")).toString();
        QByteArray xml = npid.isEmpty() ? QByteArray()
                                        : decryptEsfm(esfm, npid);
        if (xml.isEmpty()) {
            const auto hit = bruteforceNpid(
                esfm, 0, 100000,
                [this]() {
                    return m_abort.load(std::memory_order_relaxed);
                });
            if (!hit.first.isEmpty())
                xml = hit.second;
        }
        if (xml.isEmpty())
            continue;

        // carved icons: 240x240 trophy icons in trophy-id order
        QList<QByteArray> iconBytes;
        for (const auto &c : carveTrpIcons(data)) {
            if (c.width == 240 && c.height == 240)
                iconBytes.append(data.mid(int(c.offset), int(c.size)));
        }

        TrophyPack pack;
        for (const trophies::Trophy &t : parseTrophyXml(xml)) {
            TrophyListModel::Item it;
            it.id = t.id;
            it.name = t.name;
            it.detail = t.detail;
            it.grade = t.grade;
            it.hidden = t.hidden;
            bool okId = false;
            const int idNum = t.id.toInt(&okId);
            it.iconIndex = (okId && idNum < iconBytes.size()) ? idNum : -1;
            pack.items.append(it);
        }
        pack.icons = iconBytes;
        emit loaded(token, pack);
        return;
    }

    // no readable pack in this file: report the empty result so the
    // controller can drop its loading state (otherwise the Trophies tab
    // would show "Loading…" forever)
    emit loaded(token, TrophyPack());
}

} // namespace psg
