#include "editor/LayoutEditorFields.h"

#include "layout/ChromeBlur.h"
#include "layout/PageActionParse.h"
#include "layout/PageDim.h"
#include "layout/PageTypes.h"
#include "ui/KeySymbols.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QHash>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>


namespace gazer {

void addChromeFields(PropertyBinder& b, QFormLayout* form, const PageChrome& st,
                     const ChromeMutate& apply, bool includeItemPaint)
{
    b.card(form, QStringLiteral("Look"), [&](QFormLayout* f) {
        b.note(f, QStringLiteral("Empty inherits the named style (Inherit), then the page, then "
                                 "settings (thickness 1 / radius 0)."));
        b.color(f, QStringLiteral("Background"), st.background, [apply](PageColor c) {
            apply(QStringLiteral("Background"), [&](PageChrome& s) { s.background = std::move(c); });
        });
        if (includeItemPaint) {
            b.color(f, QStringLiteral("Foreground"), st.foreground, [apply](PageColor c) {
                apply(QStringLiteral("Foreground"),
                      [&](PageChrome& s) { s.foreground = std::move(c); });
            });
        }
        b.color(f, QStringLiteral("Border"), st.borderColor, [apply](PageColor c) {
            apply(QStringLiteral("Border"), [&](PageChrome& s) { s.borderColor = std::move(c); });
        });
        b.optionalBox(f, QStringLiteral("Thickness"), st.thickness,
                      QStringLiteral("all  or  t,r,b,l"), [apply](std::optional<PageBox> v) {
                          apply(QStringLiteral("Thickness"), [&](PageChrome& s) { s.thickness = v; });
                      });
        b.optionalBox(f, QStringLiteral("Radius"), st.radius,
                      QStringLiteral("all  or  tl,tr,br,bl"), [apply](std::optional<PageBox> v) {
                          apply(QStringLiteral("Radius"), [&](PageChrome& s) { s.radius = v; });
                      });
        b.optionalReal(f, QStringLiteral("Frosted blur"), st.blur, 0, kChromeBlurMax, 1,
                       [apply](std::optional<double> v) {
                           apply(QStringLiteral("Blur"), [&](PageChrome& s) { s.blur = v; });
                       });
        if (!includeItemPaint) {
            return;
        }
        b.text(f, QStringLiteral("Progress style"),
               st.progressStyle ? st.progressStyle->toCsv() : QString(),
               [apply](const QString& raw) {
                   const QString s = raw.trimmed();
                   apply(QStringLiteral("Progress style"), [&](PageChrome& c) {
                       c.progressStyle =
                           s.isEmpty() ? std::nullopt
                                       : std::optional<ProgressStyle>(ProgressStyle::fromCsv(s));
                   });
               });
        b.color(f, QStringLiteral("Progress color"), st.progressColor, [apply](PageColor c) {
            apply(QStringLiteral("Progress color"),
                  [&](PageChrome& s) { s.progressColor = std::move(c); });
        });
        b.note(f, QStringLiteral("Empty inherits. Tokens: radial, pie, fill, fillup, "
                                 "filldown, fillleft, fillright."));
    });
}

namespace {

QString activationText(const PageDwell& d)
{
    if (!d.activation || d.activation->isEmpty()) {
        return {};
    }
    QStringList parts;
    for (int n : *d.activation) {
        parts.push_back(QString::number(n));
    }
    return parts.join(QLatin1Char(','));
}

QString navTargetChoice(const PageAction& a)
{
    switch (a.targetScope) {
    case PageNavScope::All:
        return QStringLiteral("-all");
    case PageNavScope::Self:
        return QStringLiteral("-self");
    case PageNavScope::Others:
        return QStringLiteral("-!self");
    case PageNavScope::Id:
        break;
    }
    return a.targetId;
}

void applyNavTargetChoice(PageAction& a, const QString& t)
{
    if (t == QLatin1String("-all")) {
        a.targetScope = PageNavScope::All;
        a.targetId.clear();
        return;
    }
    if (t == QLatin1String("-self")) {
        a.targetScope = PageNavScope::Self;
        a.targetId.clear();
        return;
    }
    if (t == QLatin1String("-!self")) {
        a.targetScope = PageNavScope::Others;
        a.targetId.clear();
        return;
    }
    a.targetScope = PageNavScope::Id;
    a.targetId = t;
}

} // namespace

void addDwellFields(PropertyBinder& b, QFormLayout* form, const PageDwell& dwell,
                    const DwellMutate& apply, bool includeHeading, const QStringList& inheritIds,
                    const QString& inheritCurrent,
                    const std::function<void(const QString&)>& inheritApply)
{
    b.card(form, QStringLiteral("Timing"), [&](QFormLayout* f) {
        if (includeHeading) {
            b.note(f, QStringLiteral("Empty inherits the named dwell, then the page, then "
                                     "settings."));
        }
        if (inheritApply) {
            addOptionalIdCombo(b, f, QStringLiteral("Inherit"), inheritIds, inheritCurrent,
                               inheritApply);
        }
        b.integer(f, QStringLiteral("Scan grace ms"), dwell.scanGrace.value_or(-1), -1, 5000,
                  [apply](int v) {
                      apply(QStringLiteral("Scan grace"), [&](PageDwell& d) {
                          if (v < 0) {
                              d.scanGrace.reset();
                          } else {
                              d.scanGrace = v;
                          }
                      });
                  });
        b.integer(f, QStringLiteral("Dwell grace ms"), dwell.dwellGrace.value_or(-1), -1, 5000,
                  [apply](int v) {
                      apply(QStringLiteral("Dwell grace"), [&](PageDwell& d) {
                          if (v < 0) {
                              d.dwellGrace.reset();
                          } else {
                              d.dwellGrace = v;
                          }
                      });
                  });
        b.text(f, QStringLiteral("Hold times (ms)"), activationText(dwell),
               [apply](const QString& t) {
                   apply(QStringLiteral("Dwell ms"), [&](PageDwell& d) {
                       if (t.trimmed().isEmpty()) {
                           d.activation.reset();
                           return;
                       }
                       QVector<int> seq;
                       for (const QString& p : t.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                           bool ok = false;
                           const int n = p.trimmed().toInt(&ok);
                           if (ok) {
                               seq.push_back(n);
                           }
                       }
                       if (seq.isEmpty()) {
                           d.activation.reset();
                       } else {
                           d.activation = seq;
                       }
                   });
               });
        b.note(f, QStringLiteral(
                      "Comma-separated hold steps. Empty inherits Settings. −1 grace inherits."));
    });
}

void addActionFields(PropertyBinder& b, QFormLayout* form, const PageAction& action,
                     const QString& itemLabel, const ActionCatalog& catalog,
                     const ActionMutate& apply)
{
    b.heading(form, QStringLiteral("Do this"));
    b.combo(form, QStringLiteral("Type"), pageActionSpells(), pageActionSpell(action),
            [apply](const QString& t) {
                apply(QStringLiteral("Action type"),
                      [&](PageAction& a) { (void)applyPageActionSpell(a, t); });
            });
    if (action.type == PageActionType::Send) {
        b.text(form, QStringLiteral("Key"), action.sendKey.isEmpty() ? itemLabel : action.sendKey,
               [apply](const QString& t) {
                   apply(QStringLiteral("Send key"), [&](PageAction& a) {
                       a.type = PageActionType::Send;
                       a.sendKey = t;
                   });
               });
        b.combo(form, QStringLiteral("Edge"),
                {QString(), QStringLiteral("Down"), QStringLiteral("Up")}, action.sendEdge,
                [apply](const QString& t) {
                    apply(QStringLiteral("Send edge"), [&](PageAction& a) { a.sendEdge = t; });
                });
        b.integer(form, QStringLiteral("Hold ms"), action.sendDurationMs, 0, 10000,
                  [apply](int v) {
                      apply(QStringLiteral("Send duration"),
                            [&](PageAction& a) { a.sendDurationMs = v; });
                  });
    }
    if (action.type == PageActionType::Command) {
        QStringList ids = catalog.commands;
        QStringList labels = catalog.commandLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        if (!action.command.isEmpty() && !ids.contains(action.command)) {
            ids.prepend(action.command);
            labels.prepend(friendlyCommandLabel(action.command));
        }
        if (ids.isEmpty()) {
            b.text(form, QStringLiteral("Command"), action.command, [apply](const QString& t) {
                apply(QStringLiteral("Command name"), [&](PageAction& a) {
                    a.type = PageActionType::Command;
                    a.command = t;
                });
            });
        } else {
            b.comboValues(form, QStringLiteral("Command"), labels, ids,
                          action.command.isEmpty() ? ids.first() : action.command,
                          [apply](const QString& t) {
                              apply(QStringLiteral("Command name"), [&](PageAction& a) {
                                  a.type = PageActionType::Command;
                                  a.command = t;
                              });
                          });
        }
        b.text(form, QStringLiteral("Args"), action.args, [apply](const QString& t) {
            apply(QStringLiteral("Command args"), [&](PageAction& a) { a.args = t; });
        });
    }
    if (action.type == PageActionType::ShowLayers) {
        b.text(form, QStringLiteral("Layers"), layerListCsv(normalizedLayers(action.layers)),
               [apply](const QString& t) {
                   apply(QStringLiteral("Show layers"), [&](PageAction& a) {
                       QVector<int> layers;
                       if (!parseLayerListStrict(t, layers) || layers.isEmpty()) {
                           layers = defaultLayers();
                       }
                       a.layers = std::move(layers);
                   });
               });
    }
    if (action.type == PageActionType::HostPage) {
        QStringList ids = catalog.layoutIds;
        QStringList labels = catalog.layoutLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        QStringList hostIds = ids;
        QStringList hostLabels = labels;
        hostIds.prepend(QString());
        hostLabels.prepend(QStringLiteral("(this page)"));
        const QString hostCur = action.hostId;
        if (!hostCur.isEmpty() && !hostIds.contains(hostCur)) {
            hostIds.insert(1, hostCur);
            hostLabels.insert(1, hostCur);
        }
        b.comboValues(form, QStringLiteral("Host page"), hostLabels, hostIds, hostCur,
                      [apply](const QString& t) {
                          apply(QStringLiteral("Host page"),
                                [&](PageAction& a) { a.hostId = t.trimmed(); });
                      });
        const QString bodyCur = action.targetId;
        if (!bodyCur.isEmpty() && !ids.contains(bodyCur)) {
            ids.prepend(bodyCur);
            labels.prepend(bodyCur);
        }
        b.comboValues(form, QStringLiteral("Body page"), labels, ids, bodyCur,
                      [apply](const QString& t) {
                          apply(QStringLiteral("Host body"),
                                [&](PageAction& a) { a.targetId = t.trimmed(); });
                      });
    }
    if (action.type == PageActionType::Nav) {
        if (action.verb != PageVerb::Close) {
            QStringList ids = catalog.layoutIds;
            QStringList labels = catalog.layoutLabels;
            if (labels.size() != ids.size()) {
                labels = ids;
            }
            const QString current = navTargetChoice(action);
            if (!current.isEmpty() && !ids.contains(current)) {
                ids.prepend(current);
                labels.prepend(current);
            }
            b.comboValues(form, QStringLiteral("Target id"), labels, ids, current,
                          [apply](const QString& t) {
                              apply(QStringLiteral("Page target"),
                                    [&](PageAction& a) { applyNavTargetChoice(a, t); });
                          });
            b.check(form, QStringLiteral("Save breadcrumb"), action.breadcrumb, [apply](bool on) {
                apply(QStringLiteral("Breadcrumb"), [&](PageAction& a) { a.breadcrumb = on; });
            });
        }
    }
    if (action.type == PageActionType::Speak) {
        b.text(form, QStringLiteral("Text"), action.speakText, [apply](const QString& t) {
            apply(QStringLiteral("Speak text"), [&](PageAction& a) {
                a.type = PageActionType::Speak;
                a.speakText = t;
            });
        });
    }
    if (action.type == PageActionType::Click) {
        b.combo(form, QStringLiteral("Type"), pageActionClickKindChoices(),
                pageActionClickKindText(action.clickKind), [apply](const QString& t) {
                    apply(QStringLiteral("Click type"),
                          [&](PageAction& a) { (void)applyPageActionClickKind(a, t); });
                });
    }
    auto zoomCombo = [&]() {
        b.note(form, QStringLiteral("default = Settings mag-pick. 0 = dwell-move, no magnify. "
                                   "N = N× zoom. −1 = foresight. −2 = foresight + bonus zoom."));
        const QString z = pageActionZoomText(action);
        QStringList zs = pageActionZoomChoices();
        if (!zs.contains(z)) {
            zs.insert(1, z);
        }
        b.combo(form, QStringLiteral("Zoom"), zs, z, [apply](const QString& t) {
            apply(QStringLiteral("Zoom"), [&](PageAction& a) { (void)applyPageActionZoom(a, t); });
        });
    };
    if (action.type == PageActionType::MoveAndClick) {
        zoomCombo();
    }
    if (action.type == PageActionType::Move) {
        if (action.moveMode == PageMoveMode::Gaze) {
            zoomCombo();
        }
        if (action.moveMode == PageMoveMode::Direction) {
            const QStringList dirs = {QStringLiteral("n"),  QStringLiteral("s"),
                                      QStringLiteral("e"),  QStringLiteral("w"),
                                      QStringLiteral("ne"), QStringLiteral("nw"),
                                      QStringLiteral("se"), QStringLiteral("sw")};
            b.combo(form, QStringLiteral("Direction"), dirs,
                    pageActionCompassToken(action.moveDirection), [apply](const QString& t) {
                apply(QStringLiteral("Move direction"), [&](PageAction& a) {
                    bool ok = false;
                    a.moveDirection = PageDimParse::parseAnchor(t, &ok);
                    if (!ok) {
                        a.moveDirection = PageAnchor::Top;
                    }
                });
            });
            b.integer(form, QStringLiteral("Amount px (−1 = settings)"), action.moveAmount, -1,
                      2000, [apply](int v) {
                          apply(QStringLiteral("Move amount"),
                                [&](PageAction& a) { a.moveAmount = v; });
                      });
        }
        if (action.moveMode == PageMoveMode::Absolute || action.moveMode == PageMoveMode::Relative) {
            b.dim(form, QStringLiteral("X"), action.moveX, [apply](PageDim v) {
                apply(QStringLiteral("Move X"), [&](PageAction& a) { a.moveX = v; });
            });
            b.dim(form, QStringLiteral("Y"), action.moveY, [apply](PageDim v) {
                apply(QStringLiteral("Move Y"), [&](PageAction& a) { a.moveY = v; });
            });
        }
    }
    if (action.type == PageActionType::Ahk) {
        b.text(form, QStringLiteral("AHK"), action.ahkSource, [apply](const QString& t) {
            apply(QStringLiteral("AHK"), [&](PageAction& a) {
                a.type = PageActionType::Ahk;
                a.ahkSource = t;
            });
        });
    }
    if (action.type == PageActionType::Run) {
        b.combo(form, QStringLiteral("Kind"), pageRunKindChoices(), pageRunKindText(action.runKind),
                [apply](const QString& t) {
                    apply(QStringLiteral("Run kind"),
                          [&](PageAction& a) { (void)applyPageRunKind(a, t); });
                });
        b.text(form, QStringLiteral("File"), action.runFile, [apply](const QString& t) {
            apply(QStringLiteral("Run file"), [&](PageAction& a) {
                a.type = PageActionType::Run;
                a.runFile = t.trimmed();
            });
        });
        b.check(form, QStringLiteral("Keep running"), action.runPersist, [apply](bool on) {
            apply(QStringLiteral("Run persist"), [&](PageAction& a) { a.runPersist = on; });
        });
        b.text(form, QStringLiteral("Key"), action.runKey, [apply](const QString& t) {
            apply(QStringLiteral("Run key"), [&](PageAction& a) { a.runKey = t.trimmed(); });
        });
        b.text(form, QStringLiteral("Args"), action.args, [apply](const QString& t) {
            apply(QStringLiteral("Run args"), [&](PageAction& a) { a.args = t; });
        });
        b.note(form, QStringLiteral("Relative to the page file. Python (.py) or AutoHotkey (.ahk). "
                                    "Keep running starts once until Gazer quits."));
    }
}

QStringList visibleWhenChoices()
{
    return {QString(), QStringLiteral("expanded"), QStringLiteral("!expanded"),
            QStringLiteral("dwellSuspend"), QStringLiteral("!dwellSuspend"),
            QStringLiteral("hover_custom"), QStringLiteral("flash_custom")};
}

QStringList iconChoices()
{
    QStringList names = KeySymbols::names();
    names.prepend(QString());
    return names;
}

QString friendlyCommandLabel(const QString& commandId)
{
    static const QHash<QString, QString> k = {
        {QStringLiteral("quitApp"), QStringLiteral("Quit Gazer")},
        {QStringLiteral("toggleDwellSuspend"), QStringLiteral("Pause / resume dwell")},
        {QStringLiteral("openPageEditor"), QStringLiteral("Page editor")},
        {QStringLiteral("openPreview"), QStringLiteral("Head preview")},
        {QStringLiteral("tab"), QStringLiteral("Tab key")},
        {QStringLiteral("enter"), QStringLiteral("Enter key")},
        {QStringLiteral("backspace"), QStringLiteral("Backspace")},
        {QStringLiteral("space"), QStringLiteral("Space")},
        {QStringLiteral("escape"), QStringLiteral("Escape")},
        {QStringLiteral("leftCtrl"), QStringLiteral("Ctrl (cycle)")},
        {QStringLiteral("leftAlt"), QStringLiteral("Alt (cycle)")},
        {QStringLiteral("leftWin"), QStringLiteral("Win (cycle)")},
        {QStringLiteral("leftShift"), QStringLiteral("Shift (cycle)")},
        {QStringLiteral("releaseModifiers"), QStringLiteral("Release modifiers")},
        {QStringLiteral("mouseLeftClick"), QStringLiteral("Left click")},
        {QStringLiteral("mouseRightClick"), QStringLiteral("Right click")},
        {QStringLiteral("mouseMiddleClick"), QStringLiteral("Middle click")},
        {QStringLiteral("mouseLeftClickAtGaze"), QStringLiteral("Left click at gaze")},
        {QStringLiteral("mouseRightClickAtGaze"), QStringLiteral("Right click at gaze")},
        {QStringLiteral("mouseMiddleClickAtGaze"), QStringLiteral("Middle click at gaze")},
        {QStringLiteral("mouseMoveToGaze"), QStringLiteral("Dwell-move cursor")},
        {QStringLiteral("mouseMoveToGazeClickLoop"), QStringLiteral("Gaze click loop")},
        {QStringLiteral("toggleMagnifier"), QStringLiteral("Magnifier")},
        {QStringLiteral("toggleLookToScroll"), QStringLiteral("Look to scroll")},
        {QStringLiteral("lookToScroll"), QStringLiteral("Look to scroll")},
        {QStringLiteral("lookToMouse"), QStringLiteral("Look to mouse")},
        {QStringLiteral("lookToLeftStick"), QStringLiteral("Look to left stick")},
        {QStringLiteral("lookToRightStick"), QStringLiteral("Look to right stick")},
        {QStringLiteral("toggleComboMouse"), QStringLiteral("ComboMouse")},
        {QStringLiteral("toggleGazeReticle"), QStringLiteral("Gaze reticle")},
        {QStringLiteral("toggleGazeMouseFollow"), QStringLiteral("Cursor follows gaze")},
    };
    return k.value(commandId, commandId);
}

QStringList pageAnchorNames()
{
    return {QStringLiteral("TopLeft"),  QStringLiteral("Top"),         QStringLiteral("TopRight"),
            QStringLiteral("Left"),     QStringLiteral("Center"),      QStringLiteral("Right"),
            QStringLiteral("BottomLeft"), QStringLiteral("Bottom"),    QStringLiteral("BottomRight")};
}

void addActionSeriesFields(PropertyBinder& b, QFormLayout* form, const QVector<PageAction>& acts,
                           const QString& itemLabel, const ActionCatalog& catalog, int selectedStep,
                           const std::function<void(int)>& selectStep,
                           const std::function<void(QVector<PageAction>)>& applyAll)
{
    b.card(form, QStringLiteral("Actions"), [&](QFormLayout* inner) {
        const int step = acts.isEmpty() ? 0 : qBound(0, selectedStep, acts.size() - 1);
        if (acts.size() > 1) {
            QStringList labels;
            for (int i = 0; i < acts.size(); ++i) {
                labels.push_back(QStringLiteral("Step %1").arg(i + 1));
            }
            b.combo(inner, QStringLiteral("Step"), labels, labels[step],
                    [selectStep, labels](const QString& t) {
                        selectStep(labels.indexOf(t));
                    });
        }

        const PageAction current = acts.isEmpty() ? PageAction{} : acts[step];
        addActionFields(b, inner, current, itemLabel, catalog,
                        [applyAll, acts, step](const QString&, const auto& mut) {
                            QVector<PageAction> next = acts;
                            if (next.isEmpty()) {
                                PageAction a;
                                mut(a);
                                next.push_back(a);
                            } else {
                                mut(next[qBound(0, step, next.size() - 1)]);
                            }
                            applyAll(next);
                        });

        auto* row = new QWidget;
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        auto* add = new QPushButton(QStringLiteral("Add step"));
        bool* const loadingFlag = b.loading;
        QObject::connect(add, &QPushButton::clicked, b.host,
                         [loadingFlag, applyAll, acts, selectStep]() {
                             if (loadingFlag && *loadingFlag) {
                                 return;
                             }
                             QVector<PageAction> next = acts;
                             PageAction a;
                             a.type = PageActionType::Command;
                             next.push_back(a);
                             selectStep(next.size() - 1);
                             applyAll(next);
                         });
        rowLay->addWidget(add);
        if (acts.size() > 1) {
            auto* rm = new QPushButton(QStringLiteral("Remove step"));
            QObject::connect(rm, &QPushButton::clicked, b.host,
                             [loadingFlag, applyAll, acts, step, selectStep]() {
                                 if (loadingFlag && *loadingFlag) {
                                     return;
                                 }
                                 QVector<PageAction> next = acts;
                                 next.removeAt(step);
                                 selectStep(qMax(0, step - 1));
                                 applyAll(next);
                             });
            rowLay->addWidget(rm);
        }
        inner->addRow(QStringLiteral("Steps"), row);
    });
}

QStringList sortedKeys(const QStringList& keys)
{
    QStringList out = keys;
    out.sort(Qt::CaseInsensitive);
    return out;
}

void addOptionalIdCombo(PropertyBinder& b, QFormLayout* form, const QString& label,
                        const QStringList& ids, const QString& current,
                        const std::function<void(const QString&)>& apply)
{
    QStringList labels{QStringLiteral("(none)")};
    QStringList values{QString()};
    for (const QString& id : sortedKeys(ids)) {
        labels.push_back(id);
        values.push_back(id);
    }
    b.comboValues(form, label, labels, values, current, apply);
}

void addNamedChromeEditor(PropertyBinder& b, QFormLayout* form,
                          const QHash<QString, PageChrome>& styles, const QString& selectedId,
                          const std::function<void(const QString&)>& selectId,
                          const std::function<void()>& addNew,
                          const std::function<void(const QString& from, const QString& to)>& rename,
                          const std::function<void(const QString&)>& remove,
                          const ChromeMutate& apply)
{
    const QStringList keys = sortedKeys(styles.keys());
    addOptionalIdCombo(b, form, QStringLiteral("Style"), keys, selectedId, selectId);
    auto* row = new QWidget;
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* addBtn = new QPushButton(QStringLiteral("Add style"));
    bool* const loadingFlag = b.loading;
    QObject::connect(addBtn, &QPushButton::clicked, b.host, [loadingFlag, addNew]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        addNew();
    });
    lay->addWidget(addBtn);
    if (!selectedId.isEmpty() && styles.contains(selectedId)) {
        auto* delBtn = new QPushButton(QStringLiteral("Delete"));
        QObject::connect(delBtn, &QPushButton::clicked, b.host, [loadingFlag, remove, selectedId]() {
            if (loadingFlag && *loadingFlag) {
                return;
            }
            remove(selectedId);
        });
        lay->addWidget(delBtn);
    }
    form->addRow(QStringLiteral(" "), row);
    if (selectedId.isEmpty() || !styles.contains(selectedId)) {
        b.note(form, QStringLiteral("Pick a named style to edit, or add one to reuse look across cells."));
        return;
    }
    b.text(form, QStringLiteral("Style id"), selectedId, [rename, selectedId](const QString& t) {
        rename(selectedId, t.trimmed());
    });
    addChromeFields(b, form, styles.value(selectedId), apply);
}

