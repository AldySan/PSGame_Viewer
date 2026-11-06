#include "details.h"

#include "util.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace psg {
namespace {

const struct { const char *key; const char *label; } kCuratedJson[] = {
    {"titleId", "Title ID"},
    {"contentId", "Content ID"},
    {"contentVersion", "Content Version"},
    {"masterVersion", "Master Version"},
    {"conceptId", "Concept ID"},
    {"applicationDrmType", "DRM"},
    {"requiredSystemSoftwareVersion", "Min. System"},
    {"sdkVersion", "SDK"},
};

QString friendlyJsonValue(const QString &key, const QVariant &v)
{
    if (key == QLatin1String("sdkVersion")
        || key == QLatin1String("requiredSystemSoftwareVersion"))
        return fmtFw(v);
    return v.toString();
}

} // namespace

QStringList curatedMetaLines(Platform platform, const QVariantMap &meta)
{
    QStringList lines;
    if (meta.isEmpty())
        return lines;
    if (platform == Platform::PS5) {
        const QVariantMap lp = meta.value(QStringLiteral("localizedParameters")).toMap();
        const QString lang = lp.value(QStringLiteral("defaultLanguage")).toString();
        QString t;
        if (!lang.isEmpty())
            t = lp.value(lang).toMap().value(QStringLiteral("titleName")).toString();
        if (!t.isEmpty())
            lines.append(QStringLiteral("Title = %1").arg(t));
        for (const auto &ck : kCuratedJson) {
            const QString key = QString::fromLatin1(ck.key);
            if (!meta.contains(key))
                continue;
            lines.append(QStringLiteral("%1 = %2")
                             .arg(QString::fromLatin1(ck.label),
                                  friendlyJsonValue(key, meta.value(key))));
        }
        return lines;
    }

    // PS3/PS4 param.sfo: short useful subset in a fixed order
    static const char *order[] = {"TITLE", "TITLE_ID", "CATEGORY",
                                  "CONTENT_ID", "VERSION", "APP_VER",
                                  "SYSTEM_VER", "FORMAT", "PARENTAL_LEVEL",
                                  "RESOLUTION", "SOUND_FORMAT",
                                  "NP_COMMUNICATION_ID", "BOOTABLE"};
    QStringList localized;
    for (auto it = meta.cbegin(); it != meta.cend(); ++it) {
        if (it.key().startsWith(QLatin1String("TITLE_"))
            && it.key().mid(6).toInt() > 0)
            localized.append(it.key());
    }
    bool collapsed = false;
    if (localized.size() > 1) {
        QHash<QString, QStringList> groups;
        for (const QString &k : localized)
            groups[meta.value(k).toString()].append(k);
        if (groups.size() < localized.size()) {
            collapsed = true;
            QList<QStringList> ordered;
            for (const QString &k : localized) {
                const QString val = meta.value(k).toString();
                bool placed = false;
                for (QStringList &g : ordered) {
                    if (meta.value(g.first()).toString() == val) {
                        g.append(k);
                        placed = true;
                        break;
                    }
                }
                if (!placed)
                    ordered.append(QStringList{k});
            }
            for (const QStringList &ks : ordered) {
                const QStringList sorted = ks;
                const QString val = meta.value(sorted.first()).toString();
                if (sorted.size() == 1)
                    lines.append(QStringLiteral("%1 = %2").arg(sorted.first(), val));
                else
                    lines.append(QStringLiteral("%1..%2 (%3 langs) = %4")
                                     .arg(sorted.first(), sorted.last())
                                     .arg(sorted.size()).arg(val));
            }
        }
    }
    for (const char *k : order) {
        const QString key = QString::fromLatin1(k);
        if (!meta.contains(key))
            continue;
        if (key == QLatin1String("TITLE") && collapsed)
            continue; // localized titles already shown above
        QVariant v = meta.value(key);
        if (v.typeId() == QMetaType::QString && v.toString().trimmed().isEmpty())
            continue;
        if (key == QLatin1String("SYSTEM_VER")
            || key == QLatin1String("PS3_SYSTEM_VER"))
            v = fmtFw32(v);
        lines.append(QStringLiteral("%1 = %2").arg(key, v.toString()));
    }
    return lines;
}

QStringList fullMetaLines(const QVariantMap &meta)
{
    QStringList lines;
    QStringList keys = meta.keys();
    keys.sort();
    for (const QString &k : keys) {
        if (k.startsWith(u'_'))
            continue;
        QVariant v = meta.value(k);
        if (k == QLatin1String("SYSTEM_VER") || k == QLatin1String("PS3_SYSTEM_VER"))
            v = fmtFw32(v);
        QString s;
        if (v.typeId() == QMetaType::QVariantMap
            || v.typeId() == QMetaType::QVariantHash) {
            s = QString::fromUtf8(
                QJsonDocument(QJsonObject::fromVariantMap(v.toMap()))
                    .toJson(QJsonDocument::Compact));
            if (s.size() > 300)
                s.truncate(300);
        } else {
            s = v.toString();
        }
        if (s.size() > 200)
            s.truncate(200);
        lines.append(QStringLiteral("%1 = %2").arg(k, s));
    }
    return lines;
}

} // namespace psg
