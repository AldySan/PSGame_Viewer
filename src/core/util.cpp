#include "util.h"

#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

namespace psg {
namespace {

const QHash<QString, QString> &regionNames()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("EP"), QStringLiteral("Europe")},
        {QStringLiteral("UP"), QStringLiteral("Americas")},
        {QStringLiteral("JP"), QStringLiteral("Japan")},
        {QStringLiteral("HP"), QStringLiteral("Asia")},
    };
    return map;
}

const QHash<QString, QString> &regionShorts()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("Europe"), QStringLiteral("EU")},
        {QStringLiteral("Americas"), QStringLiteral("US")},
        {QStringLiteral("Japan"), QStringLiteral("JP")},
        {QStringLiteral("Asia"), QStringLiteral("AS")},
    };
    return map;
}

const QHash<QString, QString> &ps3TidRegions()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("NPEB"), QStringLiteral("Europe")},
        {QStringLiteral("BCES"), QStringLiteral("Europe")},
        {QStringLiteral("BLES"), QStringLiteral("Europe")},
        {QStringLiteral("NPHB"), QStringLiteral("Asia")},
        {QStringLiteral("BCAS"), QStringLiteral("Asia")},
        {QStringLiteral("BLAS"), QStringLiteral("Asia")},
        {QStringLiteral("NPJB"), QStringLiteral("Japan")},
        {QStringLiteral("BCJS"), QStringLiteral("Japan")},
        {QStringLiteral("BLJS"), QStringLiteral("Japan")},
        {QStringLiteral("NPUB"), QStringLiteral("Americas")},
        {QStringLiteral("BCUS"), QStringLiteral("Americas")},
        {QStringLiteral("BLUS"), QStringLiteral("Americas")},
    };
    return map;
}

const QHash<QString, QString> &ps2SerialRegions()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("U"), QStringLiteral("Americas")},
        {QStringLiteral("E"), QStringLiteral("Europe")},
        {QStringLiteral("J"), QStringLiteral("Japan")},
        {QStringLiteral("P"), QStringLiteral("Japan")},
        {QStringLiteral("A"), QStringLiteral("Asia")},
        {QStringLiteral("H"), QStringLiteral("Asia")},
        {QStringLiteral("K"), QStringLiteral("Korea")},
        {QStringLiteral("C"), QStringLiteral("China")},
    };
    return map;
}

const QHash<QString, QString> &ps4CategoryTypes()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("gd"), QStringLiteral("Base Game")},
        {QStringLiteral("ac"), QStringLiteral("DLC")},
        {QStringLiteral("gp"), QStringLiteral("Update")},
    };
    return map;
}

} // namespace

QString fmtSize(quint64 n)
{
    const double d = double(n);
    if (d >= 1e9)
        return QString::number(d / 1e9, 'f', 2) + QStringLiteral(" GB");
    if (d >= 1e6)
        return QString::number(d / 1e6, 'f', 1) + QStringLiteral(" MB");
    if (d >= 1e3)
        return QString::number(d / 1e3, 'f', 1) + QStringLiteral(" KB");
    return QString::number(n) + QStringLiteral(" B");
}

QString contentRegion(const QString &cid)
{
    const QString prefix = cid.split(u'-').value(0).left(2).toUpper();
    return regionNames().value(prefix, prefix.isEmpty() ? QStringLiteral("-") : prefix);
}

QString titleIdFromCid(const QString &cid)
{
    const QStringList parts = cid.split(u'-');
    if (parts.size() < 2)
        return QString();
    return parts.at(1).split(u'_').value(0).trimmed().toUpper();
}

