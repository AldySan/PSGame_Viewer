#include "types.h"

namespace psg {

QString platformName(Platform p)
{
    switch (p) {
    case Platform::PS2: return QStringLiteral("PS2");
    case Platform::PS3: return QStringLiteral("PS3");
    case Platform::PS4: return QStringLiteral("PS4");
    case Platform::PS5: return QStringLiteral("PS5");
    default: return QStringLiteral("Unknown");
    }
}

QString GameInfo::row(const QString &label, const QString &fallback) const
{
    for (const auto &r : rows) {
        if (r.label.compare(label, Qt::CaseInsensitive) == 0)
            return r.value;
    }
    return fallback;
}

bool GameInfo::hasRow(const QString &label) const
{
    for (const auto &r : rows) {
        if (r.label.compare(label, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void GameInfo::setRow(const QString &label, const QString &value)
{
    for (auto &r : rows) {
        if (r.label.compare(label, Qt::CaseInsensitive) == 0) {
            r.value = value;
            return;
        }
    }
    rows.append({label, value});
}

void GameInfo::addRow(const QString &label, const QString &value)
{
    rows.append({label, value});
}

QString GameInfo::version() const
{
    QString v = row(QStringLiteral("Version"));
    if (v.isEmpty() || v == QLatin1String("-"))
        v = row(QStringLiteral("Content Ver"));
    return v;
}

} // namespace psg
