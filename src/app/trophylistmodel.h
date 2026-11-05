#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>

namespace psg::trophies {
struct Trophy;
}

namespace psg {

// Trophy list for the Trophies tab: name, detail, grade, hidden flag
// and the carved icon (served through the cover image provider).
class TrophyListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::DisplayRole,
        DetailRole = Qt::UserRole + 1,
        IdRole,
        GradeRole,
        HiddenRole,
        IconIndexRole,
        HasIconRole,
    };

    struct Item {
        QString id;
        QString name;
        QString detail;
        QString grade;
        bool hidden = false;
        int iconIndex = -1; // index into the carved icon list, -1 = none
    };

    explicit TrophyListModel(QObject *parent = nullptr);

    void setTrophies(const QList<Item> &items);
    int count() const { return m_items.size(); }

    // read access for the icon exporter
    QList<Item> items() const { return m_items; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QList<Item> m_items;
};

} // namespace psg