QString titleIdFromFileName(const QString &path)
{
    static const QRegularExpression re(
        QStringLiteral("CUSA\\d{5}"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(QFileInfo(path).fileName());
    if (!m.hasMatch())
        return QString();
    return m.captured(0).toUpper();
}

namespace {

// QVariant -> u64 for firmware ints, with the Python `int(s, 0)` semantics
// for strings ("0x0250...", "525"). ok=false when not numeric.
bool fwToInt(const QVariant &v, quint64 *out)
{
    if (!v.isValid() || v.isNull())
        return false;
    switch (v.typeId()) {
    case QMetaType::UInt:
    case QMetaType::ULongLong:
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UShort:
    case QMetaType::Short:
    case QMetaType::Char:
    case QMetaType::UChar:
    case QMetaType::Bool:
        *out = v.toULongLong();
        return true;
    case QMetaType::Double:
    case QMetaType::Float:
        *out = quint64(v.toDouble());
        return true;
    default:
        break;
    }
    const QString s = v.toString().trimmed();
    if (s.isEmpty() || s == QLatin1String("-"))
        return false;
    bool ok = false;
    const quint64 n = s.toULongLong(&ok, 0);
    if (!ok)
        return false;
    *out = n;
    return true;
}

bool isDecimalString(const QString &s)
{
    if (s.isEmpty())
        return false;
    for (const QChar c : s) {
        if (!c.isDigit())
            return false;
    }
    return true;
}

} // namespace

QString fmtFw32(const QVariant &v)
{
    if (!v.isValid() || v.isNull())
        return QStringLiteral("-");
    quint64 n = 0;
    const bool isInt = fwToInt(v, &n);
    if (!isInt) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty())
            return QStringLiteral("-");
        // not numeric: already dotted ("05.050.000", "4.80", ...) -> pass through
        return s;
    }
    if (n == 0)
        return QStringLiteral("-");
    if (n < 0x01000000ull || n > 0xFFFFFFFFull)
        return v.toString();
    const uint hi = uint(n >> 24) & 0xFF;
    const uint lo = uint(n >> 16) & 0xFF;
    const auto nibbleOk = [](uint x) {
        return ((x >> 4) <= 9) && ((x & 0xF) <= 9);
    };
    if (!nibbleOk(hi) || !nibbleOk(lo))
        return v.toString(); // not BCD -> don't invent a version
    QString major = QString::number(hi, 16);
    if (major.size() > 1 && major.startsWith(u'0'))
        major.remove(0, 1);
    return major + QLatin1Char('.') + QString::number(lo, 16).rightJustified(2, u'0');
}

QString fmtFw(const QVariant &v)
{
    if (!v.isValid() || v.isNull() || v.toString().isEmpty())
        return QStringLiteral("-");
    quint64 n = 0;
    if (!fwToInt(v, &n))
        return v.toString();
    const quint64 top = (n >> 48) & 0xFFFF;
    if (top == 0)
        return fmtFw32(v);
    return QString::number((top >> 8) & 0xFF, 16).toUpper()
         + QLatin1Char('.')
         + QString::number(top & 0xFF, 16).toUpper().rightJustified(2, u'0');
}

QString ps4PkgType(const QString &category)
{
    if (category.isEmpty())
        return QStringLiteral("-");
    const QString lab = ps4CategoryTypes().value(category.toLower());
    return lab.isEmpty() ? category : lab;
}

QString ps4SdkVersion(const QVariantMap &sfo)
{
    for (const QString &key : {QStringLiteral("SDK_VER"), QStringLiteral("SDK_VERSION")}) {
        const QString v = [&]() {
            if (!sfo.contains(key))
                return QString();
            // PUBTOOLINFO carries the same number as bare hex ("05050000"),
            // so SDK_VER strings are parsed base 16 as well.
            const QVariant raw = sfo.value(key);
            if (raw.typeId() == QMetaType::UInt || raw.typeId() == QMetaType::ULongLong)
                return fmtFw32(raw);
            const QString s = raw.toString().trimmed();
            if (s.isEmpty() || s == QLatin1String("-"))
                return QString();
            bool ok = false;
            const quint64 n = s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)
                ? s.toULongLong(&ok, 16)
                : (isDecimalString(s) && s.size() <= 8)
                    ? s.toULongLong(&ok, 16)
                    : (s.toULongLong(&ok, 0), quint64(0));
            if (!ok)
                return QString();
            return fmtFw32(QVariant(quint64(n)));
        }();
        if (!v.isEmpty() && v != QLatin1String("-"))
            return v;
    }
    static const QRegularExpression re(
        QStringLiteral("sdk_ver\\s*=\\s*([0-9A-Fa-f]+)"));
    const QRegularExpressionMatch m = re.match(sfo.value(QStringLiteral("PUBTOOLINFO")).toString());
    if (m.hasMatch()) {
        bool ok = false;
        const quint64 n = m.captured(1).toULongLong(&ok, 16);
        if (ok) {
            const QString v = fmtFw32(QVariant(n));
            if (!v.isEmpty() && v != QLatin1String("-"))
                return v;
        }
    }
    return QString();
}

QString ps3RegionFromTitleId(const QString &tid)
{
    return ps3TidRegions().value(tid.left(4).toUpper(), QStringLiteral("-"));
}

QString ps2NormalizeSerial(const QString &raw)
{
    static const QRegularExpression re(
        QStringLiteral("([A-Za-z]{4})[_-]?(\\d{3})[._]?(\\d{2})"));
    const QRegularExpressionMatch m = re.match(raw);
    if (!m.hasMatch())
        return QString();
    return m.captured(1).toUpper() + QLatin1Char('-') + m.captured(2) + m.captured(3);
}

