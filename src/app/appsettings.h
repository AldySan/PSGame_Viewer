#pragma once

#include <QObject>
#include <QVariantMap>

class QSettings;

namespace psg {

// QSettings wrapper for the small app-level preferences (language,
// online lookups, rename dialog choices). The library database lives
// in its own SQLite store.
class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage
                   NOTIFY languageChanged)
    Q_PROPERTY(bool onlineEnabled READ onlineEnabled WRITE setOnlineEnabled
                   NOTIFY onlineEnabledChanged)
    Q_PROPERTY(QString renamePreset READ renamePreset CONSTANT)
    Q_PROPERTY(QString renamePattern READ renamePattern CONSTANT)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &lang);
    bool onlineEnabled() const;
    void setOnlineEnabled(bool enabled);

    // Rename dialog persistence (pattern + include ticks)
    QVariantMap renameInclude() const;
    QString renamePreset() const;
    QString renamePattern() const;
    Q_INVOKABLE void saveRenamePrefs(const QVariantMap &include,
                                     const QString &preset,
                                     const QString &pattern);

    QVariant value(const QString &key, const QVariant &def = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

signals:
    void languageChanged();
    void onlineEnabledChanged();

private:
    QSettings *m_s;
};

} // namespace psg
