#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QImage>

#include <cstdio>

#include "app/appsettings.h"
#include "app/entrytreemodel.h"
#include "app/imagestore.h"
#include "app/librarycontroller.h"
#include "app/portable.h"
#include "app/updatechecker.h"
#include "app/viewercontroller.h"
#include "core/db/library.h"
#include "core/detect.h"
#include "core/rename.h"
#include "core/types.h"
#include "core/util.h"

#include <iostream>

#ifndef PSGV_VERSION
#define PSGV_VERSION "0.0.1"
#endif

using namespace psg;

#ifdef PSGV_UI_TEST
// defined below main()'s helpers: opens every fixture format through
// the real UI and returns a ctest exit code
int runUiMatrix(QGuiApplication &app, QQmlApplicationEngine &engine,
                ViewerController &viewer, const QString &outDir,
                const int *qmlIssues);
#endif

namespace {

void printInfo(const QString &path)
{
    const GameInfo r = detect::openAny(path);
    if (!r.ok) {
        std::cout << "ERROR: " << r.error.toStdString() << "\n";
        return;
    }
    std::cout << "FILE: " << QFileInfo(path).fileName().toStdString() << "\n";
    std::cout << "TITLE: " << r.title.toStdString() << "\n";
    for (const SpecRow &row : r.rows)
        std::cout << row.label.toStdString() << ": "
                  << row.value.toStdString() << "\n";
    std::cout << "--- entries (" << r.entries.size() << ") ---\n";
    for (const Entry &e : r.entries) {
        std::cout << "  id=" << e.id << " size=" << e.size << " name='"
                  << e.name.toStdString() << "'";
        if (!e.codec.isEmpty() && e.codec != QLatin1String("-"))
            std::cout << " [" << e.codec.toStdString() << "]";
        std::cout << "\n";
    }
    for (const QString &line : r.amprLines)
        std::cout << "  AMPR: " << line.toStdString() << "\n";
}

int runInfo(const QStringList &files)
{
    for (const QString &f : files)
        printInfo(f);
    return 0;
}

int runBatch(const QStringList &inputs, bool recursive, bool apply,
             const QString &pattern)
{
    const QStringList files = collectBatchFiles(inputs, recursive);
    if (files.isEmpty()) {
        std::cout << "No game files found.\n";
        return 0;
    }
    const QList<BatchItem> plan = previewBatch(files, pattern);
    const int nOk = int(std::count_if(plan.cbegin(), plan.cend(),
                                      [](const BatchItem &i) { return i.ok; }));
    for (const BatchItem &item : plan) {
        if (!item.newFiles.isEmpty()) {
            for (int i = 0; i < item.files.size()
                             && i < item.newFiles.size(); ++i) {
                const char *tag = item.ok ? "OK  " : "SKIP";
                std::cout << "[" << tag << "] "
                          << QFileInfo(item.files.at(i)).fileName().toStdString()
                          << "  ->  "
                          << QFileInfo(item.newFiles.at(i)).fileName()
                                 .toStdString();
                if (!item.ok)
                    std::cout << "  (" << item.reason.toStdString() << ")";
                std::cout << "\n";
            }
        } else {
            std::cout << "[SKIP] "
                      << QFileInfo(item.files.first()).fileName().toStdString()
                      << "  (" << item.reason.toStdString() << ")\n";
        }
    }
    std::cout << "--- " << nOk << " of " << plan.size() << " ready ---\n";
    if (!apply) {
        std::cout << "Dry run. Re-run with --apply to rename.\n";
        return 0;
    }
    const QString log = QDir::current().filePath(QStringLiteral(
        "batch_rename_%1.log")
        .arg(QDateTime::currentDateTime().toString(
            QStringLiteral("yyyyMMdd_hhmmsszzz"))));
    const BatchOutcome out = applyBatch(plan, log);
    std::cout << "Renamed " << out.renamed.size() << ", skipped "
              << out.skipped.size() << ".";
    if (!out.logPath.isEmpty())
        std::cout << " Log: " << out.logPath.toStdString();
    std::cout << "\n";
    if (!out.logError.isEmpty())
        std::cerr << "WARNING: could not write the revert log ("
                  << out.logError.toStdString()
                  << "); these renames cannot be reverted with --revert.\n";
    return 0;
}

int runRevert(const QString &logPath, bool apply)
{
    const QList<QPair<QString, QString>> pairs = parseRevertLog(logPath);
    if (pairs.isEmpty()) {
        std::cout << "Nothing to revert in " << logPath.toStdString() << "\n";
        return 1;
    }
    if (!apply) {
        std::cout << "Dry run. Re-run with --apply to revert "
                  << pairs.size() << " renames.\n";
        return 0;
    }
    const BatchOutcome out = applyRevert(pairs);
    std::cout << "Reverted " << out.renamed.size() << ", skipped "
              << out.skipped.size() << ".\n";
    return 0;
}

int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PSGame-Viewer"));
    app.setApplicationName(QStringLiteral("PSGame-Viewer"));
    app.setApplicationVersion(QStringLiteral(PSGV_VERSION));
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("PSGame-Viewer — PS2/PS3/PS4/PS5 game file viewer"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption infoOpt(QStringLiteral("info"),
                               QStringLiteral("Print file info and exit"));
    QCommandLineOption batchOpt(
        QStringLiteral("batch-rename"),
        QStringLiteral("Batch rename game files in folders"));
    QCommandLineOption revertOpt(
        QStringLiteral("revert"),
        QStringLiteral("Revert a batch rename using its log file"));
    QCommandLineOption recursiveOpt(
        QStringLiteral("recursive"),
        QStringLiteral("Scan folders recursively (batch rename)"));
    QCommandLineOption applyOpt(QStringLiteral("apply"),
                                QStringLiteral("Execute (not a dry run)"));
    QCommandLineOption patternOpt(
        QStringLiteral("name-pattern"),
        QStringLiteral("Custom rename pattern, e.g. \"{orig} [{tid}]\""),
        QStringLiteral("pattern"));
    parser.addOptions({infoOpt, batchOpt, revertOpt, recursiveOpt, applyOpt,
                       patternOpt});
    parser.process(QCoreApplication::arguments());

    const QStringList positional = parser.positionalArguments();
    if (parser.isSet(infoOpt))
        return runInfo(positional);
    if (parser.isSet(batchOpt)) {
        if (positional.isEmpty()) {
            std::cerr << "--batch-rename needs at least one file or folder\n";
            return 1;
        }
        return runBatch(positional, parser.isSet(recursiveOpt),
                        parser.isSet(applyOpt), parser.value(patternOpt));
    }
    if (parser.isSet(revertOpt)) {
        if (positional.isEmpty()) {
            std::cerr << "--revert needs a log file\n";
            return 1;
        }
        return runRevert(positional.first(), parser.isSet(applyOpt));
    }
    parser.showHelp();
    return 0;
}

int runGui(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("PSGame-Viewer"));
    app.setApplicationName(QStringLiteral("PSGame-Viewer"));
    app.setApplicationVersion(QStringLiteral(PSGV_VERSION));

#ifdef PSGV_UI_TEST
    // --ui-test <dir>: open every fixture format through the real UI,
    // render each one and fail on QML errors (wired as a ctest so CI
    // covers the QML layer on every platform). Hermetic: settings and
    // the library live in PSGV_BASE_DIR (set by the test) and the
    // network stays off.
    QString uiTestDir;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromUtf8(argv[i]) == QLatin1String("--ui-test")
            && i + 1 < argc)
            uiTestDir = QString::fromUtf8(argv[i + 1]);
    }
    static int qmlIssues = 0;
    if (!uiTestDir.isEmpty()) {
        QDir().mkpath(uiTestDir);
        // hermetic defaults before AppSettings reads the ini
        QSettings pre(portable::iniPath(), QSettings::IniFormat);
        pre.setValue(QStringLiteral("onlineEnabled"), false);
        pre.setValue(QStringLiteral("checkUpdatesOnStartup"), false);
        pre.sync();
        qInstallMessageHandler(
            [](QtMsgType type, const QMessageLogContext &ctx,
               const QString &msg) {
                // only QML-originated problems fail the run: Qt builds
                // emit platform noise that is not ours to fix
                const bool fromQml = QByteArray(ctx.file).endsWith(".qml")
                    || QByteArray(ctx.category).startsWith("qml");
                if ((type == QtWarningMsg || type == QtCriticalMsg
                     || type == QtFatalMsg)
                    && fromQml) {
                    ++qmlIssues;
                    fprintf(stderr, "ui-test QML issue: %s\n",
                            qPrintable(msg));
                }
            });
    }
