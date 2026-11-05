#include "online.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace psg {
namespace {

const char kBrowserUa[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/126.0 Safari/537.36";

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(kBrowserUa));
    req.setTransferTimeout(15000);
    return req;
}

QString storeLocale(const QString &cid)
{
    const QString prefix = cid.left(2).toUpper();
    static const QHash<QString, QString> map = {
        {QStringLiteral("UP"), QStringLiteral("en-us")},
        {QStringLiteral("EP"), QStringLiteral("en-gb")},
        {QStringLiteral("JP"), QStringLiteral("ja-jp")},
        {QStringLiteral("HP"), QStringLiteral("en-hk")},
    };
    return map.value(prefix, QStringLiteral("en-us"));
}

} // namespace

OnlineService::OnlineService(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString OnlineService::coversCacheDir()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    const QString dir = base + QStringLiteral("/covers");
    QDir().mkpath(dir);
    return dir;
}

QByteArray OnlineService::getBlocking(QNetworkReply *reply, int timeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        return QByteArray();
    }
    if (reply->error() != QNetworkReply::NoError)
        return QByteArray();
    return reply->read(5000000);
}

QByteArray OnlineService::get(const QString &url, int timeoutMs)
{
    QNetworkReply *reply = m_nam->get(makeRequest(QUrl(url)));
    return getBlocking(reply, timeoutMs);
}

QByteArray OnlineService::post(const QString &url, const QByteArray &body,
                               int timeoutMs)
{
    QNetworkRequest req = makeRequest(QUrl(url));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(req, body);
    return getBlocking(reply, timeoutMs);
}

void OnlineService::fetchStoreCover(const QString &contentId)
{
    if (contentId.isEmpty() || !contentId.contains(u'-')) {
        emit storeCoverReady(contentId, QString(), QString(), QString());
        return;
    }
    if (m_storeCache.contains(contentId)) {
        const QString cached = m_storeCache.value(contentId);
        const QStringList parts = cached.split(u'\x1f');
        emit storeCoverReady(contentId, parts.value(0), parts.value(1),
                             parts.value(2));
        return;
    }
    const QString url = QStringLiteral(
        "https://store.playstation.com/%1/product/%2")
                            .arg(storeLocale(contentId), contentId);
    const QString html = QString::fromUtf8(get(url));

    QString cover;
    static const QRegularExpression re1(
        QStringLiteral("\"role\":\"([A-Z_0-9]+)\"[^}]{0,400}?\"url\":\"(https://[^\"]+)\""));
    auto it = re1.globalMatch(html);
    QHash<QString, QString> arts;
    while (it.hasNext()) {
        const auto m = it.next();
        if (!arts.contains(m.captured(1)))
            arts.insert(m.captured(1), m.captured(2));
    }
    static const QRegularExpression re2(
        QStringLiteral("\"url\":\"(https://[^\"]+)\"[^}]{0,400}?\"role\":\"([A-Z_0-9]+)\""));
    auto it2 = re2.globalMatch(html);
    while (it2.hasNext()) {
        const auto m = it2.next();
        if (!arts.contains(m.captured(2)))
            arts.insert(m.captured(2), m.captured(1));
    }
    cover = arts.value(QStringLiteral("MASTER"));
    if (cover.isEmpty())
        cover = arts.value(QStringLiteral("GAMEHUB_COVER_ART"));

    QString name;
    static const QRegularExpression nameRe(
        QStringLiteral("\"__typename\":\"Concept\",\"name\":\"([^\"]+)\""));
    const auto nm = nameRe.match(html);
    if (nm.hasMatch())
        name = nm.captured(1);
    QString release;
    static const QRegularExpression relRe(
        QStringLiteral("\"releaseDate\":\"(\\d{4}-\\d{2}-\\d{2})"));
    const auto rm = relRe.match(html);
    if (rm.hasMatch())
        release = rm.captured(1);

    m_storeCache.insert(contentId,
                        QStringList{name, cover, release}.join(u'\x1f'));
    emit storeCoverReady(contentId, name, cover, release);
}

