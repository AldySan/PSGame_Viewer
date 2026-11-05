#include "appsettings.h"

#include "portable.h"

#include <QSettings>

namespace psg {

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_s(new QSettings(portable::iniPath(), QSettings::IniFormat, this))
{
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
    emit languageChanged();
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
