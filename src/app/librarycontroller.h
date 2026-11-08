#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QUrl>

#include "core/db/library.h"

namespace psg {

// QML-facing list model over the SQLite game library, with search,
// folder scanning and open/remove actions.
class LibraryController : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    // non-empty when the library database could not be opened (e.g. the
    // portable folder is read-only): the library and recent lists are
    // unavailable until it is fixed
    Q_PROPERTY(QString error READ error CONSTANT)

public:
    enum Roles {
        TitleRole = Qt::DisplayRole,
        PathRole = Qt::UserRole + 1,
        PlatformRole,
        TitleIdRole,
        ContentIdRole,
        RegionRole,
        VersionRole,
        SizeRole,
        SizeTextRole,
        AddedTextRole,
    };

    explicit LibraryController(GameLibrary *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_entries.size(); }
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    QStringList recentFiles() const;
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void removeAt(int row);
    // scan a folder for game files and add them to the library
    Q_INVOKABLE int scanFolder(const QUrl &folder, bool recursive);
    Q_INVOKABLE int removeMissing();

signals:
    void countChanged();
    void searchChanged();
    void recentFilesChanged();
    void statusMessage(const QString &message);

private:
    GameLibrary *m_library = nullptr;
    QList<LibraryEntry> m_entries;
    QString m_search;
    QString m_error;
};

} // namespace psg
