#include "trophylistmodel.h"

namespace psg {

TrophyListModel::TrophyListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TrophyListModel::setTrophies(const QList<Item> &items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

int TrophyListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TrophyListModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_items.size())
        return QVariant();
    const Item &it = m_items.at(row);
    switch (role) {
    case NameRole: return it.name;
    case DetailRole: return it.detail;
    case IdRole: return it.id;
    case GradeRole: return it.grade;
    case HiddenRole: return it.hidden;
    case IconIndexRole: return it.iconIndex;
    case HasIconRole: return it.iconIndex >= 0;
    default: return QVariant();
    }
}

QHash<int, QByteArray> TrophyListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {DetailRole, "detail"},
        {IdRole, "tid"},
        {GradeRole, "grade"},
        {HiddenRole, "isHidden"},
        {IconIndexRole, "iconIndex"},
        {HasIconRole, "hasIcon"},
    };
}

} // namespace psg
