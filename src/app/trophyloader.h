#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <atomic>

#include "core/types.h"
#include "trophylistmodel.h"

namespace psg {

// One loaded trophy pack: the display items plus the carved icon bytes
// (icon i belongs to the item whose iconIndex == i).
struct TrophyPack {
    QList<TrophyListModel::Item> items;
    QList<QByteArray> icons;
};

// Loads the trophy pack of an opened file off the GUI thread.
//
// The ESFM fallback (bruteforcing the NP communication id) costs seconds
// of pure AES work, and reading a pack out of a container can pull up to
// 300 MB through a decompressing chain — both freeze the window when run
// on the GUI thread. The loader lives in its own worker thread; the
// controller tags every job with a token and drops results that do not
// match the currently opened file.
class TrophyLoader : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // cooperative stop for shutdown: the brute-force loop checks it
    void requestAbort() { m_abort.store(true, std::memory_order_relaxed); }

signals:
    void loaded(quint64 token, const psg::TrophyPack &pack);

public slots:
    void load(quint64 token, const QString &containerPath,
              const QList<Entry> &entries, const QVariantMap &meta);

private:
    std::atomic_bool m_abort{false};
};

} // namespace psg

Q_DECLARE_METATYPE(psg::TrophyPack)
