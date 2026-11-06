#include "updatechecker.h"

#include "core/util.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace psg {
namespace {

constexpr const char *kDefaultApi =
    "https://api.github.com/repos/AldySan/PSGame_Viewer/releases/latest";

} // namespace

UpdateChecker::UpdateChecker(const QString &currentVersion,
                             QObject *parent)
    : QObject(parent)
    , m_currentVersion(currentVersion)
    , m_nam(new QNetworkAccessManager(this))
{
}

void UpdateChecker::emitResult(const QVariantMap &result)
{
    emit checkFinished(result);
}

void UpdateChecker::checkNow()
{
    QNetworkRequest req(QUrl(qEnvironmentVariable("PSGV_UPDATE_API")
                                 .isEmpty()
                                 ? QString::fromLatin1(kDefaultApi)
                                 : qEnvironmentVariable("PSGV_UPDATE_API")));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QLatin1String("PSGame-Viewer updater"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    req.setTransferTimeout(15000);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emitResult({
                {QStringLiteral("state"), QStringLiteral("error")},
                {QStringLiteral("current"), m_currentVersion},
                {QStringLiteral("error"), reply->errorString()},
            });
            return;
        }
        const QJsonDocument doc =
            QJsonDocument::fromJson(reply->read(2000000));
        if (!doc.isObject()) {
            emitResult({
                {QStringLiteral("state"), QStringLiteral("error")},
                {QStringLiteral("current"), m_currentVersion},
                {QStringLiteral("error"),
                 QStringLiteral("invalid release response")},
            });
            return;
        }
        const QJsonObject obj = doc.object();
        const QString tag = obj.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            emitResult({
                {QStringLiteral("state"), QStringLiteral("no-release")},
                {QStringLiteral("current"), m_currentVersion},
            });
            return;
        }

        QVariantList assets;
        const QJsonArray arr = obj.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject a = v.toObject();
            const QString url =
                a.value(QStringLiteral("browser_download_url")).toString();
            if (url.isEmpty())
                continue;
            assets.append(QVariantMap{
                {QStringLiteral("name"),
                 a.value(QStringLiteral("name")).toString()},
                {QStringLiteral("url"), url},
                {QStringLiteral("size"),
                 a.value(QStringLiteral("size")).toDouble()},
            });
        }

        const bool newer = isVersionNewer(tag, m_currentVersion);
        emitResult({
            {QStringLiteral("state"),
             newer ? QStringLiteral("update-available")
                   : QStringLiteral("up-to-date")},
            {QStringLiteral("current"), m_currentVersion},
            {QStringLiteral("latest"), tag},
            {QStringLiteral("name"),
             obj.value(QStringLiteral("name")).toString()},
            {QStringLiteral("notes"),
             obj.value(QStringLiteral("body")).toString()},
            {QStringLiteral("pageUrl"),
             obj.value(QStringLiteral("html_url")).toString()},
            {QStringLiteral("assets"), assets},
        });
    });
}

} // namespace psg
