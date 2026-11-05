#include <QtTest>

#include "core/aes.h"
#include "core/detect.h"
#include "core/details.h"
#include "core/entrytree.h"
#include "core/extract.h"
#include "core/ps4/ps4pkg.h"
#include "core/ps5/exfat.h"
#include "core/ps5/ufs2.h"
#include "core/rename.h"
#include "core/sfo.h"
#include "core/trophies.h"
#include "core/util.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace psg;

namespace {

QString fixture(const QString &name)
{
    return QDir(QStringLiteral(FIXTURES_DIR)).filePath(name);
}

QString rowOf(const GameInfo &info, const QString &label)
{
    return info.row(label);
}

} // namespace

class TestPsgCore : public QObject
{
    Q_OBJECT

private slots:
    void aesNistVectors();
    void sfoParsing();
    void utilHelpers();
    void versionCompare();
    void ps4CntPkg();
    void ps5FihPkg();
    void ps3DebugPkg();
    void ps3RetailPkg();
    void ps2Iso();
    void ps3Iso();
    void ps3Folder();
    void ps5ExfatImage();
    void ps5ExfatBigPackRead();
    void ps5FfpfscExfat();
    void ps5FfpfscNestedFfpkg();
    void ps5Ffpkg();
    void ps5AppFolder();
    void ps5RetailStub();
    void ps4SplitPart();
    void trophies();
    void ucpTrophyIcons();
    void trpBoundsNoWrap();
    void ufs2TruncatedImage();
    void entryTree();
    void extraction();
    void renameEngine();
    void detailsLines();
    void detectErrors();
};

// ---------------------------------------------------------------- AES

