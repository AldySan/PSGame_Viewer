#include "trophyloader.h"

#include "core/extract.h"
#include "core/trophies.h"

namespace psg {

namespace {

// carved trophy icons of the wanted size class: 240x240 on PS4/PS5,
// 128x128 on PS3 — same carving, different filter
QList<QByteArray> iconsOfSize(const QList<trophies::CarvedIcon> &carved,
                              const QByteArray &data, int w, int h)
{
    QList<QByteArray> out;
    for (const auto &c : carved) {
        if (c.width == w && c.height == h)
            out.append(data.mid(int(c.offset), int(c.size)));
    }
    return out;
}

} // namespace

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
        // PS3 packs carry TROPCONF.SFM as PLAIN XML (retail included —
        // only the PS4 TROP.ESFM variant is AES-wrapped); when both
        // exist the plain PS3 conf wins.
        QByteArray conf;
        bool encrypted = false;
        for (const auto &f : inner) {
            if (f.name.compare(QLatin1String("TROPCONF.SFM"),
                               Qt::CaseInsensitive) == 0) {
                conf = data.mid(int(f.offset), int(f.size));
                break;
            }
        }
        if (conf.isEmpty()) {
            for (const auto &f : inner) {
                if (f.name.compare(QLatin1String("TROP.ESFM"),
                                   Qt::CaseInsensitive) == 0) {
                    conf = data.mid(int(f.offset), int(f.size));
                    encrypted = true;
                    break;
                }
            }
        }
        if (conf.size() < 16)
            continue;
        QByteArray xml;
        if (!encrypted && trophies::looksLikeTrophyXml(conf)) {
            xml = conf;
        } else {
            // PS4 packs carry the communication id in PARAM.SFO
            // (NP_COMMUNICATION_ID), PS5 images carry it in param.json
            // (npCommunicationId), PS3 folders/PKGs spell it out in the
            // TROPDIR/<NPWRxxxxx_00>/ entry path — accept all shapes.
            QString npid = meta.value(
                QStringLiteral("NP_COMMUNICATION_ID")).toString();
            if (npid.isEmpty())
                npid = meta.value(QStringLiteral("npCommunicationId")).toString();
            if (npid.isEmpty())
                npid = trophies::npCommIdFromPath(e.name);
            if (!npid.isEmpty())
                xml = decryptEsfm(conf, npid);
            if (xml.isEmpty()) {
                const auto hit = bruteforceNpid(
                    conf, 0, 100000,
                    [this]() {
                        return m_abort.load(std::memory_order_relaxed);
                    });
                if (!hit.first.isEmpty())
                    xml = hit.second;
            }
        }
        if (xml.isEmpty())
            continue;

        // carved icons: 240x240 trophy icons in trophy-id order on
        // PS4/PS5; PS3 packs use 128x128 — fall back when no 240s exist
        const QList<trophies::CarvedIcon> carved = carveTrpIcons(data);
        QList<QByteArray> iconBytes = iconsOfSize(carved, data, 240, 240);
        if (iconBytes.isEmpty())
            iconBytes = iconsOfSize(carved, data, 128, 128);

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
