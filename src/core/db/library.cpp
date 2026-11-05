#include "library.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace psg {

GameLibrary::GameLibrary(const QString &dbPath, QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    m_db = new QSqlDatabase(
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                  QStringLiteral("psg_library")));
    m_db->setDatabaseName(dbPath);
    if (!m_db->open()) {
        m_error = m_db->lastError().text();
        return;
    }
    m_valid = ensureSchema();
}

GameLibrary::~GameLibrary()
{
    if (m_db && m_db->isOpen())
        m_db->close();
    const QString name = m_db ? m_db->connectionName() : QString();
    delete m_db;
    if (!name.isEmpty())
        QSqlDatabase::removeDatabase(name);
}

bool GameLibrary::ensureSchema()
{
    QSqlQuery q(*m_db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS games ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " path TEXT UNIQUE NOT NULL,"
            " title TEXT NOT NULL DEFAULT '',"
            " platform TEXT NOT NULL DEFAULT '',"
            " title_id TEXT NOT NULL DEFAULT '',"
            " content_id TEXT NOT NULL DEFAULT '',"
            " region TEXT NOT NULL DEFAULT '',"
            " version TEXT NOT NULL DEFAULT '',"
            " size INTEGER NOT NULL DEFAULT 0,"
            " added_at INTEGER NOT NULL DEFAULT 0)"))) {
        m_error = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS recent ("
            " path TEXT PRIMARY KEY NOT NULL,"
            " opened_at INTEGER NOT NULL DEFAULT 0)"))) {
        m_error = q.lastError().text();
        return false;
    }
    if (!q.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_games_title ON games(title)"))) {
        m_error = q.lastError().text();
        return false;
    }
    return true;
}

int GameLibrary::upsertGame(const QString &path, const QString &title,
                            const QString &platform, const QString &titleId,
                            const QString &contentId, const QString &region,
                            const QString &version, quint64 size)
{
    if (!m_valid || path.isEmpty())
        return 0;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO games (path, title, platform, title_id, content_id,"
        " region, version, size, added_at)"
        " VALUES (:path, :title, :platform, :title_id, :content_id,"
        " :region, :version, :size, :added)"
        " ON CONFLICT(path) DO UPDATE SET"
        " title=excluded.title, platform=excluded.platform,"
        " title_id=excluded.title_id, content_id=excluded.content_id,"
        " region=excluded.region, version=excluded.version,"
        " size=excluded.size"));
    q.bindValue(QStringLiteral(":path"), path);
    q.bindValue(QStringLiteral(":title"), title);
    q.bindValue(QStringLiteral(":platform"), platform);
    q.bindValue(QStringLiteral(":title_id"), titleId);
    q.bindValue(QStringLiteral(":content_id"), contentId);
    q.bindValue(QStringLiteral(":region"), region);
    q.bindValue(QStringLiteral(":version"), version);
    q.bindValue(QStringLiteral(":size"), QVariant(qulonglong(size)));
    q.bindValue(QStringLiteral(":added"),
                QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        m_error = q.lastError().text();
        return 0;
    }
    QSqlQuery idq(*m_db);
    idq.prepare(QStringLiteral("SELECT id FROM games WHERE path = :path"));
    idq.bindValue(QStringLiteral(":path"), path);
    if (idq.exec() && idq.next())
        return idq.value(0).toInt();
    return 0;
}

void GameLibrary::removePath(const QString &path)
{
    if (!m_valid)
        return;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral("DELETE FROM games WHERE path = :path"));
    q.bindValue(QStringLiteral(":path"), path);
    q.exec();
}

int GameLibrary::removeMissing()
{
    if (!m_valid)
        return 0;
    int removed = 0;
    const QList<LibraryEntry> all = games();
    for (const LibraryEntry &e : all) {
        if (!QFileInfo::exists(e.path)) {
            removePath(e.path);
            ++removed;
        }
    }
    return removed;
}

QList<LibraryEntry> GameLibrary::games(const QString &search) const
{
    QList<LibraryEntry> out;
    if (!m_valid)
        return out;
    QSqlQuery q(*m_db);
    QString sql = QStringLiteral(
        "SELECT id, path, title, platform, title_id, content_id, region,"
        " version, size, added_at FROM games");
    if (!search.trimmed().isEmpty()) {
        sql += QStringLiteral(
            " WHERE title LIKE :s OR title_id LIKE :s OR content_id LIKE :s"
            " OR path LIKE :s");
    }
    sql += QStringLiteral(" ORDER BY added_at DESC, title COLLATE NOCASE");
    if (!search.trimmed().isEmpty())
        q.bindValue(QStringLiteral(":s"), QStringLiteral("%") + search.trimmed()
                                             + QStringLiteral("%"));
    q.prepare(sql);
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        LibraryEntry e;
        e.id = q.value(0).toInt();
        e.path = q.value(1).toString();
        e.title = q.value(2).toString();
        e.platform = q.value(3).toString();
        e.titleId = q.value(4).toString();
        e.contentId = q.value(5).toString();
        e.region = q.value(6).toString();
        e.version = q.value(7).toString();
        e.size = q.value(8).toULongLong();
        e.addedAt = QDateTime::fromSecsSinceEpoch(q.value(9).toLongLong());
        out.append(e);
    }
    return out;
}

QStringList GameLibrary::recentFiles(int limit) const
{
    QStringList out;
    if (!m_valid)
        return out;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "SELECT path FROM recent ORDER BY opened_at DESC LIMIT :lim"));
    q.bindValue(QStringLiteral(":lim"), limit);
    if (!q.exec())
        return out;
    while (q.next())
        out.append(q.value(0).toString());
    return out;
}

void GameLibrary::addRecent(const QString &path)
{
    if (!m_valid || path.isEmpty())
        return;
    QSqlQuery q(*m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO recent (path, opened_at) VALUES (:path, :now)"
        " ON CONFLICT(path) DO UPDATE SET opened_at=excluded.opened_at"));
    q.bindValue(QStringLiteral(":path"), path);
    q.bindValue(QStringLiteral(":now"), QDateTime::currentSecsSinceEpoch());
    q.exec();
}

void GameLibrary::clearRecent()
{
    if (!m_valid)
        return;
    QSqlQuery q(*m_db);
    q.exec(QStringLiteral("DELETE FROM recent"));
}

} // namespace psg
