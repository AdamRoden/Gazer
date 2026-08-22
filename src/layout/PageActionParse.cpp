#include "layout/PageActionParse.h"

#include "layout/PageDim.h"

#include <QStringList>

namespace gazer {

namespace {

QStringList splitCsv(const QString& value)
{
    QStringList out;
    for (const QString& p : value.split(QLatin1Char(','))) {
        const QString t = p.trimmed();
        if (!t.isEmpty()) {
            out.push_back(t);
        }
    }
    return out;
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
        if (parts.size() >= 2) {
            out.sendEdge = parts[1];
        }
        if (parts.size() >= 3) {
            out.sendDurationMs = parts[2].toInt();
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
    if (nid == QLatin1String("click") || nid == QLatin1String("mouseclick")) {
        out.type = PageActionType::Click;
        if (!parts.isEmpty()) {
            out.button = parts[0];
        }
        if (parts.size() >= 2) {
            out.clickCount = parts[1].toInt();
        }
        if (parts.size() >= 3) {
            out.clickEdge = parts[2];
        }
        if (parts.size() >= 4) {
            out.speed = parts[3].toInt();
        }
        return true;
    }
    if (nid == QLatin1String("move") || nid == QLatin1String("mousemove")) {
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
            out.speed = parts[3].toInt();
        }
        if (parts.size() >= 5) {
            out.zoomLevel = parts[4].toInt();
        }
        return true;
    }
    if (nid == QLatin1String("moveandclick") || nid == QLatin1String("mousemoveandclick")) {
        out.type = PageActionType::MoveAndClick;
        if (!parts.isEmpty()) {
            out.button = parts[0];
        }
        if (parts.size() >= 2) {
            out.clickCount = parts[1].toInt();
        }
        if (parts.size() >= 3) {
            out.clickEdge = parts[2];
        }
        if (parts.size() >= 4) {
            out.speed = parts[3].toInt();
        }
        if (parts.size() >= 5) {
            out.zoomLevel = parts[4].toInt();
        }
        return true;
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
