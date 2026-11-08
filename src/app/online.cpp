#include "online.h"

#include "portable.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QTimer>

namespace psg {
namespace {

const char kBrowserUa[] =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/126.0 Safari/537.36";

// covers/patch pages never come close to this; larger bodies are rejected
constexpr int kMaxBodyBytes = 5000000;

QNetworkRequest makeRequest(const QString &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QLatin1String(kBrowserUa));
    req.setTransferTimeout(15000);
    return req;
}

} // namespace

OnlineService::OnlineService(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString OnlineService::coversCacheDir()
{
    // portable: the disk cache lives next to the executable
    const QString dir = portable::coversCacheDir();
    QDir().mkpath(dir);
    return dir;
}

void OnlineService::requestAbort()
{
    m_abort.store(true, std::memory_order_relaxed);
    QMutexLocker lock(&m_inFlightMutex);
    // queued to the worker thread that owns the reply - no cross-thread
    // QNetworkReply method call (abort on a finished reply is a no-op)
    if (m_inFlight) {
        QMetaObject::invokeMethod(m_inFlight, &QNetworkReply::abort,
                                  Qt::QueuedConnection);
    }
}

QByteArray OnlineService::getBlocking(QNetworkReply *reply, int timeoutMs)
{
    QNetworkReply *prevInFlight = nullptr;
    {
        QMutexLocker lock(&m_inFlightMutex);
        // loop.exec() below can deliver another queued lookup (reentry):
        // remember the outer reply and restore it on exit instead of
        // nulling it, so requestAbort() can still abort the outer one
        prevInFlight = m_inFlight;
        m_inFlight = reply;
    }
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    QByteArray out;
    if (!reply->isFinished()) {
        reply->abort();
    } else if (reply->error() == QNetworkReply::NoError
               // an oversized body would be silently truncated by the
               // capped read below and fail to decode - reject it instead
               && reply->size() <= kMaxBodyBytes) {
        out = reply->read(int(kMaxBodyBytes));
    }
    {
        QMutexLocker lock(&m_inFlightMutex);
        m_inFlight = prevInFlight;
    }
    return out;
}

QByteArray OnlineService::get(const QString &url, int timeoutMs)
{
    if (m_abort.load(std::memory_order_relaxed))
        return QByteArray();
    QNetworkReply *reply = m_nam->get(makeRequest(url));
    const QByteArray out = getBlocking(reply, timeoutMs);
    reply->deleteLater(); // replies are caller-owned; never parented
    return out;
}

QByteArray OnlineService::post(const QString &url, const QByteArray &body,
                               int timeoutMs)
{
    if (m_abort.load(std::memory_order_relaxed))
        return QByteArray();
    QNetworkRequest req = makeRequest(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(req, body);
    const QByteArray out = getBlocking(reply, timeoutMs);
    reply->deleteLater();
    return out;
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
            const QByteArray raw = f.read(kMaxBodyBytes);
            if (raw.left(2) == QByteArrayLiteral("\xFF\xD8")
                || raw.left(8) == QByteArrayLiteral("\x89PNG\r\n\x1a\n")) {
                emit coverBytesReady(key, raw);
                return;
            }
        }
    }

    QList<QString> urls;
    if (key.contains(u'-') && key.count(u'-') == 1 && key.at(4) == QChar(u'-')) {
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
        if (m_abort.load(std::memory_order_relaxed))
            break;
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
    // The per-title key is embedded in the game page. Both sites used to
    // serve it as data-key="…"; orbispatches moved it into the #dynpatch
    // loader as data-loadparams="{ 'titleid': …, 'key': … }" (JSON-ish,
    // single or double quotes). Accept every shape so a lookup survives
    // either template.
    QString keyHex;
    static const QRegularExpression keyPatterns[] = {
        QRegularExpression(
            QStringLiteral("data-key=\"([0-9a-f]{64})\"")),
        QRegularExpression(
            QStringLiteral("['\"]key['\"]\\s*:\\s*'([0-9a-f]{64})'")),
        QRegularExpression(
            QStringLiteral("\"key\"\\s*:\\s*\"([0-9a-f]{64})\"")),
    };
    for (const QRegularExpression &re : keyPatterns) {
        const auto m = re.match(html);
        if (m.hasMatch()) {
            keyHex = m.captured(1);
            break;
        }
    }
    if (keyHex.isEmpty()) {
        emit patchReady(tid, QString(), 0, QString());
        return;
    }
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

} // namespace psg
