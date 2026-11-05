#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class QSqlDatabase;

namespace psg {

// One library row: a game file that was opened or scanned.
struct LibraryEntry {
    int id = 0;
    QString path;
    QString title;
    QString platform;   // PS2/PS3/PS4/PS5
    QString titleId;
    QString contentId;
    QString region;
    QString version;
    quint64 size = 0;
    QDateTime addedAt;
};

// SQLite-backed collection of known games + a recent-files list.
// The database lives next to the settings (AppDataLocation/library.db).
class GameLibrary : public QObject
{
    Q_OBJECT

public:
    explicit GameLibrary(const QString &dbPath,
                         QObject *parent = nullptr);
    ~GameLibrary() override;

    bool isValid() const { return m_valid; }
    QString lastError() const { return m_error; }

    // insert or update by path; returns the row id (0 on failure)
    int upsertGame(const QString &path, const QString &title,
                   const QString &platform, const QString &titleId,
                   const QString &contentId, const QString &region,
                   const QString &version, quint64 size);
    void removePath(const QString &path);
    // drop rows whose files no longer exist
    int removeMissing();

    // search across title, Title ID and Content ID (newest first)
    QList<LibraryEntry> games(const QString &search = QString()) const;
    QStringList recentFiles(int limit = 20) const;
    void addRecent(const QString &path);
    void clearRecent();

private:
    bool ensureSchema();

    QSqlDatabase *m_db = nullptr;
    bool m_valid = false;
    QString m_error;
};

} // namespace psg
