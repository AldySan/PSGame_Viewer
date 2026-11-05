#pragma once

#include <QString>
#include <QVariant>
#include <QStringList>

namespace psg {

// Human-readable sizes: "1.50 GB", "12.3 MB", "456 B".
QString fmtSize(quint64 n);

// Region from a PS4/PS5 Content-ID prefix ("EP0002-..." -> "Europe").
QString contentRegion(const QString &cid);

// Title ID out of a Content-ID ("UP0949-CUSA04195_00-..." -> "CUSA04195").
QString titleIdFromCid(const QString &cid);

// "CUSA04195" out of a file name, or "".
QString titleIdFromFileName(const QString &path);

// Decode the 32-bit Sony BCD firmware ints (SFO SYSTEM_VER 0x05010000 -> "5.01").
// Values that are not BCD pass through untouched.
QString fmtFw32(const QVariant &v);

// PS5 firmware ints (0x0250... -> "2.50"); falls back to fmtFw32 for 32-bit values.
QString fmtFw(const QVariant &v);

// PS4 SFO CATEGORY -> label ("gd" -> "Base Game").
QString ps4PkgType(const QString &category);

// PS4 SDK version: SDK_VER field, else PUBTOOLINFO "sdk_ver=...".
QString ps4SdkVersion(const QVariantMap &sfo);

// Region from a PS3 Title-ID prefix (BCES/NPEB/... -> "Europe").
QString ps3RegionFromTitleId(const QString &tid);

// "SCUS_973.99" -> "SCUS-97399" ("" when unparseable).
QString ps2NormalizeSerial(const QString &raw);

// Region for a PS2 serial letter / VMODE.
QString ps2Region(const QString &serial, const QString &vmode = QString());

// PS2 discs embed no title: derive one from the file name.
QString ps2TitleFromFileName(const QString &path, const QString &volumeLabel = QString());

// Short region code for file names ("Europe" -> "EU").
QString regionShort(const QString &region);

// Make a string safe as a file name component.
QString sanitizeFileNamePart(const QString &s, int limit = 120);

// "04.040.100" -> "4.40.100" (for file names).
QString normalizeVersion(const QString &ver);

// True when the string is a valid PS3 Title ID (4 letters + 5 digits).
bool isPs3TitleId(const QString &tid);

// map value as string, "-" when the key is missing.
QString valueOrDash(const QVariantMap &map, const QString &key);

} // namespace psg