void TestPsgCore::aesNistVectors()
{
    // FIPS-197 appendix C.1
    const QByteArray key = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
    const QByteArray pt = QByteArray::fromHex("00112233445566778899aabbccddeeff");
    const QByteArray ct = aes::ecbEncrypt(key, pt);
    QCOMPARE(ct.toHex(), QByteArray("69c4e0d86a7b0430d8cdb78070b4c55a"));

    // NIST SP 800-38A F.2.1 (CBC encrypt) and F.2.2 (CBC decrypt)
    const QByteArray key2 = QByteArray::fromHex("2b7e151628aed2a6abf7158809cf4f3c");
    const QByteArray iv = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
    const QByteArray pt2 = QByteArray::fromHex(
        "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    const QByteArray enc = aes::cbcEncrypt(key2, iv, pt2);
    QCOMPARE(enc.toHex(), QByteArray(
        "7649abac8119b246cee98e9b12e9197d"
        "5086cb9b507219ee95db113a917678b2"
        "73bed6b8e3c1743b7116e69e22229516"
        "3ff1caa1681fac09120eca307586e1a7"));
    const QByteArray dec = aes::cbcDecrypt(key2, iv, enc);
    QCOMPARE(dec, pt2);

    // invalid sizes are rejected, not crashed on
    QVERIFY(aes::ecbEncrypt(key, QByteArrayLiteral("short")).isEmpty());
    QVERIFY(aes::cbcDecrypt(key2, iv, QByteArray(8, 0)).isEmpty());
}

// ---------------------------------------------------------------- SFO

void TestPsgCore::sfoParsing()
{
    // grab the param.sfo out of the PS4 fixture through the entry reader
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps4_cnt.pkg")));
    QVERIFY(info.ok);
    int sfoIdx = -1;
    for (int i = 0; i < info.entries.size(); ++i) {
        if (info.entries.at(i).name == QLatin1String("param.sfo"))
            sfoIdx = i;
    }
    QVERIFY(sfoIdx >= 0);
    const QByteArray raw = readEntryBytes(info.entries.at(sfoIdx),
                                          info.path);
    QCOMPARE(raw.size(), 435);
    const QVariantMap sfo = parseSfo(raw);
    QVERIFY(!sfo.contains(QStringLiteral("_error")));
    QCOMPARE(sfo.value(QStringLiteral("TITLE")).toString(),
             QStringLiteral("PSGame-Viewer Sample"));
    QCOMPARE(sfo.value(QStringLiteral("TITLE_ID")).toString(),
             QStringLiteral("CUSA90001"));
    QCOMPARE(sfo.value(QStringLiteral("CATEGORY")).toString(),
             QStringLiteral("gd"));
    QCOMPARE(sfo.value(QStringLiteral("VERSION")).toString(),
             QStringLiteral("01.00"));
    QCOMPARE(sfo.value(QStringLiteral("SYSTEM_VER")).toUInt(), 0x05050000u);
    QCOMPARE(sfo.value(QStringLiteral("SDK_VER")).toUInt(), 0x09050000u);
}

// ---------------------------------------------------------------- util

void TestPsgCore::utilHelpers()
{
    QCOMPARE(fmtSize(1907), QStringLiteral("1.9 KB"));
    QCOMPARE(fmtSize(479), QStringLiteral("479 B"));
    QCOMPARE(fmtSize(quint64(135168)), QStringLiteral("135.2 KB"));

    QCOMPARE(contentRegion(QStringLiteral("UP9000-CUSA90001_00-X")),
             QStringLiteral("Americas"));
    QCOMPARE(contentRegion(QStringLiteral("EP0002-BLES90001_00-X")),
             QStringLiteral("Europe"));
    QCOMPARE(titleIdFromCid(QStringLiteral("UP0949-CUSA04195_00-BRAWL")),
             QStringLiteral("CUSA04195"));
    QCOMPARE(titleIdFromCid(QStringLiteral("junk")), QString());

    QCOMPARE(fmtFw32(0x05050000u), QStringLiteral("5.05"));
    QCOMPARE(fmtFw32(0x04530000u), QStringLiteral("4.53"));
    QCOMPARE(fmtFw32(0u), QStringLiteral("-"));
    QCOMPARE(fmtFw32(QVariant(0x03550000u)), QStringLiteral("3.55"));
    QCOMPARE(fmtFw(QVariant(quint64(0x0900) << 48)), QStringLiteral("9.00"));
    QCOMPARE(fmtFw(QVariant(QString("0x0250000000000000"))), QStringLiteral("2.50"));

    QCOMPARE(ps4PkgType(QStringLiteral("gd")), QStringLiteral("Base Game"));
    QCOMPARE(ps4PkgType(QStringLiteral("ac")), QStringLiteral("DLC"));
    QCOMPARE(ps4PkgType(QStringLiteral("gp")), QStringLiteral("Update"));
    QCOMPARE(ps4PkgType(QString()), QStringLiteral("-"));

    QCOMPARE(ps3RegionFromTitleId(QStringLiteral("BCES01234")),
             QStringLiteral("Europe"));
    QCOMPARE(ps2NormalizeSerial(QStringLiteral("SCUS_973.99")),
             QStringLiteral("SCUS-97399"));
    QCOMPARE(ps2Region(QStringLiteral("SLUS-99999")), QStringLiteral("Americas"));
    QCOMPARE(ps2Region(QString(), QStringLiteral("PAL")), QStringLiteral("Europe"));
    QCOMPARE(regionShort(QStringLiteral("Europe")), QStringLiteral("EU"));

    QCOMPARE(sanitizeFileNamePart(QStringLiteral("a<b>:c*?")),
             QStringLiteral("a b c"));
    QCOMPARE(normalizeVersion(QStringLiteral("04.040.100")),
             QStringLiteral("4.40.100"));
    QCOMPARE(normalizeVersion(QStringLiteral("01.00")),
             QStringLiteral("1.00"));
}

// ---------------------------------------------------------------- PS4

void TestPsgCore::versionCompare()
{
    QCOMPARE(parseVersionTag(QStringLiteral("v1.6.1")),
              QList<int>({1, 6, 1, 1}));
    QCOMPARE(parseVersionTag(QStringLiteral("1.19.4-pre2")),
              QList<int>({1, 19, 4, 0, 2}));
    QCOMPARE(parseVersionTag(QStringLiteral("1.19.4-pre1")),
              QList<int>({1, 19, 4, 0, 1}));
    QCOMPARE(parseVersionTag(QStringLiteral("2.0")),
              QList<int>({2, 0, 1}));

    QVERIFY(isVersionNewer(QStringLiteral("v1.6.1"),
                           QStringLiteral("1.6.0")));
    QVERIFY(isVersionNewer(QStringLiteral("1.19.4"),
                           QStringLiteral("1.19.4-pre2")));
    QVERIFY(isVersionNewer(QStringLiteral("1.19.4-pre2"),
                           QStringLiteral("1.19.4-pre1")));
    QVERIFY(!isVersionNewer(QStringLiteral("v1.0.0"),
                            QStringLiteral("1.0.0")));
    QVERIFY(!isVersionNewer(QStringLiteral("1.19.4-pre1"),
                            QStringLiteral("1.19.4")));
    QVERIFY(isVersionNewer(QStringLiteral("2.0"),
                           QStringLiteral("1.9.9")));
    QVERIFY(!isVersionNewer(QString(), QStringLiteral("0.1")));
}

void TestPsgCore::ps4CntPkg()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps4_cnt.pkg")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS4);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer Sample"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS4 (CNT metadata)"));
    QCOMPARE(rowOf(info, "Package"), QStringLiteral("OFC (Official)"));
    QCOMPARE(rowOf(info, "Entries"), QStringLiteral("3 (1 sys)"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("CUSA90001"));
    QCOMPARE(rowOf(info, "Content ID"),
             QStringLiteral("UP9000-CUSA90001_00-PSGAMEVIEWER01"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Americas"));
    QCOMPARE(rowOf(info, "Type"), QStringLiteral("Base Game"));
    QCOMPARE(rowOf(info, "Version"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("5.05"));
    QCOMPARE(rowOf(info, "SDK"), QStringLiteral("9.05"));
    QCOMPARE(rowOf(info, "Languages"), QStringLiteral("3"));
    QCOMPARE(rowOf(info, "Built"), QStringLiteral("2026-01-02"));
    QVERIFY(!info.hasRow(QStringLiteral("Passcode"))); // retail flag set

    QCOMPARE(info.entries.size(), 3);
    QCOMPARE(info.entries.at(0).id, 0x1000u);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("param.sfo"));
    QCOMPARE(info.entries.at(0).size, 435u);
    QCOMPARE(info.entries.at(1).id, 0x1200u);
    QCOMPARE(info.entries.at(1).name, QStringLiteral("icon0.png"));
    QCOMPARE(info.entries.at(1).size, 75u);
    QCOMPARE(info.entries.at(2).id, 0x200u);
    QCOMPARE(info.entries.at(2).name, QString());

    const QByteArray icon = readEntryBytes(info.entries.at(1), info.path);
    QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
}

// ---------------------------------------------------------------- PS5

void TestPsgCore::ps5FihPkg()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps5_fih.pkg")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS5 Sample"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 (finalized FIH)"));
    QCOMPARE(rowOf(info, "Package"), QStringLiteral("OFC (Official)"));
    QCOMPARE(rowOf(info, "Signature"), QStringLiteral("official"));
    QCOMPARE(rowOf(info, "PFS image"), QStringLiteral("479 B @ 0x11e0"));
    QCOMPARE(rowOf(info, "Entries"), QStringLiteral("3"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90001"));
    QCOMPARE(rowOf(info, "Content ID"),
             QStringLiteral("HP9000-PPSA90001_00-PSGAMEVIEWER01"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Asia"));
    QCOMPARE(rowOf(info, "Type"), QStringLiteral("Application (APP)"));
    QCOMPARE(rowOf(info, "Content Ver"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Master Ver"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Concept ID"), QStringLiteral("10000001"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("9.00"));
    QCOMPARE(rowOf(info, "DRM"), QStringLiteral("Paid"));
    QCOMPARE(rowOf(info, "SDK"), QStringLiteral("9.00"));

    QCOMPARE(info.entries.size(), 3);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("param.json"));
    QCOMPARE(info.entries.at(0).size, 382u);
    QCOMPARE(info.entries.at(1).name, QStringLiteral("icon0.png"));
}

void TestPsgCore::ps5RetailStub()
{
    QTemporaryDir dir;
    // part 1 of a split retail set whose part 0 is an FIH header without
    // a readable CNT -> metadata encrypted, filename carries the ids
    const QString p0 = dir.filePath(QStringLiteral(
        "HP9000-PPSA90004_00-GAMETITLE0000_0.pkg"));
    const QString p1 = dir.filePath(QStringLiteral(
        "HP9000-PPSA90004_00-GAMETITLE0000_1.pkg"));
    {
        QFile f(p0);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QByteArray fih(0x1000, 0);
        fih[0] = 0x7f;
        fih[1] = 'F';
        fih[2] = 'I';
        fih[3] = 'H';
        fih[5] = 0x80; // official
        f.write(fih);
        QFile f1(p1);
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write(QByteArray(4096, 1));
    }
    const GameInfo info = detect::openAny(p1);
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 retail (encrypted)"));
    QCOMPARE(rowOf(info, "Package"), QStringLiteral("OFC (Official)"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90004"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Asia"));
    QCOMPARE(rowOf(info, "Part"), QStringLiteral("2 of 2"));
}

void TestPsgCore::ps4SplitPart()
{
    QTemporaryDir dir;
    const QString p0 = dir.filePath(QStringLiteral("game_0.pkg"));
    const QString p1 = dir.filePath(QStringLiteral("game_1.pkg"));
    QVERIFY(QFile::copy(fixture(QStringLiteral("ps4_cnt.pkg")), p0));
    QFile(p1).open(QIODevice::WriteOnly);

    const GameInfo info = detect::openAny(p1);
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS4);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer Sample"));
    QCOMPARE(rowOf(info, "Part"), QStringLiteral("2 of 2"));
    QVERIFY(rowOf(info, "Size").contains(QStringLiteral("joined")));
    // entry bytes still come from the part-0 container
    QByteArray icon;
    for (const Entry &e : info.entries) {
        if (e.name == QLatin1String("icon0.png"))
            icon = readEntryBytes(e, info.path);
    }
    QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
}

// ---------------------------------------------------------------- PS3

void TestPsgCore::ps3DebugPkg()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps3_debug.pkg")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS3);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS3 NPDRM (debug)"));
    QCOMPARE(rowOf(info, "Content ID"),
             QStringLiteral("EP0002-BLES90001_00-PSGAMEVIEWER01"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("BLES90001"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Europe"));
    QCOMPARE(rowOf(info, "Version"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("4.53"));
    QCOMPARE(rowOf(info, "Files"), QStringLiteral("4"));

    QCOMPARE(info.entries.size(), 4);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("PARAM.SFO"));
    QCOMPARE(info.entries.at(0).size, 266u);
    QCOMPARE(info.entries.at(1).name, QStringLiteral("ICON0.PNG"));
    QCOMPARE(info.entries.at(2).name, QStringLiteral("USRDIR"));
    QCOMPARE(info.entries.at(3).name, QStringLiteral("USRDIR/DATA.BIN"));

    const QVariantMap sfo = parseSfo(
        readEntryBytes(info.entries.at(0), info.path));
    QCOMPARE(sfo.value(QStringLiteral("TITLE")).toString(),
             QStringLiteral("PSGame-Viewer PS3 Sample"));
    const QByteArray data = readEntryBytes(info.entries.at(3), info.path);
    QCOMPARE(data, QByteArrayLiteral("PSGame-Viewer payload ")
                       .repeated(8));
}

void TestPsgCore::ps3RetailPkg()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps3_retail.pkg")));
    QVERIFY(info.ok);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS3 NPDRM (retail)"));
    QCOMPARE(info.entries.size(), 4);
    const QVariantMap sfo = parseSfo(
        readEntryBytes(info.entries.at(0), info.path));
    QCOMPARE(sfo.value(QStringLiteral("TITLE")).toString(),
             QStringLiteral("PSGame-Viewer PS3 Sample"));
    QCOMPARE(readEntryBytes(info.entries.at(3), info.path),
             QByteArrayLiteral("PSGame-Viewer payload ").repeated(8));
}

void TestPsgCore::ps3Iso()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps3.iso")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS3);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS3 Disc"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS3 ISO (disc)"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("BCES90001"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Europe"));
    QCOMPARE(rowOf(info, "Category"), QStringLiteral("Disc Game (DG)"));
    QCOMPARE(rowOf(info, "Version"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "App Ver"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("3.55"));
    QCOMPARE(rowOf(info, "Parental Level"), QStringLiteral("3"));
    QCOMPARE(rowOf(info, "Update"), QStringLiteral("PS3UPDAT.PUP (4 B)"));
    QCOMPARE(rowOf(info, "Volume"), QStringLiteral("PSGAMEVIEWER"));
    QCOMPARE(info.entries.size(), 5);
    QCOMPARE(info.entries.at(1).name, QStringLiteral("PS3_GAME/PARAM.SFO"));
    const QVariantMap sfo = parseSfo(
        readEntryBytes(info.entries.at(1), info.path));
    QCOMPARE(sfo.value(QStringLiteral("TITLE_ID")).toString(),
             QStringLiteral("BCES90001"));
}

void TestPsgCore::ps3Folder()
{
    const GameInfo pkgInfo = detect::openAny(fixture(QStringLiteral("ps3_retail.pkg")));
    QVERIFY(pkgInfo.ok);
    const QByteArray sfoBytes = readEntryBytes(
        pkgInfo.entries.at(0), pkgInfo.path);
    const QByteArray iconBytes = readEntryBytes(
        pkgInfo.entries.at(1), pkgInfo.path);

    QTemporaryDir dir;
    const QString base = dir.filePath(QStringLiteral("BLES90001"));
    QVERIFY(QDir(base).mkpath(QStringLiteral("USRDIR")));
    QVERIFY(QDir(base).mkpath(QStringLiteral("TROPDIR/NPWR90001_00")));
    {
        QFile f(QDir(base).filePath(QStringLiteral("PARAM.SFO")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(sfoBytes);
        QFile i(QDir(base).filePath(QStringLiteral("ICON0.PNG")));
        QVERIFY(i.open(QIODevice::WriteOnly));
        i.write(iconBytes);
        QFile t(QDir(base).filePath(
            QStringLiteral("TROPDIR/NPWR90001_00/TROPHY.TRP")));
        QVERIFY(t.open(QIODevice::WriteOnly));
        QFile src(fixture(QStringLiteral("trophy.trp")));
        QVERIFY(src.open(QIODevice::ReadOnly));
        t.write(src.readAll());
    }

    const GameInfo info = detect::openAny(base);
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS3);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS3 folder"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("BLES90001"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Europe"));
    QCOMPARE(rowOf(info, "Version"), QStringLiteral("01.00"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("4.53"));
    QVERIFY(rowOf(info, "Size").contains(QStringLiteral("files")));
    // PNG gallery + trophy pack are listed as entries
    QVERIFY(info.entries.size() >= 2);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("ICON0.PNG"));
    bool hasTrophy = false;
    for (const Entry &e : info.entries)
        hasTrophy = hasTrophy || e.name.endsWith(QLatin1String("TROPHY.TRP"));
    QVERIFY(hasTrophy);
}

// ---------------------------------------------------------------- PS2

void TestPsgCore::ps2Iso()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps2.iso")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS2);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS2 ISO (disc)"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("SLUS-99999"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Americas"));
    QCOMPARE(rowOf(info, "Version"), QStringLiteral("v1.00"));
    QCOMPARE(rowOf(info, "VMODE"), QStringLiteral("NTSC"));
    QCOMPARE(rowOf(info, "Boot"), QStringLiteral("SLUS_99999"));
    QCOMPARE(rowOf(info, "Volume"), QStringLiteral("PSGAMEVIEWER"));
    QCOMPARE(info.entries.size(), 2);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("SYSTEM.CNF"));
    const QByteArray cnf = readEntryBytes(info.entries.at(0), info.path);
    QVERIFY(cnf.startsWith(QByteArrayLiteral("BOOT2 = cdrom0:")));
}