#endif

    AppSettings settings;
    ImageStore images;
    auto *provider = new PsgImageProvider(&images);
    ViewerController viewer(&settings, &images);

    // game library (SQLite) + recent files, stored next to the executable
    GameLibrary gameLibrary(portable::libraryDbPath());
    if (!gameLibrary.isValid())
        qWarning("library database unavailable: %s",
                 qPrintable(gameLibrary.lastError()));
    LibraryController library(&gameLibrary);

    // release checks; QML gets it as the Updater context property
    UpdateChecker updater(app.applicationVersion());
    QObject::connect(&viewer, &ViewerController::fileOpened, &app,
                     [&](const QString &path) {
                         if (!viewer.hasFile())
                             return;
                         gameLibrary.upsertGame(
                             path, viewer.info().title,
                             platformName(viewer.info().platform),
                             viewer.info().titleId(),
                             viewer.info().contentId(), viewer.info().region(),
                             viewer.info().version(), viewer.info().size);
                         gameLibrary.addRecent(path);
                         library.refresh();
                     });

    // renames (single, undo, batch) must not orphan the library rows and
    // recent entries that still point at the old paths
    QObject::connect(&viewer, &ViewerController::pathsRenamed, &app,
                     [&](const QStringList &olds, const QStringList &news) {
                         for (int i = 0; i < olds.size() && i < news.size(); ++i)
                             gameLibrary.renamePath(olds.at(i), news.at(i));
                         library.refresh();
                     });

    // translators (falls back to the source language when absent)
    const QString lang = settings.language();
    const QStringList locales = lang == QLatin1String("system")
        ? QLocale::system().uiLanguages()
        : QStringList{lang};
    QList<QTranslator *> translators;
    for (const QString &loc : locales) {
        auto *t = new QTranslator(&app);
        if (t->load(QStringLiteral("psgameviewer_") + QLocale(loc).name(),
                    QStringLiteral(":/i18n"))) {
            app.installTranslator(t);
            translators.append(t);
        } else {
            t->deleteLater();
        }
    }

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("psg"), provider);
    engine.rootContext()->setContextProperty(QStringLiteral("Viewer"),
                                             &viewer);
    engine.rootContext()->setContextProperty(QStringLiteral("Library"),
                                             &library);
    engine.rootContext()->setContextProperty(QStringLiteral("AppSettings"),
                                             &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("Updater"),
                                             &updater);
    engine.rootContext()->setContextProperty(QStringLiteral("AppVersion"),
                                             app.applicationVersion());
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("PSGameViewer"),
                          QStringLiteral("Main"));

