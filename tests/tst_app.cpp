#include <QtTest>

#include "app/appsettings.h"
#include "app/entrytreemodel.h"
#include "app/imagestore.h"
#include "app/librarycontroller.h"
#include "app/trophyloader.h"
#include "app/updatechecker.h"
#include "app/viewercontroller.h"
#include "core/detect.h"
#include "core/db/library.h"
#include "core/types.h"
#include "core/util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

using namespace psg;

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(FIXTURES_DIR)).filePath(name);
}

// returns an empty string when the copy fails; callers QVERIFY it
QString copyFixture(const QTemporaryDir &dir, const QString &name)
{
    const QString dst = dir.filePath(name);
    if (!QFile::copy(fixture(name), dst))
        return QString();
    QFile::setPermissions(dst, QFile::ReadUser | QFile::WriteUser);
    return dst;
}

} // namespace

class TestApp : public QObject
{
    Q_OBJECT

private slots:
    // The settings/library/cover-cache of the portable layout must not
    // leak into the source tree while the tests run.
    void initTestCase()
    {
        m_base.reset(new QTemporaryDir);
        QVERIFY(m_base->isValid());
        qputenv("PSGV_BASE_DIR", m_base->path().toUtf8());
        qRegisterMetaType<TrophyPack>("psg::TrophyPack");
        // keep the update checker tests hermetic until they override it
        qputenv("PSGV_UPDATE_API",
                QUrl::fromLocalFile(m_base->filePath("none.json"))
                    .toString()
                    .toUtf8());
    }

    void cleanupTestCase()
    {
        qunsetenv("PSGV_BASE_DIR");
        qunsetenv("PSGV_UPDATE_API");
    }

    // -- every supported format opens through the controller -------------
    void openFormats();

    // -- trophies arrive asynchronously (worker thread) -------------------
    void asyncTrophies();
    void trophyLoaderCommIdShapes();

    // -- rename / undo / batch / extract flows ----------------------------
    void renameFlow();
    void renameTargetExists();
    void batchFlow();
    void extractFlow();

    // -- settings persistence ---------------------------------------------
    void settingsPersistence();

    // -- a saved language applies without a restart ------------------------
    void languageSwitchLive();

    // -- library paths dedupe, recents cap at 5 ---------------------------
    void libraryDedupeAndRecents();

    // -- clean-missing reports "nothing to clean" too ---------------------
    void removeMissingFeedback();

    // -- library controller -----------------------------------------------
    void libraryController();

    // -- update checker against a local release JSON ---------------------
    void updateChecker();

    // -- entry tree row -> entry mapping ---------------------------------
    void entryTreeMapping();

private:
    std::unique_ptr<QTemporaryDir> m_base;
};

void TestApp::openFormats()
{
    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);

    struct Row {
        const char *file;
        const char *title;
    };
    const Row rows[] = {
        {"ps2.iso", "ps2"},
        {"ps3.iso", "PSGame-Viewer PS3 Disc"},
        {"ps3_debug.pkg", "PSGame-Viewer PS3 Sample"},
        {"ps3_retail.pkg", "PSGame-Viewer PS3 Sample"},
        {"ps4_cnt.pkg", "PSGame-Viewer Sample"},
        {"ps5_fih.pkg", "PSGame-Viewer PS5 Sample"},
        {"ps5_lih.pkg", "PSGame-Viewer PS5 Sample"},
        {"ps5.exfat", "PSGame-Viewer PS5 Image"},
        {"ps5_bigpack.exfat", "PSGame-Viewer PS5 Image"},
        {"ffpfsc_exfat.ffpfsc", "PSGame-Viewer PS5 Image"},
        {"ffpfsc_ffpkg.ffpfsc", "PSGame-Viewer PS5 Image"},
        {"ffpfsc_raw.ffpfsc", "PSGame-Viewer PS5 Image"},
        {"ffpkg.ffpkg", "PSGame-Viewer PS5 Image"},
    };
    for (const Row &r : rows) {
        viewer.openPath(QUrl::fromLocalFile(fixture(QLatin1String(r.file))));
        QVERIFY2(viewer.hasFile(), r.file);
        QVERIFY2(viewer.title() == QLatin1String(r.title),
                 qPrintable(QStringLiteral("%1: title=%2")
                                .arg(QLatin1String(r.file), viewer.title())));
        QVERIFY2(viewer.entryModel()->rowCount() > 0, r.file);
        QVERIFY2(!viewer.detailsAll().isEmpty(), r.file);
    }

    // a missing file must land in the error state, not a crash
    QSignalSpy statusSpy(&viewer, &ViewerController::statusMessage);
    viewer.openPath(QUrl::fromLocalFile(fixture(QStringLiteral("nope.pkg"))));
    QVERIFY(!viewer.hasFile());
    QVERIFY(!viewer.error().isEmpty());
    QCOMPARE(statusSpy.count(), 1);
}