void addOffsetSizeFields(PropertyBinder& b, QFormLayout* form, const PageDimPair& offset,
                         const PageDimPair& size, const std::function<void(PageDim)>& applyOffX,
                         const std::function<void(PageDim)>& applyOffY,
                         const std::function<void(PageDim)>& applyW,
                         const std::function<void(PageDim)>& applyH)
{
    b.dim(form, QStringLiteral("Offset X"), offset.x, applyOffX);
    b.dim(form, QStringLiteral("Offset Y"), offset.y, applyOffY);
    b.dim(form, QStringLiteral("Width"), size.x, applyW);
    b.dim(form, QStringLiteral("Height"), size.y, applyH);
}

void addPlacementGeometry(PropertyBinder& b, QFormLayout* form, bool desktopMode, PageAnchor anchor,
                          const PageDimPair& offset, const PageDimPair& size,
                          const std::function<void(bool)>& applyDesktop,
                          const std::function<void(const QString&)>& applyAnchor,
                          const std::function<void(PageDim)>& applyOffX,
                          const std::function<void(PageDim)>& applyOffY,
                          const std::function<void(PageDim)>& applyW,
                          const std::function<void(PageDim)>& applyH)
{
    b.check(form, QStringLiteral("Desktop bounds"), desktopMode, applyDesktop);
    b.combo(form, QStringLiteral("Anchor"), pageAnchorNames(), PageDimParse::anchorName(anchor),
            applyAnchor);
    addOffsetSizeFields(b, form, offset, size, applyOffX, applyOffY, applyW, applyH);
}