#ifdef PSGV_UI_TEST
    if (!uiTestDir.isEmpty())
        return runUiMatrix(app, engine, viewer, uiTestDir, &qmlIssues);
#endif

    // testing aid: open a file and/or grab the window (headless UI checks);
    // PSGV_DIALOG opens a named dialog and PSGV_TAB selects the viewer tab
    // before the shot (settings, rename, batch, about, update dialogs;
    // 0=Files, 1=Details, 2=Trophies) so those views can be checked too.
    // PSGV_PAGE picks the top-level page (0=Viewer, 1=Library), and
    // PSGV_OPEN accepts several files separated by '|' which are opened
    // in sequence — the reopen path (model resets, late lookups, image
    // store clears) is where flicker-class bugs live
    const QString screenshot = qEnvironmentVariable("PSGV_SCREENSHOT");
    const QString openEnv = qEnvironmentVariable("PSGV_OPEN");
    const QString openDialog = qEnvironmentVariable("PSGV_DIALOG");
    const QString openPage = qEnvironmentVariable("PSGV_PAGE");
    const int openTab = qEnvironmentVariable("PSGV_TAB").toInt();
    // PSGV_EXPAND>0 expands every Files folder before the shot
    const bool expandAll =
        qEnvironmentVariable("PSGV_EXPAND").toInt() > 0;
    // with a queue of files the shot must wait for the last open to land
    int shotDelayMs = 1500;
    if (openEnv.contains(u'|')) {
        // sequential opens: one every 350 ms, screenshot afterwards
        const QStringList queue = openEnv.split(u'|');
        for (int i = 0; i < queue.size(); ++i) {
            QTimer::singleShot(350 * i, [&viewer, i, queue]() {
                viewer.openPath(QUrl::fromLocalFile(queue.at(i)));
            });
        }
        shotDelayMs = 350 * queue.size() + 900;
    } else if (!openEnv.isEmpty()) {
        viewer.openPath(QUrl::fromLocalFile(openEnv));
    }
    if (!screenshot.isEmpty()) {
        QTimer::singleShot(shotDelayMs, [&engine, screenshot, openDialog, openTab,
                                  openPage, expandAll]() {
            const auto roots = engine.rootObjects();
            for (QObject *root : roots) {
                if (!openPage.isEmpty()) {
                    // "pages" is the objectName of the viewer/library stack
                    if (auto *stack = root->findChild<QObject *>(
                            QStringLiteral("pages")))
                        stack->setProperty("currentIndex", openPage.toInt());
                }
                if (openTab > 0) {
                    // "tabBar" is the objectName of the viewer's TabBar
                    if (auto *bar = root->findChild<QObject *>(
                            QStringLiteral("tabBar")))
                        QMetaObject::invokeMethod(
                            bar, "setCurrentIndex", Q_ARG(int, openTab));
                }
                if (expandAll) {
                    // fully expand the Files tree (folder-view screenshots)
                    const QVariant v = engine.rootContext()
                                           ->contextProperty(
                                               QStringLiteral("Viewer"));
                    if (auto *ctrl = v.value<QObject *>())
                        if (auto *m = ctrl->findChild<EntryTreeModel *>())
                            m->expandAll();
                }
                if (!openDialog.isEmpty()) {
                    // dialogs carry unique objectNames; "open" is resolved
                    // by name so no Quick Controls headers are needed here
                    const auto children = root->findChildren<QObject *>();
                    for (QObject *p : children) {
                        if (p->objectName() == openDialog)
                            QMetaObject::invokeMethod(p, "open");
                    }
                }
            }
            // give the view (and its contents) a frame to lay out, and
            // wait out the async trophy load: a pack whose NP id needs
            // brute-forcing takes seconds and the shot must show the
            // settled list, not the loading hint (bounded at 25 s)
            auto *pollTrophies = new QTimer(&engine);
            auto *attempts = new int(0);
            QObject::connect(
                pollTrophies, &QTimer::timeout, [&engine, screenshot,
                                                pollTrophies, attempts]() {
                    const QVariant v = engine.rootContext()->contextProperty(
                        QStringLiteral("Viewer"));
                    const bool loading =
                        v.value<QObject *>()
                            ? v.value<QObject *>()->property(
                                  "trophiesLoading").toBool()
                            : false;
                    if (!loading || ++(*attempts) > 250) {
                        pollTrophies->stop();
                        pollTrophies->deleteLater();
                        delete attempts;
                        QTimer::singleShot(400, [&engine, screenshot]() {
                            const auto roots = engine.rootObjects();
                            if (!roots.isEmpty()) {
                                if (auto *w = qobject_cast<QQuickWindow *>(
                                        roots.first()))
                                    w->grabWindow().save(screenshot);
                            }
                            QCoreApplication::exit(0);
                        });
                    }
                });
            pollTrophies->start(100);
        });
    }

    QObject::connect(&viewer, &ViewerController::statusMessage, &app,
                     [](const QString &) {});
    return app.exec();
}

} // namespace