void TestApp::asyncTrophies()
{
    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);

    // the ffpfsc pack carries a real retail PS3 conf (plain XML, no
    // communication id anywhere): the loader must read it directly —
    // off the GUI thread — and still deliver
    viewer.openPath(QUrl::fromLocalFile(fixture(QStringLiteral("ffpfsc_exfat.ffpfsc"))));
    QVERIFY(viewer.hasFile());
    QVERIFY(viewer.trophiesLoading());
    QTRY_COMPARE_WITH_TIMEOUT(viewer.trophyModel()->count(), 3, 60000);
    QVERIFY(!viewer.trophiesLoading());
    QCOMPARE(viewer.trophyModel()->items().at(0).name,
             QStringLiteral("First Steps"));
    QCOMPARE(viewer.trophyModel()->items().at(1).name,
             QString::fromUtf8("戦国の初陣"));

    // reopening a file without a pack must clear the list immediately and
    // drop the (impossible here, but contractually) late result
    viewer.openPath(QUrl::fromLocalFile(fixture(QStringLiteral("ps2.iso"))));
    QTRY_VERIFY(!viewer.trophiesLoading());
    QCOMPARE(viewer.trophyModel()->count(), 0);

    // a padded TRP (header total < physical size) still parses — the
    // loading state settles through the normal success path
    viewer.openPath(
        QUrl::fromLocalFile(fixture(QStringLiteral("ps5_bigpack.exfat"))));
    QTRY_COMPARE_WITH_TIMEOUT(viewer.trophyModel()->count(), 3, 60000);
    QVERIFY(!viewer.trophiesLoading());
}

void TestApp::trophyLoaderCommIdShapes()
{
    // PS4 PARAM.SFO carries NP_COMMUNICATION_ID; PS5 param.json carries
    // npCommunicationId; PS3 folders/PKGs spell the id out in the
    // TROPDIR/<NPWRxxxxx_00>/ entry path. All three must decrypt the
    // ESFM pack directly — no brute force.
    struct Shape {
        const char *metaKey;      // null: id comes from the entry path
        QString entryName;
    };
    const Shape shapes[] = {
        {"NP_COMMUNICATION_ID", QStringLiteral("sce_sys/trophy/trophy00.trp")},
        {"npCommunicationId", QStringLiteral("sce_sys/trophy/trophy00.trp")},
        {nullptr, QStringLiteral("TROPDIR/NPWR99999_00/TROPHY.TRP")},
    };
    for (const Shape &shape : shapes) {
        TrophyLoader loader;
        QSignalSpy spy(&loader, &TrophyLoader::loaded);

        QList<Entry> entries;
        Entry e;
        e.name = shape.entryName;
        e.size = quint64(QFileInfo(fixture(QStringLiteral("trophy_esfm.trp"))).size());
        e.source = Entry::Source::LocalFile;
        e.localPath = fixture(QStringLiteral("trophy_esfm.trp"));
        entries.append(e);

        QVariantMap meta;
        if (shape.metaKey)
            meta.insert(QString::fromLatin1(shape.metaKey),
                        QStringLiteral("NPWR99999_00"));
        loader.load(1, QString(), entries, meta);

        QCOMPARE(spy.count(), 1);
        const TrophyPack pack = spy.at(0).at(1).value<TrophyPack>();
        QCOMPARE(pack.items.size(), 2);
        QCOMPARE(pack.items.at(1).name, QStringLiteral("Silver Lining"));
        QVERIFY(!pack.icons.isEmpty());
    }
}

