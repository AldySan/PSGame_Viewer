#pragma once

#include <QList>
#include <QObject>
#include <QVariantMap>

class QSettings;
class QTranslator;

namespace psg {

// QSettings wrapper for the small app-level preferences (language,
// online lookups, rename dialog choices). The library database lives
// in its own SQLite store.
//
// The settings object also owns the interface translators: saving a
// new language swaps them right away, so the change is live without a
// restart (Qt sends LanguageChange and the QML engine re-evaluates
// every qsTr() binding).
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
    ~AppSettings() override;

    QString language() const;
    void setLanguage(const QString &lang);

    // true while a translation file is installed (used by the tests to
    // confirm a language switch actually loaded a translator)
    bool translationActive() const { return !m_translators.isEmpty(); }

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
    // (re)install the translators matching the saved language
    void applyLanguage();
    // uninstall this instance's translators
    void removeTranslators();

    QSettings *m_s;
    QList<QTranslator *> m_translators;
};

} // namespace psg
