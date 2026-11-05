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
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool onlineEnabled READ onlineEnabled WRITE setOnlineEnabled
                   NOTIFY onlineEnabledChanged)
    Q_PROPERTY(bool checkUpdatesOnStartup READ checkUpdatesOnStartup
                   WRITE setCheckUpdatesOnStartup
                   NOTIFY checkUpdatesOnStartupChanged)
    // rename dialog persistence: NOT constant, the dialogs re-read these
    // after every save, so they must notify
    Q_PROPERTY(QVariantMap renameInclude READ renameInclude
                   NOTIFY renamePrefsChanged)
    Q_PROPERTY(QString renamePreset READ renamePreset
                   NOTIFY renamePrefsChanged)
    Q_PROPERTY(QString renamePattern READ renamePattern
                   NOTIFY renamePrefsChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &lang);

    // "dark" (default) or "light"
    QString theme() const;
    void setTheme(const QString &theme);

    bool onlineEnabled() const;
    void setOnlineEnabled(bool enabled);

    // background check when the app starts (default on)
    bool checkUpdatesOnStartup() const;
    void setCheckUpdatesOnStartup(bool enabled);

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
    void themeChanged();
    void onlineEnabledChanged();
    void checkUpdatesOnStartupChanged();
    void renamePrefsChanged();

private:
    QSettings *m_s;
};

} // namespace psg
