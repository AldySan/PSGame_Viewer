#include "viewercontroller.h"

#include "appsettings.h"
#include "core/detect.h"
#include "core/details.h"
#include "core/extract.h"
#include "core/rename.h"
#include "core/trophies.h"
#include "core/util.h"
#include "imagestore.h"
#include "online.h"
#include "portable.h"
#include "trophyloader.h"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QMetaType>
#include <QRegularExpression>
#include <QThread>

namespace psg {
namespace {

QImage cropPs2CoverBand(const QImage &img)
{
    // PS2 box-front scans carry a black "PlayStation 2" band on top.
    // Detect it via the saturated logo in the top-right corner, then cut
    // at the strongest horizontal edge in the top 13%.
    if (img.width() < 64 || img.height() < 160)
        return img;
    int saturated = 0;
    for (int y = 0; y < img.height() * 13 / 100; ++y) {
        for (int x = img.width() * 72 / 100; x < img.width(); x += 2) {
            const QRgb c = img.pixel(x, y);
            const int mx = qMax(qRed(c), qMax(qGreen(c), qBlue(c)));
            const int mn = qMin(qRed(c), qMin(qGreen(c), qBlue(c)));
            if (mx - mn > 90)
                ++saturated;
        }
    }
    if (saturated < 30)
        return img;
    int cut = img.height() * 11 / 100;
    int best = 0;
    for (int i = img.height() * 2 / 100; i < img.height() * 13 / 100; ++i) {
        int edge = 0;
        for (int x = 0; x < img.width(); x += 4) {
            const int a = qGray(img.pixel(x, i));
            const int b = qGray(img.pixel(x, i > 0 ? i - 1 : 0));
            edge += qAbs(a - b);
        }
        if (edge > best) {
            best = edge;
            cut = i;
        }
    }
    return img.copy(0, cut + 1, img.width(), img.height() - cut - 1);
}

} // namespace

ViewerController::ViewerController(AppSettings *settings, ImageStore *images,
                                   QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_images(images)
    , m_online(new OnlineService())
    , m_onlineThread(new QThread(this))
    , m_trophyLoader(new TrophyLoader())
    , m_trophyThread(new QThread(this))
    , m_entryModel(new EntryTreeModel(this))
    , m_trophyModel(new TrophyListModel(this))
{
    qRegisterMetaType<TrophyPack>("psg::TrophyPack");

    // the lookups do blocking HTTP with a 15 s transfer timeout; keep them
    // off the GUI thread so a slow or unreachable network cannot freeze
    // the window while covers and patch info load
    m_online->moveToThread(m_onlineThread);
    connect(m_onlineThread, &QThread::finished,
            m_online, &QObject::deleteLater);
    m_onlineThread->start();

    // trophy packs (up to 300 MB reads, plus the seconds-long NP-id
    // brute-force fallback) load on their own worker as well
    m_trophyLoader->moveToThread(m_trophyThread);
    connect(m_trophyThread, &QThread::finished,
            m_trophyLoader, &QObject::deleteLater);
    connect(m_trophyLoader, &TrophyLoader::loaded, this,
            [this](quint64 token, const TrophyPack &pack) {
                if (token != m_trophyToken || !m_info.ok)
                    return; // late result for a previously opened file
                m_trophiesLoading = false;
                m_images->setImages(QStringLiteral("trophy"), pack.icons);
                m_trophyModel->setTrophies(pack.items);
                emit infoChanged();
            });
    m_trophyThread->start();

    connect(m_online, &OnlineService::patchReady, this,
            [this](const QString &tid, const QString &latest, int count,
                   const QString &date) {
                if (!m_info.ok || tid.compare(m_info.patchTid,
                                              Qt::CaseInsensitive) != 0)
                    return; // late reply for a previously opened file
                if (latest.isEmpty()) {
                    m_patchInfo.clear();
                } else {
                    m_patchInfo = QStringLiteral("v%1 · %2 %3")
                                      .arg(latest)
                                      .arg(count)
                                      .arg(tr("patches"));
                    if (!date.isEmpty())
                        m_patchInfo += QStringLiteral(" · ") + date;
                }
                emit patchInfoChanged();
            });
    connect(m_online, &OnlineService::coverBytesReady, this,
            [this](const QString &key, const QByteArray &bytes) {
                if (bytes.isEmpty() || !m_info.ok)
                    return;
                if (key != m_onlineCoverKey)
                    return; // late reply for a previously opened file
                QImage img;
                if (!img.loadFromData(bytes))
                    return;
                if (m_info.platform == Platform::PS2)
                    img = cropPs2CoverBand(img);
                QBuffer buf;
                buf.open(QIODevice::WriteOnly);
                img.save(&buf, "PNG");
                m_images->setImage(QStringLiteral("online/%1").arg(key),
                                   buf.buffer());
                m_coverKey = QStringLiteral("online/%1").arg(key);
                emit infoChanged();
            });
}

ViewerController::~ViewerController()
{
    if (m_onlineThread) {
        // Ask the worker to drop any in-flight HTTP first: a blocking
        // lookup can otherwise keep the thread busy for many seconds
        // (up to 4 transfers x 15 s timeout), and destroying a QThread
        // that is still running aborts the process.
        if (m_online)
            m_online->requestAbort();
        m_onlineThread->quit();
        m_onlineThread->wait();
    }
    if (m_trophyThread) {
        // same for the trophy scan: the brute-force loop polls the flag
        // and returns early instead of blocking the destructor for seconds
        if (m_trophyLoader)
            m_trophyLoader->requestAbort();
        m_trophyThread->quit();
        m_trophyThread->wait();
    }
}

QString ViewerController::title() const
{
    return m_info.ok ? m_info.title : tr("No file opened");
}

QString ViewerController::fileName() const
{
    return QFileInfo(m_info.path).fileName();
}

QString ViewerController::filePath() const
{
    return m_info.ok ? m_info.path : QString();
}

QString ViewerController::platformBadge() const
{
    if (!m_info.ok)
        return QString();
    return m_info.row(QStringLiteral("Platform"));
}

QString ViewerController::packageBadge() const
{
    if (!m_info.ok)
        return QString();
    const QString pkg = m_info.row(QStringLiteral("Package"));
    if (pkg.contains(QLatin1String("Fake"), Qt::CaseInsensitive))
        return QStringLiteral("FPKG");
    if (pkg.contains(QLatin1String("Official"), Qt::CaseInsensitive)
        || pkg.startsWith(QLatin1String("OFC"), Qt::CaseInsensitive))
        return QStringLiteral("OFC");
    return pkg;
}

QString ViewerController::statusSummary() const
{
    if (!m_info.ok)
        return QString();
    // LMAN-style bottom bar: Type (Fake) (REGION) - vVER - System SYS
    const QString pkg = m_info.row(QStringLiteral("Package"));
    QString base = pkg.section(u'(', 0, 0).trimmed();
    if (base.isEmpty())
        base = pkg;
    QString fake;
    if (pkg.contains(QLatin1String("fake"), Qt::CaseInsensitive))
        fake = QStringLiteral(" (Fake)");
    else if (pkg.contains(QLatin1String("official"), Qt::CaseInsensitive)
             || pkg.startsWith(QLatin1String("OFC"), Qt::CaseInsensitive))
        fake = QStringLiteral(" (Official)");
    const QString shortR = regionShort(m_info.region());
    QString ver = m_info.version();
    if (!ver.isEmpty() && !ver.startsWith(u'v') && ver != QLatin1String("-"))
        ver.prepend(u'v');
    const QString sys = m_info.row(QStringLiteral("Min. System"));
    QString out = base + fake;
    if (!shortR.isEmpty() && shortR != QLatin1String("-"))
        out += QStringLiteral(" (%1)").arg(shortR);
    out += QStringLiteral(" - ")
         + ((ver.isEmpty() || ver == QLatin1String("-"))
                ? QStringLiteral("-")
                : ver);
    if (!sys.isEmpty() && sys != QLatin1String("-"))
        out += QStringLiteral(" - System %1").arg(sys);
    return out;
}

QVariantList ViewerController::rows() const
{
    QVariantList out;
    for (const SpecRow &r : m_info.rows)
        out.append(QVariantMap{{"label", r.label}, {"value", r.value}});
    return out;
}

void ViewerController::openPath(const QUrl &path)
{
    const QString local = path.isLocalFile() ? path.toLocalFile()
                                             : path.toString();
    if (local.isEmpty())
        return;
    m_busy = true;
    emit infoChanged();

    // lookups started for the previous file must not land on this one
    m_onlineCoverKey.clear();

    m_info = detect::openAny(local);
    m_patchInfo.clear();
    m_undoOld.clear();
    m_undoNew.clear();
    m_images->clear();
    m_coverKey.clear();
    m_galleryKeys.clear();
    m_detailsCurated.clear();
    m_detailsAll.clear();

    if (m_info.ok) {
        m_entryModel->setEntries(m_info.entries);
        m_detailsCurated = curatedMetaLines(m_info.platform, m_info.meta);
        m_detailsAll = fullMetaLines(m_info.meta);
        loadGallery();
        startTrophyLoad();
    } else {
        m_entryModel->setEntries({});
        m_trophyModel->setTrophies({});
        m_trophiesLoading = false;
    }
    m_busy = false;
    emit infoChanged();
    if (!m_info.ok) {
        emit statusMessage(tr("Could not open: %1").arg(m_info.error));
    } else {
        emit fileOpened(m_info.path);
        emit statusMessage(tr("Opened %1").arg(
            QFileInfo(m_info.path).fileName()));
        startOnlineLookups();
    }
}

void ViewerController::clear()
{
    m_info = GameInfo();
    m_images->clear();
    m_coverKey.clear();
    m_galleryKeys.clear();
    m_detailsCurated.clear();
    m_detailsAll.clear();
    m_patchInfo.clear();
    m_entryModel->setEntries({});
    m_trophyModel->setTrophies({});
    m_trophiesLoading = false;
    ++m_trophyToken; // invalidate any in-flight trophy load
    emit infoChanged();
}

void ViewerController::loadGallery()
{
    // Memory guard for hostile containers: real packages carry a couple
    // of dozen sce_sys PNGs; a crafted one may list thousands. Stop
    // caching past 64 images or 256 MB, whichever comes first.
    constexpr int kMaxImages = 64;
    constexpr quint64 kMaxTotalBytes = 256ull << 20;
    QList<QByteArray> gallery;
    quint64 total = 0;
    for (const Entry &e : m_info.entries) {
        if (!e.name.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
            continue;
        if (e.size == 0 || e.size > 32000000)
            continue;
        if (gallery.size() >= kMaxImages || total >= kMaxTotalBytes)
            break;
        const QByteArray bytes = readEntryBytes(e, m_info.path);
        if (bytes.isEmpty()
            || bytes.left(8) != QByteArrayLiteral("\x89PNG\r\n\x1a\n"))
            continue;
        gallery.append(bytes);
        total += quint64(bytes.size());
    }
    if (gallery.isEmpty())
        return;
    m_images->setImages(QStringLiteral("gallery"), gallery);
    for (int i = 0; i < gallery.size(); ++i)
        m_galleryKeys.append(QStringLiteral("gallery/%1").arg(i));
    m_coverKey = m_galleryKeys.value(0);
}

void ViewerController::startTrophyLoad()
{
    // anything with a .trp/.ucp entry? (cheap scan over the entry list)
    bool hasPack = false;
    for (const Entry &e : m_info.entries) {
        const QString lower = e.name.toLower();
        if ((lower.endsWith(QLatin1String(".trp"))
             || lower.endsWith(QLatin1String(".ucp")))
            && e.size > 0 && e.size < 300000000) {
            hasPack = true;
            break;
        }
    }
    m_trophyModel->setTrophies({});
    if (!hasPack) {
        m_trophiesLoading = false;
        return;
    }
    m_trophiesLoading = true;
    emit infoChanged();
    const quint64 token = ++m_trophyToken;
    QMetaObject::invokeMethod(
        m_trophyLoader, "load", Qt::QueuedConnection, Q_ARG(quint64, token),
        Q_ARG(QString, m_info.path), Q_ARG(QList<Entry>, m_info.entries),
        Q_ARG(QVariantMap, m_info.meta));
}

void ViewerController::startOnlineLookups()
{
    if (!m_settings || !m_settings->onlineEnabled())
        return;
    // patch info for the current title (fetched off the GUI thread)
    if (!m_info.patchTid.isEmpty())
        QMetaObject::invokeMethod(
            m_online, "fetchLatestPatch", Qt::QueuedConnection,
            Q_ARG(QString, m_info.patchTid));
    // PS2/PS3 carry no usable icon inside the container: fetch art
    if (m_coverKey.isEmpty()) {
        QString coverKey;
        if (m_info.platform == Platform::PS2 && !m_info.ps2Serial.isEmpty())
            coverKey = m_info.ps2Serial;
        else if (m_info.platform == Platform::PS3
                 && isPs3TitleId(m_info.titleId()))
            coverKey = m_info.titleId();
        if (!coverKey.isEmpty()) {
            m_onlineCoverKey = coverKey;
            QMetaObject::invokeMethod(
                m_online, "fetchCoverBytes", Qt::QueuedConnection,
                Q_ARG(QString, coverKey), Q_ARG(QString, coverKey),
                Q_ARG(QString, m_info.region()));
        }
    }
}

QString ViewerController::extractEntry(int entryIndex, const QUrl &destDir,
                                       const QString &newName)
{
    if (entryIndex < 0 || entryIndex >= m_info.entries.size())
        return QString();
    const Entry &e = m_info.entries.at(entryIndex);
    if (e.size == 0)
        return QString();
    const QString dir = destDir.isLocalFile() ? destDir.toLocalFile()
                                              : destDir.toString();
    QString rel = sanitizePkgPath(e.name);
    if (!newName.isEmpty()) {
        const QString sub = rel.section(u'/', 0, -2);
        rel = sub.isEmpty() ? sanitizePkgPath(newName)
                            : sub + u'/' + sanitizePkgPath(newName);
    }
    const QString out = QDir(dir).filePath(rel);
    if (!extractEntryTo(e, m_info.path, out)) {
        emit statusMessage(tr("Could not extract %1").arg(
            QFileInfo(rel).fileName()));
        return QString();
    }
    emit statusMessage(tr("Extracted %1").arg(QFileInfo(rel).fileName()));
    return out;
}

QString ViewerController::extractAll(const QUrl &destDir)
{
    const QString dir = destDir.isLocalFile() ? destDir.toLocalFile()
                                              : destDir.toString();
    const ExtractResult r = extractEntries(m_info.entries, m_info.path, dir);
    emit statusMessage(tr("Extracted %1, failed %2").arg(r.ok).arg(r.fail));
    return dir;
}

bool ViewerController::saveCover(const QUrl &destFile)
{
    if (m_coverKey.isEmpty())
        return false;
    const QByteArray bytes = m_images->image(m_coverKey);
    if (bytes.isEmpty())
        return false;
    const QString path = destFile.isLocalFile() ? destFile.toLocalFile()
                                                : destFile.toString();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(bytes) == bytes.size();
}

QString ViewerController::copyCoverToClipboard() const
{
    if (m_coverKey.isEmpty())
        return tr("Nothing to copy");
    const QByteArray bytes = m_images->image(m_coverKey);
    QImage img;
    if (bytes.isEmpty() || !img.loadFromData(bytes))
        return tr("Nothing to copy");
    QGuiApplication::clipboard()->setImage(img);
    return tr("Cover copied to clipboard");
}

QString ViewerController::exportTrophyIcons(const QUrl &destDir)
{
    const QList<TrophyListModel::Item> items =
        m_trophyModel ? m_trophyModel->items() : QList<TrophyListModel::Item>();
    if (!m_info.ok || items.isEmpty())
        return tr("No trophy icons loaded");
    const QString dir =
        destDir.isLocalFile() ? destDir.toLocalFile() : destDir.toString();
    if (dir.isEmpty())
        return tr("No trophy icons loaded");

    // save into a game-named subfolder, not loose files
    QString folder = buildCleanName(m_info);
    if (folder.isEmpty())
        folder = sanitizeFileNamePart(m_info.title);
    if (folder.isEmpty())
        folder = sanitizeFileNamePart(
            QFileInfo(m_info.path).completeBaseName());
    if (folder.isEmpty())
        folder = QStringLiteral("trophies");
    const QString dest = QDir(dir).filePath(sanitizeFileNamePart(folder));
    if (!QDir().mkpath(dest))
        return tr("Cannot create folder: %1").arg(dest);

    int saved = 0;
    int skipped = 0;
    for (int i = 0; i < items.size(); ++i) {
        const TrophyListModel::Item &it = items.at(i);
        const QByteArray bytes =
            m_images->image(QStringLiteral("trophy/%1").arg(it.iconIndex));
        if (it.iconIndex < 0 || bytes.isEmpty()
            || bytes.left(8) != QByteArrayLiteral("\x89PNG\r\n\x1a\n")) {
            ++skipped;
            continue;
        }
        QString name = sanitizeFileNamePart(
            QStringLiteral("%1_%2").arg(it.id, it.name));
        if (name.isEmpty())
            name = QStringLiteral("trophy_%1").arg(i);
        QFile f(QDir(dest).filePath(name + QStringLiteral(".png")));
        if (f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size())
            ++saved;
        else
            ++skipped;
    }
    QString msg = tr("Saved %1 trophy icons to %2")
                      .arg(saved)
                      .arg(QDir(dir).relativeFilePath(dest));
    if (skipped > 0)
        msg += tr(" (%1 skipped)").arg(skipped);
    emit statusMessage(msg);
    return msg;
}

void ViewerController::selectCover(const QString &key)
{
    if (m_images->image(key).isEmpty())
        return;
    m_coverKey = key;
    emit infoChanged();
}

void ViewerController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
    emit statusMessage(tr("Copied to clipboard"));
}

QString ViewerController::renamePreview(const QString &pattern,
                                        const QVariantMap &include) const
{
    if (!m_info.ok)
        return QString();
    RenameParts parts;
    parts.title = include.value(QStringLiteral("title"), true).toBool();
    parts.tid = include.value(QStringLiteral("tid"), true).toBool();
    parts.ver = include.value(QStringLiteral("ver"), true).toBool();
    parts.region = include.value(QStringLiteral("region"), true).toBool();
    const QString origBase =
        QFileInfo(m_info.path).completeBaseName().remove(
            QRegularExpression(QStringLiteral("_\\d+$")));
    const FormatResult fr = pattern.isEmpty()
        ? FormatResult{buildCleanName(m_info, parts), {}}
        : formatName(m_info, pattern, origBase, parts);
    return fr.base;
}

QString ViewerController::applyRename(const QString &pattern,
                                      const QVariantMap &include)
{
    if (!m_info.ok)
        return QString();
    const QString base = renamePreview(pattern, include);
    if (base.isEmpty())
        return QString();
    const QFileInfo fi(m_info.path);
    // split sets keep their part suffix
    QString suffix;
    static const QRegularExpression re(QStringLiteral("_(\\d+)$"));
    const auto m = re.match(fi.completeBaseName());
    if (m.hasMatch())
        suffix = u'_' + m.captured(1);
    const QString newName = base + suffix
        + (fi.suffix().isEmpty() ? QString()
                                 : u'.' + fi.suffix());
    const QString target = fi.dir().filePath(newName);
    if (QFileInfo::exists(target))
        return QString();
    QFile f(m_info.path);
    if (!f.rename(target))
        return QString();
    m_undoOld = m_info.path;
    m_undoNew = target;
    m_info.path = target;
    emit infoChanged();
    emit pathsRenamed({m_undoOld}, {m_undoNew});
    emit statusMessage(tr("Renamed to %1").arg(newName));
    return target;
}

bool ViewerController::undoRename()
{
    if (m_undoOld.isEmpty() || m_undoNew.isEmpty())
        return false;
    QFile f(m_undoNew);
    if (!f.rename(m_undoOld))
        return false;
    const QString undoNew = m_undoNew;
    const QString undoOld = m_undoOld;
    m_info.path = m_undoOld;
    m_undoOld.clear();
    m_undoNew.clear();
    emit infoChanged();
    emit pathsRenamed({undoNew}, {undoOld});
    emit statusMessage(tr("Rename undone"));
    return true;
}

QVariantList ViewerController::renamePresetList() const
{
    QVariantList out;
    for (const RenamePreset &p : renamePresets()) {
        out.append(QVariantMap{{"id", p.id},
                               {"label", p.label},
                               {"pattern", p.pattern}});
    }
    return out;
}

int ViewerController::batchScan(const QList<QUrl> &inputs, bool recursive)
{
    QStringList paths;
    for (const QUrl &u : inputs)
        paths.append(u.isLocalFile() ? u.toLocalFile() : u.toString());
    m_batchFiles = collectBatchFiles(paths, recursive);
    return m_batchFiles.size();
}

QVariantList ViewerController::batchPreview(const QString &pattern,
                                            const QVariantMap &include)
{
    RenameParts parts;
    parts.title = include.value(QStringLiteral("title"), true).toBool();
    parts.tid = include.value(QStringLiteral("tid"), true).toBool();
    parts.ver = include.value(QStringLiteral("ver"), true).toBool();
    parts.region = include.value(QStringLiteral("region"), true).toBool();
    m_batchPlan = previewBatch(m_batchFiles, pattern, parts);
    QVariantList out;
    for (const BatchItem &item : m_batchPlan) {
        QVariantMap m;
        m.insert(QStringLiteral("ok"), item.ok);
        m.insert(QStringLiteral("reason"), item.reason);
        QStringList olds, news;
        for (const QString &f : item.files)
            olds.append(QFileInfo(f).fileName());
        for (const QString &f : item.newFiles)
            news.append(QFileInfo(f).fileName());
        m.insert(QStringLiteral("files"), olds);
        m.insert(QStringLiteral("newFiles"), news);
        out.append(m);
    }
    return out;
}

QVariantMap ViewerController::batchApply()
{
    // portable: the revert log lives next to the executable
    const QString log = portable::batchLogPath();
    const BatchOutcome out = applyBatch(m_batchPlan, log);
    QVariantMap m;
    m.insert(QStringLiteral("renamed"), out.renamed.size());
    m.insert(QStringLiteral("skipped"), out.skipped.size());
    m.insert(QStringLiteral("logPath"), out.logPath);
    // surfaced so the dialog can warn that these renames cannot be
    // reverted from a log (the renames themselves still happened)
    if (!out.logError.isEmpty())
        m.insert(QStringLiteral("logError"), out.logError);
    if (!out.renamed.isEmpty()) {
        QStringList olds, news;
        for (const auto &pair : out.renamed) {
            olds.append(pair.first);
            news.append(pair.second);
        }
        emit pathsRenamed(olds, news);
    }
    emit statusMessage(tr("Batch: renamed %1, skipped %2")
                           .arg(out.renamed.size())
                           .arg(out.skipped.size()));
    return m;
}

QString ViewerController::batchRevert(const QUrl &logFile, bool apply)
{
    const QString path =
        logFile.isLocalFile() ? logFile.toLocalFile() : logFile.toString();
    const QList<QPair<QString, QString>> pairs = parseRevertLog(path);
    if (pairs.isEmpty())
        return tr("Nothing to revert in %1").arg(QFileInfo(path).fileName());
    if (!apply)
        return tr("Dry run: %1 renames would be reverted. Run with apply.")
            .arg(pairs.size());
    const BatchOutcome out = applyRevert(pairs);
    return tr("Reverted %1, skipped %2")
        .arg(out.renamed.size())
        .arg(out.skipped.size());
}

} // namespace psg
