#include "appsettings.h"

#include "portable.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace psg {

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_s(new QSettings(portable::iniPath(), QSettings::IniFormat, this))
{
    applyLanguage();
}

AppSettings::~AppSettings()
{
    // uninstall what this instance installed: two settings objects can
    // be alive at once (tests), and a stale catalog would keep
    // translating after its owner is gone
    removeTranslators();
}

void AppSettings::removeTranslators()
{
    QCoreApplication *app = QCoreApplication::instance();
    if (!app)
        return;
    for (QTranslator *t : std::as_const(m_translators)) {
        app->removeTranslator(t);
        delete t;
    }
    m_translators.clear();
}

void AppSettings::applyLanguage()
{
    QCoreApplication *app = QCoreApplication::instance();
    if (!app)
        return;
    removeTranslators();

    const QString lang = language();
    const QStringList locales = lang == QLatin1String("system")
        ? QLocale::system().uiLanguages()
        : QStringList{lang};
    for (const QString &loc : locales) {
        auto *t = new QTranslator(app);
        if (t->load(QStringLiteral("psgameviewer_") + QLocale(loc).name(),
                    QStringLiteral(":/i18n"))) {
            app->installTranslator(t);
            m_translators.append(t);
        } else {
            t->deleteLater();
        }
    }
}

QString AppSettings::language() const
{
    return m_s->value(QStringLiteral("language"), QStringLiteral("system"))
        .toString();
}

void AppSettings::setLanguage(const QString &lang)
{
    if (lang == language())
        return;
    m_s->setValue(QStringLiteral("language"), lang);
    // swap the translators before the notify: the QML bindings that
    // react to languageChanged re-evaluate against the NEW catalog
    applyLanguage();
    emit languageChanged();
}

QString AppSettings::theme() const
{
    const QString t = m_s->value(QStringLiteral("theme"),
                                QStringLiteral("dark")).toString();
    return t == QLatin1String("light") ? QStringLiteral("light")
                                       : QStringLiteral("dark");
}

void AppSettings::setTheme(const QString &theme)
{
    const QString t = theme == QLatin1String("light")
        ? QStringLiteral("light") : QStringLiteral("dark");
    if (t == AppSettings::theme())
        return;
    m_s->setValue(QStringLiteral("theme"), t);
    emit themeChanged();
}

bool AppSettings::onlineEnabled() const
{
    return m_s->value(QStringLiteral("onlineEnabled"), true).toBool();
}

bool AppSettings::checkUpdatesOnStartup() const
{
    return m_s->value(QStringLiteral("checkUpdatesOnStartup"), true)
        .toBool();
}

void AppSettings::setCheckUpdatesOnStartup(bool enabled)
{
    if (enabled == checkUpdatesOnStartup())
        return;
    m_s->setValue(QStringLiteral("checkUpdatesOnStartup"), enabled);
    emit checkUpdatesOnStartupChanged();
}

void AppSettings::setOnlineEnabled(bool enabled)
{
    if (enabled == onlineEnabled())
        return;
    m_s->setValue(QStringLiteral("onlineEnabled"), enabled);
    emit onlineEnabledChanged();
}

QVariantMap AppSettings::renameInclude() const
{
    QVariantMap def;
    def.insert(QStringLiteral("title"), true);
    def.insert(QStringLiteral("tid"), true);
    def.insert(QStringLiteral("ver"), true);
    def.insert(QStringLiteral("region"), true);
    const QVariantMap saved =
        m_s->value(QStringLiteral("renameInclude")).toMap();
    for (auto it = saved.cbegin(); it != saved.cend(); ++it)
        def.insert(it.key(), it.value().toBool());
    return def;
}

QString AppSettings::renamePreset() const
{
    return m_s->value(QStringLiteral("renamePreset"), QStringLiteral("clean"))
        .toString();
}

QString AppSettings::renamePattern() const
{
    return m_s->value(QStringLiteral("renamePattern")).toString();
}

void AppSettings::saveRenamePrefs(const QVariantMap &include,
                                  const QString &preset, const QString &pattern)
{
    if (!include.isEmpty())
        m_s->setValue(QStringLiteral("renameInclude"), include);
    if (!preset.isEmpty())
        m_s->setValue(QStringLiteral("renamePreset"), preset);
    m_s->setValue(QStringLiteral("renamePattern"), pattern);
    emit renamePrefsChanged();
}

QVariant AppSettings::value(const QString &key, const QVariant &def) const
{
    return m_s->value(key, def);
}

void AppSettings::setValue(const QString &key, const QVariant &value)
{
    m_s->setValue(key, value);
}

} // namespace psg
