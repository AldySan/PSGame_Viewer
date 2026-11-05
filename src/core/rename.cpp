#include "rename.h"

#include "detect.h"
#include "util.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace psg {

const char *const kBatchExtensions[] = {".pkg", ".ffpkg", ".ffpfsc",
                                        ".exfat", ".iso"};

namespace {

bool hasBatchExt(const QString &path)
{
    for (const char *ext : kBatchExtensions) {
        if (path.endsWith(QString::fromLatin1(ext), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QString typeSuffix(const GameInfo &info)
{
    const QString t = info.row(QStringLiteral("Type")).trimmed();
    const QString low = t.toLower();
    if (low == QLatin1String("update") || low == QLatin1String("dlc"))
        return t;
    return QString();
}

} // namespace

QString buildCleanName(const GameInfo &info, const RenameParts &parts)
{
    const QString title = info.title.trimmed();
    const QString tid = info.row(QStringLiteral("Title ID")).trimmed();
    QString ver = info.version().trimmed();
    if (ver.startsWith(u'v', Qt::CaseInsensitive))
        ver = ver.mid(1);
    ver = normalizeVersion(ver);
    QString region = info.region().trimmed();
    region = regionShort(region);

    QStringList out;
    if (!title.isEmpty() && parts.title)
        out.append(title);
    if (!tid.isEmpty() && tid != title && parts.tid)
        out.append(tid);
    if (!ver.isEmpty() && ver != QLatin1String("-") && parts.ver)
        out.append(QStringLiteral("v") + ver);
    if (!region.isEmpty() && region != QLatin1String("-") && parts.region)
        out.append(region);
    const QString suffix = typeSuffix(info);
    if (!suffix.isEmpty())
        out.append(suffix);
    if (out.isEmpty())
        return QString();
    return sanitizeFileNamePart(out.join(QStringLiteral(" - ")));
}

FormatResult formatName(const GameInfo &info, const QString &pattern,
                        const QString &origBase, const RenameParts &parts)
{
    FormatResult res;
    if (pattern.isEmpty())
        return {buildCleanName(info, parts), {}};

    const QString title = info.title.trimmed();
    const QString tid = info.row(QStringLiteral("Title ID")).trimmed();
    const QString cid = info.row(QStringLiteral("Content ID")).trimmed();
    QString ver = info.version().trimmed();
    if (ver.startsWith(u'v', Qt::CaseInsensitive))
        ver = ver.mid(1);
    ver = normalizeVersion(ver);
    QString region = info.region().trimmed();
    region = regionShort(region);

    QHash<QString, QString> vals;
    vals.insert(QStringLiteral("title"), title);
    vals.insert(QStringLiteral("tid"), tid == QLatin1String("-") ? QString() : tid);
    vals.insert(QStringLiteral("cid"), cid == QLatin1String("-") ? QString() : cid);
    vals.insert(QStringLiteral("version"),
                (ver.isEmpty() || ver == QLatin1String("-")) ? QString() : ver);
    vals.insert(QStringLiteral("region"),
                region == QLatin1String("-") ? QString() : region);
    vals.insert(QStringLiteral("type"), typeSuffix(info));
    vals.insert(QStringLiteral("orig"), origBase.trimmed());

    const QString low = pattern.toLower();
    for (auto it = vals.cbegin(); it != vals.cend(); ++it) {
        if (it.value().isEmpty() && low.contains(u'{' + it.key() + u'}'))
            res.missing.append(it.key().toUpper());
    }

    static const QRegularExpression tokenRe(QStringLiteral("\\{([a-zA-Z]+)\\}"));
    QString name;
    int pos = 0;
    while (true) {
        const QRegularExpressionMatch m = tokenRe.match(pattern, pos);
        if (!m.hasMatch()) {
            name += pattern.mid(pos);
            break;
        }
        name += pattern.mid(pos, m.capturedStart() - pos);
        const QString tok = m.captured(1).toLower();
        name += vals.contains(tok) ? vals.value(tok) : m.captured(0);
        pos = m.capturedEnd();
    }

    // empty token + its separators would leave "[]", " - - v", etc.
    static const QRegularExpression emptyBrackets(
        QStringLiteral("[\\[\\(\\{]\\s*[\\]\\)\\}]"));
    static const QRegularExpression dashRuns(QStringLiteral("(?:\\s*-\\s*){2,}"));
    static const QRegularExpression leadTrailDash(
        QStringLiteral("^\\s*-\\s*|[-\\s]+$"));
    static const QRegularExpression leadDash(QStringLiteral("^\\s*-\\s*"));
    static const QRegularExpression multiSpace(QStringLiteral("\\s{2,}"));
    name.remove(emptyBrackets);
    name.replace(dashRuns, QStringLiteral(" - "));
    name.remove(leadTrailDash);
    name.remove(leadDash);
    name.replace(multiSpace, QStringLiteral(" "));
    while (name.startsWith(u' ') || name.startsWith(u'-') || name.startsWith(u'_')
           || name.startsWith(u'.'))
        name.remove(0, 1);
    while (name.endsWith(u' ') || name.endsWith(u'-') || name.endsWith(u'_')
           || name.endsWith(u'.'))
        name.chop(1);
    res.base = sanitizeFileNamePart(name);
    return res;
}

QList<RenamePreset> renamePresets()
{
    return {
        {QStringLiteral("clean"),
         QStringLiteral("Clean name (Title - Title ID - vVersion - Region)"),
         QString()},
        {QStringLiteral("tid"), QStringLiteral("Title ID only"),
         QStringLiteral("{tid}")},
        {QStringLiteral("cid"), QStringLiteral("Content ID"),
         QStringLiteral("{cid}")},
        {QStringLiteral("append"), QStringLiteral("Append [Title ID] to original"),
         QStringLiteral("{orig} [{tid}]")},
    };
}

QStringList collectBatchFiles(const QStringList &inputs, bool recursive)
{
    QStringList out;
    QSet<QString> seen;
    const auto tryAdd = [&](const QString &fp) {
        const QString key = QFileInfo(fp).absoluteFilePath().toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            out.append(fp);
        }
    };
    for (const QString &inp : inputs) {
        QFileInfo fi(inp);
        if (fi.isDir()) {
            if (recursive) {
                QDirIterator it(inp, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString fp = it.next();
                    if (hasBatchExt(fp))
                        tryAdd(fp);
                }
            } else {
                const QDir d(inp);
                const QStringList files = d.entryList(QDir::Files, QDir::Name);
                for (const QString &fn : files) {
                    const QString fp = d.filePath(fn);
                    if (QFileInfo(fp).isFile() && hasBatchExt(fp))
                        tryAdd(fp);
                }
            }
        } else if (fi.isFile() && hasBatchExt(inp)) {
            tryAdd(inp);
        }
    }
    return out;
}

QList<BatchItem> previewBatch(const QStringList &files, const QString &pattern,
                              const RenameParts &include)
{
    QList<BatchItem> plan;
    QSet<QString> seenSets;
    QSet<QString> seenTargets;
    for (const QString &fp : files) {
        const QStringList parts = detect::splitSetSiblings(fp);
        const QString setKey =
            QFileInfo(parts.first()).absoluteFilePath().toLower();
        if (seenSets.contains(setKey))
            continue;
        seenSets.insert(setKey);

        BatchItem item;
        item.files = parts;
        const GameInfo r = detect::openAny(parts.first());
        if (!r.ok) {
            item.reason = r.error.isEmpty()
                ? QStringLiteral("unrecognized file") : r.error;
            plan.append(item);
            continue;
        }
        QString origBase = QFileInfo(parts.first()).completeBaseName();
        if (parts.size() > 1) {
            static const QRegularExpression re(QStringLiteral("_\\d+$"));
            origBase.remove(re);
        }
        const FormatResult fr = pattern.isEmpty()
            ? FormatResult{buildCleanName(r, include), {}}
            : formatName(r, pattern, origBase);
        if (fr.base.isEmpty()) {
            item.reason = !fr.missing.isEmpty()
                ? QStringLiteral("missing ") + fr.missing.join(QStringLiteral(", "))
                : QStringLiteral("not enough info for a name");
            plan.append(item);
            continue;
        }
        item.newBase = fr.base;
        item.ok = true;
        const QDir dir = QFileInfo(parts.first()).dir();
        if (parts.size() == 1) {
            const QString ext = QFileInfo(parts.first()).suffix();
            item.newFiles.append(dir.filePath(
                fr.base + (ext.isEmpty() ? QString() : u'.' + ext)));
        } else {
            static const QRegularExpression re(
                QStringLiteral("_(\\d+)(\\.[^.]+)$"));
            for (const QString &p : parts) {
                const QRegularExpressionMatch m =
                    re.match(QFileInfo(p).fileName());
                const QString num = m.hasMatch() ? m.captured(1) : QStringLiteral("0");
                const QString ext =
                    m.hasMatch() ? m.captured(2) : QStringLiteral(".pkg");
                item.newFiles.append(
                    dir.filePath(QStringLiteral("%1_%2%3").arg(fr.base, num, ext)));
            }
        }
        QStringList problems;
        for (int i = 0; i < item.files.size() && i < item.newFiles.size(); ++i) {
            const QString &old = item.files.at(i);
            const QString &neu = item.newFiles.at(i);
            if (old.compare(neu, Qt::CaseInsensitive) == 0)
                continue;
            if (QFileInfo::exists(neu)) {
                problems.append(
                    QStringLiteral("target exists: %1").arg(QFileInfo(neu).fileName()));
            } else {
                const QString nk =
                    QFileInfo(neu).absoluteFilePath().toLower();
                if (seenTargets.contains(nk))
                    problems.append(QStringLiteral("duplicate target: %1")
                                        .arg(QFileInfo(neu).fileName()));
            }
        }
        if (!problems.isEmpty()) {
            item.ok = false;
            item.reason = problems.first();
        }
        for (const QString &t : item.newFiles)
            seenTargets.insert(QFileInfo(t).absoluteFilePath().toLower());
        plan.append(item);
    }
    return plan;
}

BatchOutcome applyBatch(const QList<BatchItem> &plan, const QString &logPath)
{
    BatchOutcome out;
    QStringList lines;
    lines.append(QStringLiteral("# PSGame-Viewer batch rename %1")
                     .arg(QDateTime::currentDateTime()
                              .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"))));
    for (const BatchItem &item : plan) {
        if (!item.ok)
            continue;
        for (int i = 0; i < item.files.size() && i < item.newFiles.size(); ++i) {
            const QString old = item.files.at(i);
            const QString neu = item.newFiles.at(i);
            if (old.compare(neu, Qt::CaseInsensitive) == 0)
                continue;
            if (QFileInfo::exists(neu)) {
                out.skipped.append({old, QStringLiteral("target exists: %1")
                                              .arg(QFileInfo(neu).fileName())});
                continue;
            }
            QFile f(old);
            if (!f.rename(neu)) {
                out.skipped.append({old, f.errorString()});
                continue;
            }
            out.renamed.append({old, neu});
            lines.append(old + u'\t' + neu);
        }
    }
    if (!logPath.isEmpty() && !out.renamed.isEmpty()) {
        QFile lf(logPath);
        if (lf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            lf.write(lines.join(u'\n').toUtf8() + '\n');
            out.logPath = logPath;
        }
    }
    return out;
}

QList<QPair<QString, QString>> parseRevertLog(const QString &path)
{
    QList<QPair<QString, QString>> pairs;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return pairs;
    const QStringList lines = QString::fromUtf8(f.readAll()).split(u'\n');
    for (QString line : lines) {
        line.remove(u'\r');
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList parts = line.split(u'\t');
        if (parts.size() != 2)
            continue;
        const QString old = parts.at(0).trimmed();
        const QString neu = parts.at(1).trimmed();
        if (old.isEmpty() || neu.isEmpty())
            continue;
        // current name first, restore target second
        pairs.append({neu, old});
    }
    return pairs;
}

BatchOutcome applyRevert(const QList<QPair<QString, QString>> &pairs,
                         bool onlyExisting)
{
    BatchOutcome out;
    for (const auto &pr : pairs) {
        const QString current = pr.first;
        const QString restore = pr.second;
        if (current.compare(restore, Qt::CaseInsensitive) == 0)
            continue;
        if (onlyExisting && !QFileInfo::exists(current)) {
            out.skipped.append(
                {current, QStringLiteral("current file not found")});
            continue;
        }
        if (QFileInfo::exists(restore)) {
            out.skipped.append({current,
                                QStringLiteral("target exists: %1")
                                    .arg(QFileInfo(restore).fileName())});
            continue;
        }
        QFile f(current);
        if (!f.rename(restore)) {
            out.skipped.append({current, f.errorString()});
            continue;
        }
        out.renamed.append({current, restore});
    }
    return out;
}

} // namespace psg