void OnlineService::fetchCoverBytes(const QString &key, const QString &tid,
                                    const QString &region)
{
    // disk cache first, so covers survive offline
    const QString dir = coversCacheDir();
    for (const QString &ext : {QStringLiteral(".jpg"), QStringLiteral(".png")}) {
        const QString path = dir + u'/' + key + ext;
        QFile f(path);
        if (f.size() > 1024 && f.open(QIODevice::ReadOnly)) {
            const QByteArray raw = f.read(5000000);
            if (raw.left(2) == QByteArrayLiteral("\xFF\xD8")
                || raw.left(8) == QByteArrayLiteral("\x89PNG\r\n\x1a\n")) {
                emit coverBytesReady(key, raw);
                return;
            }
        }
    }

    QList<QString> urls;
    if (key.contains(u'-') && key.count(u'-') == 1 && key.at(4) == u'-') {
        // PS2 serial "SCUS-97399": xlenore flat then 3d
        urls.append(QStringLiteral(
            "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/default/%1.jpg").arg(key));
        urls.append(QStringLiteral(
            "https://raw.githubusercontent.com/xlenore/ps2-covers/main/covers/3d/%1.png").arg(key));
    } else {
        // PS3 Title ID: GameTDB box-front by region folder
        static const QHash<QString, QString> regionMap = {
            {QStringLiteral("AMERICAS"), QStringLiteral("US")},
            {QStringLiteral("EUROPE"), QStringLiteral("EN")},
            {QStringLiteral("JAPAN"), QStringLiteral("JA")},
            {QStringLiteral("ASIA"), QStringLiteral("JA")},
            {QStringLiteral("KOREA"), QStringLiteral("KO")},
            {QStringLiteral("CHINA"), QStringLiteral("EN")},
        };
        QStringList order;
        const QString rg = regionMap.value(region.toUpper());
        if (!rg.isEmpty())
            order.append(rg);
        order << QStringLiteral("US") << QStringLiteral("EN")
              << QStringLiteral("JA");
        QStringList seen;
        for (const QString &r : order) {
            if (!seen.contains(r)) {
                seen.append(r);
                urls.append(QStringLiteral("https://art.gametdb.com/ps3/cover/%1/%2.jpg").arg(r, key));
            }
        }
    }
    for (const QString &url : urls) {
        const QByteArray raw = get(url);
        if (raw.isEmpty())
            continue;
        if (raw.left(2) != QByteArrayLiteral("\xFF\xD8")
            && raw.left(8) != QByteArrayLiteral("\x89PNG\r\n\x1a\n"))
            continue;
        // cache to disk
        const QString ext = raw.left(8) == QByteArrayLiteral("\x89PNG\r\n\x1a\n")
            ? QStringLiteral(".png")
            : QStringLiteral(".jpg");
        QFile out(coversCacheDir() + u'/' + key + ext);
        if (out.open(QIODevice::WriteOnly))
            out.write(raw);
        emit coverBytesReady(key, raw);
        return;
    }
    emit coverBytesReady(key, QByteArray());
}

void OnlineService::fetchLatestPatch(const QString &titleId)
{
    if (titleId.isEmpty()) {
        emit patchReady(titleId, QString(), 0, QString());
        return;
    }
    const QString tid = titleId.toUpper();
    if (m_patchCache.contains(tid)) {
        const QStringList parts = m_patchCache.value(tid).split(u'\x1f');
        emit patchReady(tid, parts.value(0), parts.value(1).toInt(),
                        parts.value(2));
        return;
    }
    static const QHash<QString, QString> hosts = {
        {QStringLiteral("PPSA"), QStringLiteral("https://prosperopatches.com")},
        {QStringLiteral("CUSA"), QStringLiteral("https://orbispatches.com")},
    };
    const QString host = hosts.value(tid.left(4));
    if (host.isEmpty()) {
        emit patchReady(tid, QString(), 0, QString());
        return;
    }
    const QString html = QString::fromUtf8(get(host + u'/' + tid));
    QString keyHex;
    static const QRegularExpression re(
        QStringLiteral("data-key=\"([0-9a-f]{64})\""));
    const auto m = re.match(html);
    if (!m.hasMatch()) {
        emit patchReady(tid, QString(), 0, QString());
        return;
    }
    keyHex = m.captured(1);
    const QByteArray body = QStringLiteral(
        "{\"titleid\":\"%1\",\"key\":\"%2\"}").arg(tid, keyHex).toUtf8();
    const QByteArray resp = post(host + QStringLiteral("/api/internal/loadpatches"),
                                 body);
    if (resp.isEmpty()) {
        emit patchReady(tid, QString(), 0, QString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("success")).toBool()) {
        emit patchReady(tid, QString(), 0, QString());
        return;
    }
    const QJsonArray patches = doc.object().value(QStringLiteral("patches")).toArray();
    QString latest;
    QString date;
    for (const QJsonValue &v : patches) {
        if (v.toObject().value(QStringLiteral("is_latest")).toBool()) {
            latest = v.toObject().value(QStringLiteral("content_ver")).toString();
            if (latest.isEmpty())
                latest = v.toObject().value(QStringLiteral("version")).toString();
            date = v.toObject().value(QStringLiteral("import_date")).toString();
            if (date.isEmpty())
                date = v.toObject().value(QStringLiteral("creation_date")).toString();
            break;
        }
    }
    m_patchCache.insert(tid,
                        QStringList{latest, QString::number(patches.size()), date}
                            .join(u'\x1f'));
    emit patchReady(tid, latest, patches.size(), date);
}

QString OnlineService::latestPatchBlocking(const QString &titleId)
{
    QString out;
    connect(this, &OnlineService::patchReady, this,
            [&out](const QString &, const QString &latest, int, const QString &) {
                out = latest;
            });
    fetchLatestPatch(titleId);
    return out;
}

} // namespace psg
