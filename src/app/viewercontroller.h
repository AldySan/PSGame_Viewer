#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

#include "core/rename.h"
#include "core/types.h"
#include "entrytreemodel.h"
#include "trophylistmodel.h"

namespace psg {

class AppSettings;
class ImageStore;
class OnlineService;

// Central controller between QML and the parsing core. Owns the current
// GameInfo, the derived models, the in-memory image store and the online
// lookups for the opened file.
class ViewerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY infoChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY infoChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY infoChanged)
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY infoChanged)
    Q_PROPERTY(QString error READ error NOTIFY infoChanged)
    Q_PROPERTY(QString platformBadge READ platformBadge NOTIFY infoChanged)
    Q_PROPERTY(QString packageBadge READ packageBadge NOTIFY infoChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY infoChanged)
    Q_PROPERTY(QString coverKey READ coverKey NOTIFY infoChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY infoChanged)
    Q_PROPERTY(QStringList galleryKeys READ galleryKeys NOTIFY infoChanged)
    Q_PROPERTY(QStringList detailsCurated READ detailsCurated NOTIFY infoChanged)
    Q_PROPERTY(QStringList detailsAll READ detailsAll NOTIFY infoChanged)
    Q_PROPERTY(QString patchInfo READ patchInfo NOTIFY patchInfoChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY infoChanged)
    Q_PROPERTY(bool undoAvailable READ undoAvailable NOTIFY infoChanged)

public:
    explicit ViewerController(AppSettings *settings, ImageStore *images,
                              QObject *parent = nullptr);

    EntryTreeModel *entryModel() const { return m_entryModel; }
    TrophyListModel *trophyModel() const { return m_trophyModel; }
    OnlineService *online() const { return m_online; }
    const GameInfo &info() const { return m_info; }

    QString title() const;
    QString fileName() const;
    QString filePath() const;
    bool hasFile() const { return m_info.ok; }
    QString error() const { return m_info.error; }
    QString platformBadge() const;
    QString packageBadge() const;
    QString statusSummary() const;
    QString coverKey() const { return m_coverKey; }
    QVariantList rows() const;
    QStringList galleryKeys() const { return m_galleryKeys; }
    QStringList detailsCurated() const { return m_detailsCurated; }
    QStringList detailsAll() const { return m_detailsAll; }
    QString patchInfo() const { return m_patchInfo; }
    bool busy() const { return m_busy; }
    bool undoAvailable() const { return !m_undoNew.isEmpty(); }

    Q_INVOKABLE void openPath(const QUrl &path);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString extractEntry(int entryIndex, const QUrl &destDir,
                                     const QString &newName = QString());
    Q_INVOKABLE QString extractAll(const QUrl &destDir);
    Q_INVOKABLE bool saveCover(const QUrl &destFile);
    Q_INVOKABLE QString copyCoverToClipboard() const;
    Q_INVOKABLE void selectCover(const QString &key);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QString renamePreview(const QString &pattern,
                                      const QVariantMap &include) const;
    Q_INVOKABLE QString applyRename(const QString &pattern,
                                    const QVariantMap &include);
    Q_INVOKABLE bool undoRename();

    // rename presets for the dialogs: [{id, label, pattern}]
    Q_INVOKABLE QVariantList renamePresetList() const;
    // batch rename: scan -> preview -> apply (+ revert log)
    Q_INVOKABLE int batchScan(const QList<QUrl> &inputs, bool recursive);
    Q_INVOKABLE QVariantList batchPreview(const QString &pattern,
                                          const QVariantMap &include);
    Q_INVOKABLE QVariantMap batchApply();
    Q_INVOKABLE QString batchRevert(const QUrl &logFile, bool apply);

signals:
    void infoChanged();
    void patchInfoChanged();
    void statusMessage(const QString &message);
    void fileOpened(const QString &path);

private:
    void loadTrophies();
    void loadGallery();
    void startOnlineLookups();
    QString galleryEntryName(int i) const;

    AppSettings *m_settings = nullptr;
    ImageStore *m_images = nullptr;
    OnlineService *m_online = nullptr;
    EntryTreeModel *m_entryModel = nullptr;
    TrophyListModel *m_trophyModel = nullptr;
    GameInfo m_info;
    QString m_coverKey;
    QStringList m_galleryKeys;
    QStringList m_detailsCurated;
    QStringList m_detailsAll;
    QString m_patchInfo;
    bool m_busy = false;
    // single-file rename undo (old, new)
    QString m_undoOld;
    QString m_undoNew;
    // batch rename state
    QStringList m_batchFiles;
    QList<BatchItem> m_batchPlan;
};

} // namespace psg