QString ps2Region(const QString &serial, const QString &vmode)
{
    const QString s = serial.trimmed().toUpper();
    if (s.size() >= 3) {
        const QString hit = ps2SerialRegions().value(QString(s.at(2)));
        if (!hit.isEmpty())
            return hit;
    }
    const QString v = vmode.trimmed().toUpper();
    if (v.contains(QLatin1String("PAL")))
        return QStringLiteral("Europe");
    if (v.contains(QLatin1String("NTSC")))
        return QStringLiteral("Americas / Japan (NTSC)");
    return QStringLiteral("-");
}

QString ps2TitleFromFileName(const QString &path, const QString &volumeLabel)
{
    const QString stem = QFileInfo(path).completeBaseName().trimmed();
    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z]{4}[-_ ]?\\d{3}[._]?\\d{2}\\s*[-_.\\s]*"));
    QString t = stem;
    t.remove(re);
    while (t.startsWith(u' ') || t.startsWith(u'-') || t.startsWith(u'_') || t.startsWith(u'.'))
        t.remove(0, 1);
    while (t.endsWith(u' ') || t.endsWith(u'-') || t.endsWith(u'_') || t.endsWith(u'.'))
        t.chop(1);
    if (!t.isEmpty())
        return t;
    if (!volumeLabel.isEmpty())
        return volumeLabel;
    return stem;
}

QString regionShort(const QString &region)
{
    const QString s = regionShorts().value(region);
    return s.isEmpty() ? region : s;
}

QString sanitizeFileNamePart(const QString &s, int limit)
{
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        const bool forbidden = QString::fromUtf8("<>:\"/\\|?*").contains(c);
        if (forbidden || c.unicode() < 32)
            out.append(u' ');
        else
            out.append(c);
    }
    out = out.simplified();
    while (out.startsWith(u'.') || out.startsWith(u' '))
        out.remove(0, 1);
    while (out.endsWith(u'.') || out.endsWith(u' '))
        out.chop(1);
    if (out.size() > limit) {
        out = out.left(limit);
        while (out.endsWith(u'.') || out.endsWith(u' '))
            out.chop(1);
    }
    return out;
}

QString normalizeVersion(const QString &ver)
{
    const QStringList segs = ver.trimmed().split(u'.');
    QStringList out;
    for (int i = 0; i < segs.size(); ++i) {
        QString s = segs.at(i).trimmed();
        const bool digits = isDecimalString(s);
        if (digits && (i == 0 || s.size() > 2))
            s = QString::number(s.toULongLong());
        if (!s.isEmpty())
            out.append(s);
    }
    return out.join(u'.');
}

bool isPs3TitleId(const QString &tid)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z]{4}\\d{5}$"));
    return re.match(tid.trimmed().toUpper()).hasMatch();
}

QString valueOrDash(const QVariantMap &map, const QString &key)
{
    if (!map.contains(key))
        return QStringLiteral("-");
    const QVariant v = map.value(key);
    if (v.typeId() == QMetaType::QString && v.toString().isEmpty())
        return QStringLiteral("-");
    return v.toString();
}

namespace {

// Leading digits of one dot-separated part ("" and "rc3" -> 0).
int leadingDigits(const QString &part)
{
    int end = 0;
    while (end < part.size() && part.at(end).isDigit())
        ++end;
    bool ok = false;
    const int v = part.left(end).toInt(&ok);
    return ok ? v : 0;
}

} // namespace

QList<int> parseVersionTag(const QString &tag)
{
    QString s = tag.trimmed();
    if (s.startsWith(u'v') || s.startsWith(u'V'))
        s = s.mid(1);
    const int dash = s.indexOf(u'-');
    const QString core = dash < 0 ? s : s.left(dash);
    const QString pre = dash < 0 ? QString() : s.mid(dash + 1);

    QList<int> out;
    for (const QString &part : core.split(u'.'))
        out.append(leadingDigits(part));
    if (out.isEmpty())
        out.append(0);

    if (dash < 0) {
        // final releases sort above their pre-releases
        out.append(1);
        return out;
    }
    out.append(0);
    QList<int> preNums;
    int start = -1;
    for (int i = 0; i <= pre.size(); ++i) {
        const bool digit = i < pre.size() && pre.at(i).isDigit();
        if (digit && start < 0)
            start = i;
        if (!digit && start >= 0) {
            bool ok = false;
            const int v = pre.mid(start, i - start).toInt(&ok);
            if (ok)
                preNums.append(v);
            start = -1;
        }
    }
    if (preNums.isEmpty())
        preNums.append(0);
    out += preNums;
    return out;
}

bool isVersionNewer(const QString &latest, const QString &current)
{
    QList<int> a = parseVersionTag(latest);
    QList<int> b = parseVersionTag(current);
    const int n = qMax(a.size(), b.size());
    while (a.size() < n)
        a.append(0);
    while (b.size() < n)
        b.append(0);
    return a > b;
}

} // namespace psg