void TestApp::renameFlow()
{
    QTemporaryDir dir;
    const QString path = copyFixture(dir, QStringLiteral("ps4_cnt.pkg"));
    QVERIFY(!path.isEmpty());

    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);
    viewer.openPath(QUrl::fromLocalFile(path));
    QVERIFY(viewer.hasFile());
    QTRY_VERIFY(!viewer.trophiesLoading());

    QSignalSpy renamedSpy(&viewer, &ViewerController::pathsRenamed);
    QSignalSpy statusSpy(&viewer, &ViewerController::statusMessage);

    // default preview produces a non-empty clean name
    const QString preview =
        viewer.renamePreview(QString(), QVariantMap());
    QVERIFY(!preview.isEmpty());

    // custom pattern with tokens
    const QString custom = viewer.renamePreview(
        QStringLiteral("{tid} [{orig}]"), QVariantMap());
    QVERIFY(custom.contains(QLatin1String("[")));
    QVERIFY(custom.contains(QLatin1String("]")));

    // apply renames on disk, keeps the suffix, and reports the pair
    const QString target = viewer.applyRename(QString(), QVariantMap());
    QVERIFY(!target.isEmpty());
    QVERIFY(QFileInfo::exists(target));
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(target.endsWith(QLatin1String(".pkg"),
                            Qt::CaseInsensitive));
    QCOMPARE(renamedSpy.count(), 1);
    QCOMPARE(renamedSpy.at(0).at(0).toStringList(), QStringList{path});

    // undo restores the original name
    QVERIFY(viewer.undoAvailable());
    QVERIFY(viewer.undoRename());
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(!QFileInfo::exists(target));
    QCOMPARE(renamedSpy.count(), 2);
    QVERIFY(!viewer.undoAvailable());
}

void TestApp::renameTargetExists()
{
    QTemporaryDir dir;
    const QString path = copyFixture(dir, QStringLiteral("ps4_cnt.pkg"));
    QVERIFY(!path.isEmpty());

    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);
    viewer.openPath(QUrl::fromLocalFile(path));
    QVERIFY(viewer.hasFile());
    QTRY_VERIFY(!viewer.trophiesLoading());

    // occupy the clean target name: the rename must refuse, not clobber
    const QString target = viewer.renamePreview(QString(), QVariantMap())
        + QStringLiteral(".pkg");
    QFile blocker(QFileInfo(path).dir().filePath(target));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    QVERIFY(viewer.applyRename(QString(), QVariantMap()).isEmpty());
    QVERIFY(QFileInfo::exists(path)); // untouched
}

void TestApp::batchFlow()
{
    QTemporaryDir dir;
    const QString a = copyFixture(dir, QStringLiteral("ps2.iso"));
    const QString b = copyFixture(dir, QStringLiteral("ps4_cnt.pkg"));
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());

    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);

    QCOMPARE(viewer.batchScan({QUrl::fromLocalFile(dir.path())}, false), 2);
    // "{orig} [x]" guarantees names that differ from the originals
    const QVariantList plan =
        viewer.batchPreview(QStringLiteral("{orig} [x]"), QVariantMap());
    QCOMPARE(plan.size(), 2);
    int ok = 0;
    for (const QVariant &v : plan)
        ok += v.toMap().value(QStringLiteral("ok")).toBool() ? 1 : 0;
    QCOMPARE(ok, 2);

    QSignalSpy renamedSpy(&viewer, &ViewerController::pathsRenamed);
    const QVariantMap res =
        viewer.batchApply(); // applies the last preview
    QCOMPARE(res.value(QStringLiteral("renamed")).toInt(), 2);
    QCOMPARE(res.value(QStringLiteral("skipped")).toInt(), 0);
    QVERIFY(!res.value(QStringLiteral("logPath")).toString().isEmpty());
    QCOMPARE(renamedSpy.count(), 1);
    QVERIFY(!QFileInfo::exists(a));
    QVERIFY(!QFileInfo::exists(b));

    // revert through the log restores both names
    const QString msg = viewer.batchRevert(
        QUrl::fromLocalFile(res.value(QStringLiteral("logPath")).toString()),
        true);
    QVERIFY(msg.contains(QLatin1String("Reverted 2")));
    QVERIFY(QFileInfo::exists(a));
    QVERIFY(QFileInfo::exists(b));
}

void TestApp::extractFlow()
{
    QTemporaryDir dir;
    ImageStore images;
    AppSettings settings;
    ViewerController viewer(&settings, &images);
    viewer.openPath(QUrl::fromLocalFile(fixture(QStringLiteral("ps4_cnt.pkg"))));
    QVERIFY(viewer.hasFile());

    // the icon entry extracts with its exact size
    int iconIdx = -1;
    const GameInfo info = viewer.info();
    for (int i = 0; i < info.entries.size(); ++i)
        if (info.entries.at(i).name == QLatin1String("icon0.png"))
            iconIdx = i;
    QVERIFY(iconIdx >= 0);
    const QString out = viewer.extractEntry(
        iconIdx, QUrl::fromLocalFile(dir.path()));
    QVERIFY(!out.isEmpty());
    QCOMPARE(QFileInfo(out).size(), qint64(75));

    // extract-all writes every non-empty entry
    const QString allDir = dir.filePath(QStringLiteral("all"));
    const QString all = viewer.extractAll(QUrl::fromLocalFile(allDir));
    QVERIFY(QFileInfo(all).isDir());
    QVERIFY(QFileInfo(QDir(allDir).filePath(QStringLiteral("icon0.png"))).exists());
}