void addNamedDwellEditor(PropertyBinder& b, QFormLayout* form,
                         const QHash<QString, PageDwell>& dwells, const QString& selectedId,
                         const std::function<void(const QString&)>& selectId,
                         const std::function<void()>& addNew,
                         const std::function<void(const QString& from, const QString& to)>& rename,
                         const std::function<void(const QString&)>& remove,
                         const DwellMutate& apply)
{
    const QStringList keys = sortedKeys(dwells.keys());
    addOptionalIdCombo(b, form, QStringLiteral("Dwell"), keys, selectedId, selectId);
    auto* row = new QWidget;
    auto* lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* addBtn = new QPushButton(QStringLiteral("Add dwell"));
    bool* const loadingFlag = b.loading;
    QObject::connect(addBtn, &QPushButton::clicked, b.host, [loadingFlag, addNew]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        addNew();
    });
    lay->addWidget(addBtn);
    if (!selectedId.isEmpty() && dwells.contains(selectedId)) {
        auto* delBtn = new QPushButton(QStringLiteral("Delete"));
        QObject::connect(delBtn, &QPushButton::clicked, b.host, [loadingFlag, remove, selectedId]() {
            if (loadingFlag && *loadingFlag) {
                return;
            }
            remove(selectedId);
        });
        lay->addWidget(delBtn);
    }
    form->addRow(QStringLiteral(" "), row);
    if (selectedId.isEmpty() || !dwells.contains(selectedId)) {
        b.note(form, QStringLiteral("Named dwells are referenced by cells, zones, and grids."));
        return;
    }
    b.text(form, QStringLiteral("Dwell id"), selectedId, [rename, selectedId](const QString& t) {
        rename(selectedId, t.trimmed());
    });
    addDwellFields(b, form, dwells.value(selectedId), apply, false);
}

} // namespace gazer
