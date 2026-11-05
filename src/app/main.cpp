#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#include <QUrl>

#include "app/appsettings.h"
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
#define PSGV_VERSION "1.0.0"
#endif

using namespace psg;

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
            QStringLiteral("yyyyMMdd_hhmmss"))));
    const BatchOutcome out = applyBatch(plan, log);
    std::cout << "Renamed " << out.renamed.size() << ", skipped "
              << out.skipped.size() << ".";
    if (!out.logPath.isEmpty())
        std::cout << " Log: " << out.logPath.toStdString();
    std::cout << "\n";
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

    AppSettings settings;
    ImageStore images;
    auto *provider = new PsgImageProvider(&images);
    ViewerController viewer(&settings, &images);

    // game library (SQLite) + recent files, stored next to the executable
    GameLibrary gameLibrary(portable::libraryDbPath());
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

    // testing aid: open a file and/or grab the window (headless UI checks)
    const QString screenshot = qEnvironmentVariable("PSGV_SCREENSHOT");
    const QString openFile = qEnvironmentVariable("PSGV_OPEN");
    if (!openFile.isEmpty())
        viewer.openPath(QUrl::fromLocalFile(openFile));
    if (!screenshot.isEmpty()) {
        QTimer::singleShot(1500, [&engine, screenshot]() {
            const auto roots = engine.rootObjects();
            if (!roots.isEmpty()) {
                if (auto *w = qobject_cast<QQuickWindow *>(roots.first()))
                    w->grabWindow().save(screenshot);
            }
            QCoreApplication::exit(0);
        });
    }

    QObject::connect(&viewer, &ViewerController::statusMessage, &app,
                     [](const QString &) {});
    return app.exec();
}

} // namespace

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
