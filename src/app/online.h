#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace psg {

// Online lookups: PS2/PS3 cover art and the latest patch version for
// PS4/PS5 Title IDs. Everything is cached on disk (covers) or in memory
// (patch results) and fails silent: the UI simply keeps the local icon
// when the network is unavailable.
class OnlineService : public QObject
{
    Q_OBJECT

public:
    explicit OnlineService(QObject *parent = nullptr);

    // cover bytes for a PS2 serial (SCUS-97399) or PS3 Title ID (BCUS98111).
    // invokable so the GUI can run them on the OnlineService thread.
    Q_INVOKABLE void fetchCoverBytes(const QString &key, const QString &tid,
                                    const QString &region);
    // "(latest) N patches" info for a PS4/PS5 Title ID
    Q_INVOKABLE void fetchLatestPatch(const QString &titleId);

    static QString coversCacheDir();

signals:
    void coverBytesReady(const QString &key, const QByteArray &bytes);
    void patchReady(const QString &titleId, const QString &latest,
                    int patchCount, const QString &date);

private:
    QByteArray get(const QString &url, int timeoutMs = 15000);
    QByteArray post(const QString &url, const QByteArray &body,
                    int timeoutMs = 15000);
    QByteArray getBlocking(QNetworkReply *reply, int timeoutMs);

    QNetworkAccessManager *m_nam;
    QHash<QString, QString> m_patchCache;
};

} // namespace psg