void TestApp::settingsPersistence()
{
    {
        AppSettings s;
        s.setTheme(QStringLiteral("light"));
        s.setLanguage(QStringLiteral("id"));
        s.setOnlineEnabled(false);
        s.setCheckUpdatesOnStartup(false);
        QVariantMap inc;
        inc.insert(QStringLiteral("title"), true);
        inc.insert(QStringLiteral("tid"), false);
        s.saveRenamePrefs(inc, QStringLiteral("custom"),
                          QStringLiteral("{tid}"));
    }
    // a fresh instance reads the same values back from the ini
    AppSettings s2;
    QCOMPARE(s2.theme(), QStringLiteral("light"));
    QCOMPARE(s2.language(), QStringLiteral("id"));
    QVERIFY(!s2.onlineEnabled());
    QVERIFY(!s2.checkUpdatesOnStartup());
    QCOMPARE(s2.renamePreset(), QStringLiteral("custom"));
    QCOMPARE(s2.renamePattern(), QStringLiteral("{tid}"));
    QVERIFY(s2.renameInclude().value(QStringLiteral("title")).toBool());
    QVERIFY(!s2.renameInclude().value(QStringLiteral("tid")).toBool());
}

void TestApp::libraryController()
{
    QTemporaryDir dir;
    const QString a = copyFixture(dir, QStringLiteral("ps2.iso"));
    const QString b = copyFixture(dir, QStringLiteral("ps4_cnt.pkg"));

    GameLibrary library(m_base->filePath(QStringLiteral("test-library.db")));
    QVERIFY(library.isValid());
    LibraryController controller(&library);

    QSignalSpy statusSpy(&controller, &LibraryController::statusMessage);
    QCOMPARE(controller.scanFolder(QUrl::fromLocalFile(dir.path()), true), 2);
    QCOMPARE(controller.count(), 2);
    QCOMPARE(controller.recentFiles(), QStringList());

    // search narrows by title
    controller.setSearch(QStringLiteral("Sample"));
    QCOMPARE(controller.count(), 1);
    controller.setSearch(QStringLiteral("ps2"));
    QCOMPARE(controller.count(), 1);
    controller.setSearch(QString());
    QCOMPARE(controller.count(), 2);

    // removing a row drops it from the database; one entry stays
    controller.removeAt(0);
    QCOMPARE(controller.count(), 1);

    // a vanished file disappears through removeMissing
    QFile::remove(a);
    QFile::remove(b);
    controller.removeMissing();
    QCOMPARE(controller.count(), 0);
    QVERIFY(statusSpy.count() >= 2);
}

void TestApp::languageSwitchLive()
{
    // saving a language must swap the translators immediately: the
    // catalog is installed and lookups resolve without any restart
    {
        AppSettings s;
        s.setLanguage(QStringLiteral("id"));
        QVERIFY(s.translationActive());
        QCOMPARE(QCoreApplication::translate("LibraryPage", "Clean missing"),
                 QStringLiteral("Bersihkan yang hilang"));

        s.setLanguage(QStringLiteral("zh_CN"));
        QVERIFY(s.translationActive());
        QCOMPARE(QCoreApplication::translate("LibraryPage", "Clean missing"),
                 QStringLiteral("\u6e05\u7406\u5931\u6548\u9879"));
    }

    // a fresh instance reading the same setting starts translated too
    // (the first instance must have uninstalled its translators when it
    // died, or the two catalogs would stack)
    AppSettings s2;
    QCOMPARE(QCoreApplication::translate("LibraryPage", "Clean missing"),
             QStringLiteral("\u6e05\u7406\u5931\u6548\u9879"));

    // back to "system": the C locale of the CI has no catalog, so the
    // source strings come back (and the next tests run untranslated)
    s2.setLanguage(QStringLiteral("system"));
    QCOMPARE(QCoreApplication::translate("LibraryPage", "Clean missing"),
             QStringLiteral("Clean missing"));
}