// ---------------------------------------------------------------- PS5 images

void TestPsgCore::ps5ExfatImage()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps5.exfat")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS5 Image"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 exFAT image"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90002"));
    QCOMPARE(rowOf(info, "Region"), QStringLiteral("Americas"));
    QCOMPARE(rowOf(info, "Type"), QStringLiteral("Application (APP)"));
    QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("9.00"));
    QCOMPARE(rowOf(info, "DRM"), QStringLiteral("Paid"));
    QCOMPARE(rowOf(info, "SDK"), QStringLiteral("9.00"));
    QCOMPARE(info.entries.size(), 1);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("icon0.png"));
    const QByteArray icon = readEntryBytes(info.entries.at(0), info.path);
    QCOMPARE(icon.size(), 75);
    QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
}

void TestPsgCore::ps5ExfatBigPackRead()
{
    // regression test for the exFAT payload read cap: a trophy pack above
    // the old 5 MB limit (admitted by the 300 MB entry filter) must read
    // back completely through the lazy ExfatChain source
    const GameInfo info =
        detect::openAny(fixture(QStringLiteral("ps5_bigpack.exfat")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);

    const Entry *pack = nullptr;
    for (const Entry &e : info.entries) {
        if (e.name.endsWith(QLatin1String(".trp"), Qt::CaseInsensitive))
            pack = &e;
    }
    QVERIFY(pack != nullptr);
    QCOMPARE(pack->size, quint64(6) << 20);

    const QByteArray data = readEntryBytes(*pack, info.path, 300000000);
    QCOMPARE(quint64(data.size()), quint64(6) << 20);
    // the fixture keeps a valid TRP at the head of the padded pack
    QVERIFY(data.left(4) == QByteArrayLiteral("\xDC\xA2\x4D\x00"));
    QVERIFY(data.right(4) == QByteArrayLiteral("\x00\x00\x00\x00"));
}

void TestPsgCore::ps5FfpfscExfat()
{
    // compressed and raw payloads take the same paths through the code
    for (const char *name : {"ffpfsc_exfat.ffpfsc", "ffpfsc_raw.ffpfsc"}) {
        const GameInfo info =
            detect::openAny(fixture(QLatin1String(name)));
        QVERIFY(info.ok);
        QCOMPARE(info.platform, Platform::PS5);
        QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS5 Image"));
        QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 ffpfsc image"));
        QCOMPARE(rowOf(info, "Inner file"), QStringLiteral("LOOP00002.exfat"));
        QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90002"));
        QCOMPARE(rowOf(info, "Min. System"), QStringLiteral("9.00"));

        // icon0.png is cached inline at parse time, the trophy pack stays
        // a lazy FAT-chain read resolved through the PFS view
        QCOMPARE(info.entries.size(), 2);
        QCOMPARE(info.entries.at(0).name, QStringLiteral("icon0.png"));
        QCOMPARE(info.entries.at(0).source, Entry::Source::Cached);
        QCOMPARE(info.entries.at(1).name,
                 QStringLiteral("sce_sys/trophy/trophy00.trp"));
        QCOMPARE(info.entries.at(1).source, Entry::Source::ExfatChain);

        const QByteArray icon = readEntryBytes(info.entries.at(0), info.path);
        QCOMPARE(icon.size(), 75);
        QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));

        QFile trp(fixture(QStringLiteral("trophy.trp")));
        QVERIFY(trp.open(QIODevice::ReadOnly));
        QCOMPARE(readEntryBytes(info.entries.at(1), info.path), trp.readAll());
    }
}

