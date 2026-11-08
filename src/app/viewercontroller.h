#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantList>

#include "core/rename.h"
#include "core/types.h"
#include "entrytreemodel.h"
#include "trophylistmodel.h"

class QThread;

namespace psg {

class AppSettings;
class ImageStore;
class OnlineService;
class TrophyLoader;

// Central controller between QML and the parsing core. Owns the current
// GameInfo, the derived models, the in-memory image store and the online
// lookups for the opened file.
class ViewerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY infoChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY infoChanged)
    Q_PROPERTY(bool hasFile READ hasFile NOTIFY infoChanged)
    Q_PROPERTY(QString error READ error NOTIFY infoChanged)
    Q_PROPERTY(QString packageBadge READ packageBadge NOTIFY infoChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY infoChanged)
    // cover/gallery keys get their own notify: re-emitting infoChanged
    // on every trophy pack or online cover used to re-evaluate every
    // property and rebuild the whole gallery Repeater for nothing
    Q_PROPERTY(QString coverKey READ coverKey NOTIFY galleryChanged)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY infoChanged)
    Q_PROPERTY(QStringList galleryKeys READ galleryKeys NOTIFY galleryChanged)
    Q_PROPERTY(QStringList detailsCurated READ detailsCurated NOTIFY infoChanged)
    Q_PROPERTY(QStringList detailsAll READ detailsAll NOTIFY infoChanged)
    Q_PROPERTY(QString patchInfo READ patchInfo NOTIFY patchInfoChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY infoChanged)
    Q_PROPERTY(bool undoAvailable READ undoAvailable NOTIFY infoChanged)
    // a trophy pack is being loaded off the GUI thread (ESFM fallback
    // scans can take seconds); the Trophies tab shows a loading hint
    Q_PROPERTY(bool trophiesLoading READ trophiesLoading
                   NOTIFY infoChanged)
    // the models behind the Files and Trophies tabs; CONSTANT because the
    // model objects live as long as the controller
    Q_PROPERTY(EntryTreeModel *entryModel READ entryModel CONSTANT)
    Q_PROPERTY(TrophyListModel *trophyModel READ trophyModel CONSTANT)

public:
    explicit ViewerController(AppSettings *settings, ImageStore *images,
                              QObject *parent = nullptr);
    ~ViewerController() override;

    EntryTreeModel *entryModel() const { return m_entryModel; }
    TrophyListModel *trophyModel() const { return m_trophyModel; }
    OnlineService *online() const { return m_online; }
    const GameInfo &info() const { return m_info; }

    QString title() const;
    QString fileName() const;
    bool hasFile() const { return m_info.ok; }
    QString error() const { return m_info.error; }
    QString packageBadge() const;
    QString statusSummary() const;
    QString coverKey() const { return m_coverKey; }
    QVariantList rows() const;
    QStringList galleryKeys() const { return m_galleryKeys; }
    QStringList detailsCurated() const { return m_detailsCurated; }
    QStringList detailsAll() const { return m_detailsAll; }
    QString patchInfo() const { return m_patchInfo; }
    bool busy() const { return m_busy; }
    bool trophiesLoading() const { return m_trophiesLoading; }
    bool undoAvailable() const { return !m_undoNew.isEmpty(); }

    Q_INVOKABLE void openPath(const QUrl &path);
    Q_INVOKABLE QString extractEntry(int entryIndex, const QUrl &destDir,
                                     const QString &newName = QString());
    Q_INVOKABLE QString extractAll(const QUrl &destDir);
    Q_INVOKABLE bool saveCover(const QUrl &destFile);
    Q_INVOKABLE QString copyCoverToClipboard() const;
    // Write every carved trophy icon to <destDir>/<game name>/<id>_<name>.png.
    // Returns a user-facing status message.
    Q_INVOKABLE QString exportTrophyIcons(const QUrl &destDir);
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
    void galleryChanged();
    void patchInfoChanged();
    void statusMessage(const QString &message);
    void fileOpened(const QString &path);
    // every successful rename (single rename, undo, batch apply) so the
    // library and recent list can follow the files to their new paths
    void pathsRenamed(const QStringList &oldPaths,
                      const QStringList &newPaths);

private:
    void loadGallery();
    void startTrophyLoad();
    void startOnlineLookups();

    AppSettings *m_settings = nullptr;
    ImageStore *m_images = nullptr;
    // online lookups run on a worker thread so the blocking HTTP below
    // never freezes the GUI; results arrive via queued signals
    OnlineService *m_online = nullptr;
    QThread *m_onlineThread = nullptr;
    // trophy packs load on their own worker: the NP-id brute-force
    // fallback is seconds of pure AES work and must never freeze the
    // window. Results carry the token of the open that started them.
    TrophyLoader *m_trophyLoader = nullptr;
    QThread *m_trophyThread = nullptr;
    quint64 m_trophyToken = 0;
    bool m_trophiesLoading = false;
    // cover lookup key of the file that started the in-flight request;
    // late replies for a previously opened file are dropped
    QString m_onlineCoverKey;
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
