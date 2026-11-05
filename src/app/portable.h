#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QString>

namespace psg {

// Portable application layout: every piece of state lives next to the
// executable — the settings file, the library database, the cover cache
// and the batch rename logs. Nothing is written to the registry, $HOME
// or system config dirs, so the whole folder can be carried around on a
// USB stick and the app never needs an installer.
namespace portable {

inline QString baseDir()
{
    return QCoreApplication::applicationDirPath();
}

inline QString iniPath()
{
    return baseDir() + QStringLiteral("/PSGame-Viewer.ini");
}

inline QString libraryDbPath()
{
    return baseDir() + QStringLiteral("/library.db");
}

inline QString coversCacheDir()
{
    return baseDir() + QStringLiteral("/cache/covers");
}

inline QString batchLogPath()
{
    // ms resolution: two applies in the same second must not overwrite
    // each other's log
    return baseDir() + QStringLiteral("/batch_rename_%1.log").arg(
        QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_hhmmsszzz")));
}

} // namespace portable
} // namespace psg