void TestPsgCore::ps5FfpfscNestedFfpkg()
{
    const GameInfo info =
        detect::openAny(fixture(QStringLiteral("ffpfsc_ffpkg.ffpfsc")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS5 Image"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 ffpfsc image"));
    QCOMPARE(rowOf(info, "Inner file"),
             QStringLiteral("PPSA90003-app.ffpkg"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90003"));
    QCOMPARE(info.entries.size(), 2); // icon0 + icon1, both cached
    const QByteArray icon = readEntryBytes(info.entries.at(0), info.path);
    QCOMPARE(icon.size(), 75);
    QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
}

void TestPsgCore::ps5Ffpkg()
{
    const GameInfo info =
        detect::openAny(fixture(QStringLiteral("ffpkg.ffpkg")));
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(info.title, QStringLiteral("PSGame-Viewer PS5 Image"));
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 ffpkg image"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90003"));
    QVERIFY(!info.hasRow(QStringLiteral("Inner file")));
    QCOMPARE(info.entries.size(), 2);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("icon0.png"));
    const QByteArray icon = readEntryBytes(info.entries.at(0), info.path);
    QCOMPARE(icon.size(), 75);
    QVERIFY(icon.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
}

void TestPsgCore::ps5AppFolder()
{
    const GameInfo fih = detect::openAny(fixture(QStringLiteral("ps5_fih.pkg")));
    QVERIFY(fih.ok);
    QByteArray pj, icon;
    for (const Entry &e : fih.entries) {
        if (e.name == QLatin1String("param.json"))
            pj = readEntryBytes(e, fih.path);
        else if (e.name == QLatin1String("icon0.png"))
            icon = readEntryBytes(e, fih.path);
    }
    QVERIFY(!pj.isEmpty());

    QTemporaryDir dir;
    const QString base = dir.filePath(QStringLiteral("PPSA90003"));
    QVERIFY(QDir(base).mkpath(QStringLiteral("sce_sys")));
    {
        QFile f(QDir(base).filePath(QStringLiteral("sce_sys/param.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(pj);
        QFile i(QDir(base).filePath(QStringLiteral("sce_sys/icon0.png")));
        QVERIFY(i.open(QIODevice::WriteOnly));
        i.write(icon);
    }
    const GameInfo info = detect::openAny(base);
    QVERIFY(info.ok);
    QCOMPARE(info.platform, Platform::PS5);
    QCOMPARE(rowOf(info, "Platform"), QStringLiteral("PS5 app folder"));
    QCOMPARE(rowOf(info, "Title ID"), QStringLiteral("PPSA90001"));
    QCOMPARE(info.entries.size(), 1);
    QCOMPARE(info.entries.at(0).name, QStringLiteral("icon0.png"));
    QVERIFY(info.entries.at(0).source == Entry::Source::LocalFile);
}

// ---------------------------------------------------------------- trophies

void TestPsgCore::trophies()
{
    QFile f(fixture(QStringLiteral("trophy.trp")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray data = f.readAll();

    const QList<trophies::TrpInner> inner = trophies::parseTrp(data);
    QCOMPARE(inner.size(), 3);
    QCOMPARE(inner.at(0).name, QStringLiteral("TROPCONF.SFM"));
    QCOMPARE(inner.at(1).name, QStringLiteral("ICON0.PNG"));
    QCOMPARE(inner.at(2).name, QStringLiteral("ICON1.PNG"));

    const QByteArray esfm = data.mid(int(inner.at(0).offset),
                                     int(inner.at(0).size));
    const QByteArray xml = trophies::decryptEsfm(esfm,
                                                 QStringLiteral("NPWR99999_00"));
    QVERIFY(!xml.isEmpty());
    QVERIFY(xml.contains("<trophyconf>"));

    const QList<trophies::Trophy> list = trophies::parseTrophyXml(xml);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).name, QStringLiteral("First Steps"));
    QCOMPARE(list.at(0).grade, QStringLiteral("P"));
    QCOMPARE(list.at(0).hidden, false);
    QCOMPARE(list.at(1).name, QStringLiteral("Collector"));
    QCOMPARE(list.at(1).grade, QStringLiteral("G"));

    // wrong NP comm ID fails cleanly
    QVERIFY(trophies::decryptEsfm(esfm, QStringLiteral("NPWR00001_00")).isEmpty());

    // brute force finds the ID from the blob alone
    const QPair<QString, QByteArray> hit =
        trophies::bruteforceNpid(esfm, 99990, 100000);
    QCOMPARE(hit.first, QStringLiteral("NPWR99999_00"));
    QVERIFY(hit.second.contains("<trophyconf>"));

    const QList<trophies::CarvedIcon> icons = trophies::carveTrpIcons(data);
    QCOMPARE(icons.size(), 2);
    QCOMPARE(icons.at(0).width, 240);
    QCOMPARE(icons.at(0).height, 240);
    QCOMPARE(icons.at(1).width, 4);
}

void TestPsgCore::ucpTrophyIcons()
{
    QFile f(fixture(QStringLiteral("trophy.ucp")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray data = f.readAll();

    const trophies::UcpResult res = trophies::ucpTrophies(data);
    QCOMPARE(res.npCommId, QStringLiteral("NPWR90000_00"));
    QCOMPARE(res.title, QStringLiteral("PSGame-Viewer UCP Sample"));
    QCOMPARE(res.trophies.size(), 12);
    QCOMPARE(res.trophies.at(0).name, QStringLiteral("Trophy 1"));
    QCOMPARE(res.trophies.at(11).name, QStringLiteral("Trophy 12"));
    QCOMPARE(res.icons.size(), 12);

    // display order is the natural order of the icon names: the blob
    // sizes grow with the trophy number (trophy_2.png < trophy_10.png,
    // which a plain lexical sort would get wrong)
    const QList<QByteArray> sorted = trophies::ucpIconsSorted(res);
    QCOMPARE(sorted.size(), 12);
    for (int i = 1; i < sorted.size(); ++i) {
        QVERIFY(sorted.at(i - 1).startsWith(
            QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
        QVERIFY(sorted.at(i - 1).size() < sorted.at(i).size());
    }
}

void TestPsgCore::trpBoundsNoWrap()
{
    QFile f(fixture(QStringLiteral("trophy.trp")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    // entry 1 starts at 0x38 + 0x40; patch its offset so that
    // offset + size wraps around 2^64 and would pass an
    // "offset + size > data.size()" check
    const int offField = 0x38 + 0x40 + 8;
    const quint64 wrap = Q_UINT64_C(0xFFFFFFFFFFFFF000);
    for (int i = 0; i < 8; ++i)
        data[offField + i] = char((wrap >> (56 - 8 * i)) & 0xFF);
    QVERIFY(trophies::parseTrp(data).isEmpty());

    // sanity: the untouched fixture still parses
    QFile g(fixture(QStringLiteral("trophy.trp")));
    QVERIFY(g.open(QIODevice::ReadOnly));
    QCOMPARE(trophies::parseTrp(g.readAll()).size(), 3);
}

void TestPsgCore::ufs2TruncatedImage()
{
    QFile f(fixture(QStringLiteral("ffpkg.ffpkg")));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray whole = f.readAll();

    // cut the image short: reads must fail cleanly (empty or partial
    // data), never wrap around block ends
    QByteArray trunc(whole.constData(), whole.size() * 2 / 3);
    QBuffer buf(&trunc);
    buf.open(QIODevice::ReadOnly);
    bool ok = false;
    const ps5::Ufs2File ufs(&buf, QStringLiteral("truncated"), &ok);
    if (!ok)
        QSKIP("truncated image rejected outright");
    ps5::Ufs2File::Found found;
    if (ufs.find({QStringLiteral("icon0.png")}, &found)) {
        const QByteArray bytes = ufs.readFile(found);
        QVERIFY(bytes.size() <= int(found.size));
        QVERIFY(bytes == QByteArray()
                || bytes.startsWith(QByteArrayLiteral("\x89PNG\r\n\x1a\n")));
    }
    // the root directory is still listable or cleanly empty
    const QList<ps5::Ufs2File::DirEntry> root = ufs.listDir(2);
    QVERIFY(root.size() <= 8);
}

// ---------------------------------------------------------------- tree

void TestPsgCore::entryTree()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps3.iso")));
    QVERIFY(info.ok);
    const QList<EntryTreeNode> tree = buildEntryTree(info.entries);
    QVERIFY(tree.size() >= 4);
    // root has two folders (PS3_GAME, PS3_UPDATE) + one file (PS3_DISC.SFB)
    int ps3Game = -1, ps3Update = -1;
    for (int ci : tree.at(0).childDirs) {
        if (tree.at(ci).name == QLatin1String("PS3_GAME"))
            ps3Game = ci;
        if (tree.at(ci).name == QLatin1String("PS3_UPDATE"))
            ps3Update = ci;
    }
    QVERIFY(ps3Game >= 0);
    QVERIFY(ps3Update >= 0);
    QCOMPARE(tree.at(0).files.size(), 1);
    QCOMPARE(tree.at(0).files.at(0).name, QStringLiteral("PS3_DISC.SFB"));
    // PS3_GAME has USRDIR below and 2 direct files
    QCOMPARE(tree.at(ps3Game).files.size(), 2);
    int usrdir = -1;
    for (int ci : tree.at(ps3Game).childDirs) {
        if (tree.at(ci).name == QLatin1String("USRDIR"))
            usrdir = ci;
    }
    QVERIFY(usrdir >= 0);
    QCOMPARE(tree.at(usrdir).files.size(), 1);
    QCOMPARE(tree.at(usrdir).files.at(0).name, QStringLiteral("EBOOT.BIN"));
    QCOMPARE(tree.at(ps3Update).files.size(), 1);
    // folder sizes are summed
    QCOMPARE(tree.at(ps3Update).size, 4u);
}

// ---------------------------------------------------------------- extract

void TestPsgCore::extraction()
{
    QTemporaryDir dir;
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps3_retail.pkg")));
    QVERIFY(info.ok);
    const ExtractResult r = extractEntries(info.entries, info.path,
                                           dir.path());
    QCOMPARE(r.ok, 3); // zero-size entries (the USRDIR stub) are skipped
    QCOMPARE(r.fail, 0);
    const QString sfoPath = dir.filePath(QStringLiteral("PARAM.SFO"));
    QVERIFY(QFile::exists(sfoPath));
    QFile sfo(sfoPath);
    QVERIFY(sfo.open(QIODevice::ReadOnly));
    const QVariantMap sfoMap = parseSfo(sfo.readAll());
    QCOMPARE(sfoMap.value(QStringLiteral("TITLE")).toString(),
             QStringLiteral("PSGame-Viewer PS3 Sample"));
    QFile data(dir.filePath(QStringLiteral("USRDIR/DATA.BIN")));
    QVERIFY(data.open(QIODevice::ReadOnly));
    QCOMPARE(data.readAll(), QByteArrayLiteral("PSGame-Viewer payload ")
                                  .repeated(8));

    // GP4 project for the extracted PS4 container
    QTemporaryDir dir2;
    const GameInfo ps4 = detect::openAny(fixture(QStringLiteral("ps4_cnt.pkg")));
    QVERIFY(ps4.ok);
    extractEntries(ps4.entries, ps4.path, dir2.path());
    const QString gp4 = writeGp4(ps4, dir2.path(),
                                 dir2.filePath(QStringLiteral("project.gp4")));
    QVERIFY(QFile::exists(gp4));
    QFile g(gp4);
    QVERIFY(g.open(QIODevice::ReadOnly));
    const QString xml = QString::fromUtf8(g.readAll());
    QVERIFY(xml.contains(QStringLiteral("<psproject fmt=\"gp4\"")));
    QVERIFY(xml.contains(QStringLiteral("content_id=\"UP9000-CUSA90001_00-PSGAMEVIEWER01\"")));
    QVERIFY(xml.contains(QStringLiteral("targ_path=\"param.sfo\"")));
}

// ---------------------------------------------------------------- rename

void TestPsgCore::renameEngine()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps4_cnt.pkg")));
    QVERIFY(info.ok);

    QCOMPARE(buildCleanName(info),
             QStringLiteral("PSGame-Viewer Sample - CUSA90001 - v1.00 - US"));
    RenameParts parts;
    parts.title = false;
    QCOMPARE(buildCleanName(info, parts),
             QStringLiteral("CUSA90001 - v1.00 - US"));

    const FormatResult r1 = formatName(info, QStringLiteral("{orig} [{tid}]"),
                                        QStringLiteral("my-game"));
    QCOMPARE(r1.base, QStringLiteral("my-game [CUSA90001]"));
    QVERIFY(r1.missing.isEmpty());

    const FormatResult r2 = formatName(info, QStringLiteral("{title} - {version}"));
    QCOMPARE(r2.base, QStringLiteral("PSGame-Viewer Sample - 1.00"));

    // update/DLC suffix from the Type row
    GameInfo dlc = info;
    dlc.setRow(QStringLiteral("Type"), QStringLiteral("DLC"));
    QCOMPARE(buildCleanName(dlc),
             QStringLiteral("PSGame-Viewer Sample - CUSA90001 - v1.00 - US - DLC"));

    // missing tokens are reported, empty ones dropped with separators
    const FormatResult r3 = formatName(
        dlc, QStringLiteral("{title} - {tid} - {type}"));
    QCOMPARE(r3.base, QStringLiteral("PSGame-Viewer Sample - CUSA90001 - DLC"));
    GameInfo noType = info;
    noType.setRow(QStringLiteral("Type"), QStringLiteral("Base Game"));
    const FormatResult r4 = formatName(
        noType, QStringLiteral("{title} - {type}"));
    QCOMPARE(r4.base, QStringLiteral("PSGame-Viewer Sample"));
    QVERIFY(r4.missing.contains(QStringLiteral("TYPE")));

    // presets exist and the batch plan handles split sets
    const QList<RenamePreset> presets = renamePresets();
    QCOMPARE(presets.size(), 4);
    QCOMPARE(presets.at(1).pattern, QStringLiteral("{tid}"));

    QTemporaryDir dir;
    const QString p0 = dir.filePath(QStringLiteral("game_0.pkg"));
    const QString p1 = dir.filePath(QStringLiteral("game_1.pkg"));
    QVERIFY(QFile::copy(fixture(QStringLiteral("ps4_cnt.pkg")), p0));
    QFile(p1).open(QIODevice::WriteOnly);
    const QList<BatchItem> plan = previewBatch({p0, p1});
    QCOMPARE(plan.size(), 1); // split set stays together
    QVERIFY(plan.at(0).ok);
    QCOMPARE(plan.at(0).newFiles.size(), 2);
    QVERIFY(plan.at(0).newFiles.at(0).endsWith(
        QStringLiteral("PSGame-Viewer Sample - CUSA90001 - v1.00 - US_0.pkg")));
    QVERIFY(plan.at(0).newFiles.at(1).endsWith(
        QStringLiteral("PSGame-Viewer Sample - CUSA90001 - v1.00 - US_1.pkg")));

    // apply + revert roundtrip via the log
    const QString log = dir.filePath(QStringLiteral("batch.log"));
    const BatchOutcome applied = applyBatch(plan, log);
    QCOMPARE(applied.renamed.size(), 2);
    QVERIFY(applied.logError.isEmpty());
    QVERIFY(!QFile::exists(p0));
    QVERIFY(QFile::exists(plan.at(0).newFiles.at(0)));
    const QList<QPair<QString, QString>> pairs = parseRevertLog(log);
    QCOMPARE(pairs.size(), 2);
    const BatchOutcome reverted = applyRevert(pairs);
    QCOMPARE(reverted.renamed.size(), 2);
    QVERIFY(QFile::exists(p0));
    QVERIFY(QFile::exists(p1));

    // an unwritable log location is reported, not silently dropped
    const QString p2 = dir.filePath(QStringLiteral("solo.pkg"));
    QVERIFY(QFile::copy(fixture(QStringLiteral("ps4_cnt.pkg")), p2));
    const QList<BatchItem> plan2 = previewBatch({p2});
    QVERIFY(plan2.at(0).ok);
    QCOMPARE(plan2.at(0).files.size(), 1);
    const BatchOutcome failed = applyBatch(
        plan2, dir.filePath(QStringLiteral("no/such/dir/batch.log")));
    QCOMPARE(failed.renamed.size(), 1);
    QVERIFY(!failed.logError.isEmpty());
    QVERIFY(failed.logPath.isEmpty());
}

// ---------------------------------------------------------------- details

void TestPsgCore::detailsLines()
{
    const GameInfo info = detect::openAny(fixture(QStringLiteral("ps4_cnt.pkg")));
    QVERIFY(info.ok);
    const QStringList curated = curatedMetaLines(info.platform, info.meta);
    QVERIFY(curated.contains(QStringLiteral("TITLE = PSGame-Viewer Sample")));
    QVERIFY(curated.contains(QStringLiteral("TITLE_ID = CUSA90001")));
    QVERIFY(curated.contains(QStringLiteral("CATEGORY = gd")));
    const QStringList full = fullMetaLines(info.meta);
    QVERIFY(full.size() >= 9);
    QVERIFY(full.contains(QStringLiteral("PUBTOOLINFO = c_date=20260102030405 sdk_ver=09050000")));

    const GameInfo fih = detect::openAny(fixture(QStringLiteral("ps5_fih.pkg")));
    QVERIFY(fih.ok);
    const QStringList ps5 = curatedMetaLines(fih.platform, fih.meta);
    QVERIFY(ps5.contains(QStringLiteral("Title = PSGame-Viewer PS5 Sample")));
    QVERIFY(ps5.contains(QStringLiteral("Title ID = PPSA90001")));
    QVERIFY(ps5.contains(QStringLiteral("Min. System = 9.00")));
}

// ---------------------------------------------------------------- errors

void TestPsgCore::detectErrors()
{
    QTemporaryDir dir;
    // unknown magic
    const QString junk = dir.filePath(QStringLiteral("junk.pkg"));
    {
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArrayLiteral("NOPE"));
    }
    const GameInfo bad = detect::openAny(junk);
    QVERIFY(!bad.ok);
    QVERIFY(bad.error.contains(QStringLiteral("unknown magic")));

    // broken containers report a clear reason
    const QString pfsc = dir.filePath(QStringLiteral("x.ffpfsc"));
    {
        QFile f(pfsc);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(128, 'x'));
    }
    QVERIFY(detect::openAny(pfsc).error.contains(QStringLiteral("not a PFS image")));
    const QString ffpkg = dir.filePath(QStringLiteral("x.ffpkg"));
    {
        QFile f(ffpkg);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArrayLiteral("NOPE"));
    }
    QVERIFY(detect::openAny(ffpkg).error.contains(QStringLiteral("not a UFS2 image")));

    // missing file
    QVERIFY(!detect::openAny(dir.filePath(QStringLiteral("nope.pkg"))).ok);
}

QTEST_MAIN(TestPsgCore)
#include "tst_psgcore.moc"