void TestApp::libraryDedupeAndRecents()
{
    // the canonical path form: ".." and trailing slashes collapse, so a
    // mangled reopen cannot create a second library row for one file
    QCOMPARE(normalizeGamePath(QStringLiteral("/a/b/../c/d.pkg")),
             QStringLiteral("/a/c/d.pkg"));
    QCOMPARE(normalizeGamePath(QStringLiteral("/a/b/")),
             QStringLiteral("/a/b"));
#ifdef Q_OS_WIN
    // url-typed QML signals lower-case the drive letter on Windows
    QCOMPARE(normalizeGamePath(QStringLiteral("c:/games/x.pkg")),
             QStringLiteral("C:/games/x.pkg"));
#endif

    QTemporaryDir dir;
    const QString a = copyFixture(dir, QStringLiteral("ps2.iso"));
    QVERIFY(!a.isEmpty());

    GameLibrary library(m_base->filePath(QStringLiteral("dedupe.db")));
    QVERIFY(library.isValid());
    LibraryController controller(&library);

    // the same file upserted through a mangled spelling stays one row
    library.upsertGame(a, QStringLiteral("ps2"), QStringLiteral("PS2"),
                       QString(), QString(), QString(), QString(), 1);
    const QString mangled = QFileInfo(a).dir().filePath(
        QStringLiteral("sub/../") + QFileInfo(a).fileName());
    library.upsertGame(mangled, QStringLiteral("ps2"), QStringLiteral("PS2"),
                       QString(), QString(), QString(), QString(), 1);
    QCOMPARE(library.games().size(), 1);
    controller.refresh();
    QCOMPARE(controller.count(), 1);

    // recents: same file in two spellings -> one chip; the list caps
    // at five even when the table holds more
    library.addRecent(mangled);
    library.addRecent(a);
    QCOMPARE(controller.recentFiles(), QStringList{a});

    for (int i = 0; i < 8; ++i) {
        QFile f(dir.filePath(QStringLiteral("extra%1.pkg").arg(i)));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
        f.close();
        library.addRecent(f.fileName());
    }
    QCOMPARE(controller.recentFiles().size(), 5);

    // two copies of the same file (same name, same size) count once
    QTemporaryDir dir2;
    const QString c1 = copyFixture(dir2, QStringLiteral("ps2.iso"));
    const QString sub = dir2.filePath(QStringLiteral("sub"));
    QVERIFY(QDir().mkpath(sub));
    const QString c2 = QDir(sub).filePath(QStringLiteral("ps2.iso"));
    QVERIFY(QFile::copy(c1, c2));
    GameLibrary library2(m_base->filePath(QStringLiteral("dedupe2.db")));
    QVERIFY(library2.isValid());
    LibraryController controller2(&library2);
    library2.addRecent(c1);
    library2.addRecent(c2);
    QCOMPARE(controller2.recentFiles().size(), 1);
}

void TestApp::removeMissingFeedback()
{
    QTemporaryDir dir;
    const QString a = copyFixture(dir, QStringLiteral("ps2.iso"));
    QVERIFY(!a.isEmpty());

    GameLibrary library(m_base->filePath(QStringLiteral("feedback.db")));
    QVERIFY(library.isValid());
    LibraryController controller(&library);
    QSignalSpy statusSpy(&controller, &LibraryController::statusMessage);

    // nothing missing: the action must still say so instead of being
    // silent
    library.upsertGame(a, QStringLiteral("ps2"), QStringLiteral("PS2"),
                       QString(), QString(), QString(), QString(), 1);
    controller.refresh();
    QCOMPARE(controller.removeMissing(), 0);
    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy.at(0).at(0).toString().contains(
                 QStringLiteral("All games present")),
             true);

    // and with a vanished file it reports the removal count
    QFile::remove(a);
    QCOMPARE(controller.removeMissing(), 1);
    QCOMPARE(statusSpy.count(), 2);
    QCOMPARE(statusSpy.at(1).at(0).toString().contains(
                 QStringLiteral("Removed 1")),
             true);
}