#ifdef PSGV_UI_TEST

namespace {

struct UiExpect {
    const char *file;
    bool trophies; // a trophy pack must load (may take seconds: the
                   // NP-id brute force runs on the worker thread)
};

bool settleAndGrab(QQmlApplicationEngine &engine, ViewerController &viewer,
                   const QString &outPng, bool wantTrophies, QString *why)
{
    auto ready = [&viewer, wantTrophies]() {
        return !viewer.busy()
            && (!wantTrophies || !viewer.trophiesLoading());
    };
    QElapsedTimer t;
    t.start();
    while (!ready() && t.elapsed() < 60000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (!ready()) {
        *why = QStringLiteral("window did not settle in 60 s");
        return false;
    }
    // a frame for layout and async images
    QElapsedTimer t2;
    t2.start();
    while (t2.elapsed() < 500)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    const auto roots = engine.rootObjects();
    if (roots.isEmpty()) {
        *why = QStringLiteral("no root object");
        return false;
    }
    auto *window = qobject_cast<QQuickWindow *>(roots.first());
    if (!window) {
        *why = QStringLiteral("root is not a window");
        return false;
    }
    const QImage shot = window->grabWindow();
    if (shot.isNull() || shot.width() < 100) {
        *why = QStringLiteral("empty grab");
        return false;
    }
    // a flat render means the scene did not really draw
    QSet<QRgb> colors;
    for (int y = 0; y < shot.height() && colors.size() <= 24; y += 16)
        for (int x = 0; x < shot.width() && colors.size() <= 24; x += 16)
            colors.insert(shot.pixel(x, y));
    if (colors.size() < 4) {
        *why = QStringLiteral("render is blank (%1 colors)")
                   .arg(colors.size());
        return false;
    }
    if (!shot.save(outPng)) {
        *why = QStringLiteral("could not save %1").arg(outPng);
        return false;
    }
    if (wantTrophies && viewer.trophyModel()->count() == 0) {
        *why = QStringLiteral("trophy pack did not load");
        return false;
    }
    return true;
}

} // namespace

// Opens every supported fixture format through the assembled UI stack,
// renders a screenshot per format and returns a ctest exit code.
int runUiMatrix(QGuiApplication &app, QQmlApplicationEngine &engine,
                ViewerController &viewer, const QString &outDir,
                const int *qmlIssues)
{
    Q_UNUSED(app);
    const UiExpect matrix[] = {
        {"ps2.iso", false},
        {"ps3.iso", false},
        {"ps3_debug.pkg", false},
        {"ps3_retail.pkg", false},
        {"ps4_cnt.pkg", false},
        {"ps5_fih.pkg", false},
        {"ps5_lih.pkg", false},
        {"ps5.exfat", false},
        {"ps5_bigpack.exfat", false}, // padded pack -> no trophies
        {"ffpfsc_exfat.ffpfsc", true},
        {"ffpfsc_ffpkg.ffpfsc", false},
        {"ffpfsc_raw.ffpfsc", true},
        {"ffpkg.ffpkg", false},
    };
    int failures = 0;
    for (const UiExpect &e : matrix) {
        const QString path = QDir(QStringLiteral(FIXTURES_DIR))
                                 .filePath(QLatin1String(e.file));
        viewer.openPath(QUrl::fromLocalFile(path));
        QString why;
        const QString png = QDir(outDir).filePath(
            QStringLiteral("%1.png").arg(QFileInfo(path).completeBaseName()));
        const bool ok = viewer.hasFile()
            && viewer.entryModel()->rowCount() > 0
            && settleAndGrab(engine, viewer, png, e.trophies, &why);
        printf("ui-test: %-24s %s%s\n", e.file, ok ? "PASS" : "FAIL",
               ok ? "" : qPrintable(QStringLiteral(" - ") + why));
        fflush(stdout);
        if (!ok)
            ++failures;
    }
    if (*qmlIssues > 0) {
        printf("ui-test: %d QML issue(s) logged above\n", *qmlIssues);
        ++failures;
    }
    printf("ui-test: %d failure(s)\n", failures);
    fflush(stdout);
    return failures == 0 ? 0 : 1;
}

#endif // PSGV_UI_TEST

int main(int argc, char *argv[])
{
    // CLI modes run without a display or event loop
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromUtf8(argv[i]);
        if (a == QLatin1String("--info") || a == QLatin1String("--batch-rename")
            || a == QLatin1String("--revert") || a == QLatin1String("--version")
            || a == QLatin1String("--help") || a == QLatin1String("-h"))
            return runCli(argc, argv);
    }
    return runGui(argc, argv);
}
