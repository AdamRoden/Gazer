#include "layout/PageActionParse.h"

#include "layout/PageDim.h"

#include <QStringList>

namespace gazer {

namespace {

QStringList splitCsv(const QString& value)
{
    QStringList out;
    for (const QString& p : value.split(QLatin1Char(','))) {
        out.push_back(p.trimmed());
    }
    while (!out.isEmpty() && out.last().isEmpty()) {
        out.removeLast();
    }
    return out;
}

bool isEdgeToken(const QString& t)
{
    const QString l = t.trimmed().toLower();
    return l == QLatin1String("down") || l == QLatin1String("up");
}

bool parseIntToken(const QString& t, int* out, QString* error, const char* what)
{
    const QString s = t.trimmed();
    if (s.isEmpty()) {
        return true;
    }
    bool ok = false;
    const int v = s.toInt(&ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("%1 must be an integer, got '%2'")
                         .arg(QString::fromLatin1(what), t);
        }
        return false;
    }
    *out = v;
    return true;
}

bool parseClickTail(const QStringList& parts, PageAction& out, QString* error, bool withZoom)
{
    if (!parts.isEmpty()) {
        out.button = parts[0];
    }
    int i = 1;
    const int n = parts.size();
    if (i < n && isEdgeToken(parts[i])) {
        // Count omitted; default 1.
    } else if (i < n) {
        if (!parseIntToken(parts[i], &out.clickCount, error, "Click count")) {
            return false;
        }
        ++i;
    }
    if (i < n && isEdgeToken(parts[i])) {
        out.clickEdge = parts[i];
        ++i;
    } else if (i < n && parts[i].isEmpty()) {
        ++i;
    }
    if (i < n) {
        if (!parseIntToken(parts[i], &out.speed, error, "Click speed")) {
            return false;
        }
        ++i;
    }
    if (withZoom && i < n) {
        if (!parseIntToken(parts[i], &out.zoomLevel, error, "Zoom")) {
            return false;
        }
        ++i;
    }
    if (i < n) {
        if (error) {
            *error = QStringLiteral("Unexpected extra field '%1'").arg(parts[i]);
        }
        return false;
    }
    return true;
}

} // namespace

bool parsePageAction(const QXmlStreamAttributes& a, const QString& cdata, PageAction& out,
                     QString* error)
{
    const QString id = a.value(QStringLiteral("id")).toString().trimmed();
    out.value = a.value(QStringLiteral("value")).toString();
    out.args = a.value(QStringLiteral("args")).toString();
    out.ahkSource = cdata;

    const QString nid = id.toLower();
    const QStringList parts = splitCsv(out.value);

    auto need = [&](int n, const char* what) -> bool {
        if (parts.size() < n) {
            if (error) {
                *error = QStringLiteral("%1 needs at least %2 value field(s)")
                             .arg(QString::fromLatin1(what))
                             .arg(n);
            }
            return false;
        }
        return true;
    };

    if (nid == QLatin1String("send")) {
        out.type = PageActionType::Send;
        if (!parts.isEmpty()) {
            out.sendKey = parts[0];
        }
        int i = 1;
        if (i < parts.size() && isEdgeToken(parts[i])) {
            out.sendEdge = parts[i];
            ++i;
        } else if (i < parts.size() && parts[i].isEmpty()) {
            ++i;
        }
        if (i < parts.size()) {
            if (!parseIntToken(parts[i], &out.sendDurationMs, error, "Send duration")) {
                return false;
            }
            ++i;
        }
        if (i < parts.size()) {
            if (error) {
                *error = QStringLiteral("Unexpected extra Send field '%1'").arg(parts[i]);
            }
            return false;
        }
        return true;
    }
    if (nid == QLatin1String("page")) {
        out.type = PageActionType::Page;
        if (!need(2, "Page")) {
            return false;
        }
        const QString verb = parts[0].toLower();
        if (verb == QLatin1String("open")) {
            out.verb = PageVerb::Open;
        } else if (verb == QLatin1String("close")) {
            out.verb = PageVerb::Close;
        } else if (verb == QLatin1String("toggle")) {
            out.verb = PageVerb::Toggle;
        } else {
            if (error) {
                *error = QStringLiteral("Unknown Page verb '%1'").arg(parts[0]);
            }
            return false;
        }
        const QString kind = parts[1].toLower();
        if (kind == QLatin1String("page")) {
            out.targetKind = PageTargetKind::Page;
        } else if (kind == QLatin1String("grid")) {
            out.targetKind = PageTargetKind::Grid;
        } else if (kind == QLatin1String("zone")) {
            out.targetKind = PageTargetKind::Zone;
        } else {
            if (error) {
                *error = QStringLiteral("Unknown Page target '%1'").arg(parts[1]);
            }
            return false;
        }
        if (parts.size() >= 3) {
            out.targetId = parts[2];
        }
        return true;
    }
    if (nid == QLatin1String("click")) {
        out.type = PageActionType::Click;
        return parseClickTail(parts, out, error, false);
    }
    if (nid == QLatin1String("move")) {
        out.type = PageActionType::Move;
        if (!parts.isEmpty()) {
            const QString m = parts[0].toLower();
            if (m == QLatin1String("gaze")) {
                out.moveMode = PageMoveMode::Gaze;
            } else if (m == QLatin1String("absolute")) {
                out.moveMode = PageMoveMode::Absolute;
            } else if (m == QLatin1String("relative")) {
                out.moveMode = PageMoveMode::Relative;
            } else {
                if (error) {
                    *error = QStringLiteral("Unknown Move mode '%1'").arg(parts[0]);
                }
                return false;
            }
        }
        if (parts.size() >= 2) {
            out.moveX = PageDimParse::parse(parts[1], error);
            if (error && !error->isEmpty()) {
                return false;
            }
        }
        if (parts.size() >= 3) {
            out.moveY = PageDimParse::parse(parts[2], error);
            if (error && !error->isEmpty()) {
                return false;
            }
        }
        if (parts.size() >= 4) {
            if (!parseIntToken(parts[3], &out.speed, error, "Move speed")) {
                return false;
            }
        }
        if (parts.size() >= 5) {
            if (!parseIntToken(parts[4], &out.zoomLevel, error, "Zoom")) {
                return false;
            }
        }
        return true;
    }
    if (nid == QLatin1String("moveandclick")) {
        out.type = PageActionType::MoveAndClick;
        return parseClickTail(parts, out, error, true);
    }
    if (nid == QLatin1String("command")) {
        out.type = PageActionType::Command;
        out.command = out.value.trimmed();
        if (out.command.isEmpty()) {
            out.command = out.args.trimmed();
        }
        return true;
    }
    if (nid == QLatin1String("speak")) {
        out.type = PageActionType::Speak;
        out.speakText = out.value;
        return true;
    }
    if (nid == QLatin1String("ahk")) {
        out.type = PageActionType::Ahk;
        return true;
    }
    if (error) {
        *error = QStringLiteral("Unknown Action id '%1'").arg(id);
    }
    return false;
}

} // namespace gazer