void TestApp::updateChecker()
{
    const QString json = m_base->filePath(QStringLiteral("release.json"));
    {
        QFile f(json);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({
            "tag_name": "v9.9.9",
            "name": "Big One",
            "body": "## New Features\n- everything",
            "html_url": "https://example.com/rel",
            "assets": [
                {"name": "a.zip", "browser_download_url": "https://example.com/a.zip", "size": 10}
            ]
        })");
    }
    qputenv("PSGV_UPDATE_API",
            QUrl::fromLocalFile(json).toString().toUtf8());

    UpdateChecker checker(QStringLiteral("0.0.1"));
    QSignalSpy spy(&checker, &UpdateChecker::checkFinished);
    checker.checkNow();
    QVERIFY(spy.wait(10000));

    const QVariantMap res = spy.at(0).at(0).toMap();
    QCOMPARE(res.value(QStringLiteral("state")).toString(),
             QStringLiteral("update-available"));
    QCOMPARE(res.value(QStringLiteral("latest")).toString(),
             QStringLiteral("v9.9.9"));
    QCOMPARE(res.value(QStringLiteral("current")).toString(),
             QStringLiteral("0.0.1"));
    QCOMPARE(res.value(QStringLiteral("pageUrl")).toString(),
             QStringLiteral("https://example.com/rel"));
    QCOMPARE(res.value(QStringLiteral("assets")).toList().size(), 1);

    // a tag that is not newer reads as up-to-date
    UpdateChecker same(QStringLiteral("9.9.9"));
    QSignalSpy spy2(&same, &UpdateChecker::checkFinished);
    same.checkNow();
    QVERIFY(spy2.wait(10000));
    QCOMPARE(spy2.at(0).at(0).toMap()
                 .value(QStringLiteral("state")).toString(),
             QStringLiteral("up-to-date"));

    // an unreachable endpoint reports the error state
    qputenv("PSGV_UPDATE_API",
            QStringLiteral("file:///nope/missing.json").toUtf8());
    UpdateChecker broken(QStringLiteral("0.0.1"));
    QSignalSpy spy3(&broken, &UpdateChecker::checkFinished);
    broken.checkNow();
    QVERIFY(spy3.wait(10000));
    QCOMPARE(spy3.at(0).at(0).toMap()
                 .value(QStringLiteral("state")).toString(),
             QStringLiteral("error"));
}

void TestApp::entryTreeMapping()
{
    EntryTreeModel model;

    // the nameless system entry (id 0x200) is not listed, but the rows
    // must still map back to the original entry indices for extraction
    QList<Entry> entries;
    Entry sys;
    sys.id = 0x200;
    sys.name = QString(); // nameless: skipped by the listing
    sys.size = 64;
    entries.append(sys);
    Entry icon;
    icon.id = 0x1200;
    icon.name = QStringLiteral("sce_sys/icon0.png");
    icon.size = 75;
    entries.append(icon);
    Entry nested;
    nested.id = 1;
    nested.name = QStringLiteral("sce_sys/trophy/trophy00.trp");
    nested.size = 1024;
    entries.append(nested);
    model.setEntries(entries);

    QVERIFY(model.rowCount() >= 2); // "sce_sys" folder + icon file (+ trp)

    // every visible file row maps to the ORIGINAL entry index
    const int iconRow = [&] {
        for (int r = 0; r < model.rowCount(); ++r)
            if (model.data(model.index(r, 0),
                           EntryTreeModel::PathRole).toString()
                == QLatin1String("sce_sys/icon0.png"))
                return r;
        return -1;
    }();
    QVERIFY(iconRow >= 0);
    QCOMPARE(model.data(model.index(iconRow, 0),
                        EntryTreeModel::EntryIndexRole).toInt(), 1);
    QCOMPARE(model.entryIndexAt(iconRow), 1);

    // expand/collapse re-order the rows (folders first, then files, and
    // a fully expanded tree reveals trophy00.trp above icon0.png); the
    // visible file rows must still map to their ORIGINAL entry indices
    const auto entryIndexOfPath = [&model](const QString &path) {
        for (int r = 0; r < model.rowCount(); ++r)
            if (model.data(model.index(r, 0),
                           EntryTreeModel::PathRole).toString() == path)
                return model.entryIndexAt(r);
        return -1;
    };
    model.expandAll();
    QCOMPARE(entryIndexOfPath(QStringLiteral("sce_sys/icon0.png")), 1);
    QCOMPARE(entryIndexOfPath(
                 QStringLiteral("sce_sys/trophy/trophy00.trp")), 2);
    model.collapseAll();
    QCOMPARE(model.rowCount(), 1); // only the top folder stays visible
    model.expandAll();
    QCOMPARE(entryIndexOfPath(QStringLiteral("sce_sys/icon0.png")), 1);
}

QTEST_MAIN(TestApp)
#include "tst_app.moc"
