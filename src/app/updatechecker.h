#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class QNetworkAccessManager;

namespace psg {

// Checks the project's GitHub releases for a newer version. The endpoint
// can be overridden with PSGV_UPDATE_API for testing; version comparison
// lives in psgcore (parseVersionTag/isVersionNewer) so it is unit tested.
class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(const QString &currentVersion,
                            QObject *parent = nullptr);

    QString currentVersion() const { return m_currentVersion; }

    // Fire a check; emits checkFinished exactly once per call.
    Q_INVOKABLE void checkNow();

signals:
    // result map:
    //   state    "update-available" | "up-to-date" | "no-release" | "error"
    //   current  the running version
    //   latest   the latest tag (when a release was found)
    //   name     release title
    //   notes    release body
    //   pageUrl  release page (opens in the browser)
    //   assets   [{name, url, size}] release attachments
    //   error    reason (state "error")
    void checkFinished(const QVariantMap &result);

private:
    void emitResult(const QVariantMap &result);

    QString m_currentVersion;
    QNetworkAccessManager *m_nam = nullptr;
    int m_pending = 0;
};

} // namespace psg
