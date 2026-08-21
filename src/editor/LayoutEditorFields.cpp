#include "editor/LayoutEditorFields.h"

#include "layout/LayoutSchema.h"
#include "ui/MouseIcons.h"
#include "ui/Theme.h"

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
#include <QSpinBox>
#include <QStringList>
#include <QWidget>

namespace gazer {

void PropertyBinder::heading(QFormLayout* form, const QString& text)
{
    auto* lab = new QLabel(text);
    lab->setObjectName(QStringLiteral("fieldHeading"));
    form->addRow(lab);
}

void PropertyBinder::note(QFormLayout* form, const QString& text)
{
    auto* lab = new QLabel(text);
    lab->setWordWrap(true);
    lab->setObjectName(QStringLiteral("fieldNote"));
    form->addRow(lab);
}

void PropertyBinder::text(QFormLayout* form, const QString& label, const QString& value,
                          const std::function<void(const QString&)>& apply)
{
    auto* e = new QLineEdit(value);
    bool* const loadingFlag = loading;
    QObject::connect(e, &QLineEdit::editingFinished, host, [loadingFlag, e, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(e->text());
    });
    form->addRow(label, e);
}

void PropertyBinder::integer(QFormLayout* form, const QString& label, int value, int min, int max,
                             const std::function<void(int)>& apply)
{
    auto* s = new QSpinBox;
    s->setRange(min, max);
    s->setValue(value);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    bool* const loadingFlag = loading;
    QObject::connect(s, &QSpinBox::editingFinished, host, [loadingFlag, s, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(s->value());
    });
    form->addRow(label, s);
}

void PropertyBinder::real(QFormLayout* form, const QString& label, double value, double min,
                          double max, int decimals, const std::function<void(double)>& apply)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setDecimals(decimals);
    s->setValue(value);
    s->setButtonSymbols(QAbstractSpinBox::NoButtons);
    bool* const loadingFlag = loading;
    QObject::connect(s, &QDoubleSpinBox::editingFinished, host, [loadingFlag, s, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(s->value());
    });
    form->addRow(label, s);
}

void PropertyBinder::check(QFormLayout* form, const QString& label, bool value,
                           const std::function<void(bool)>& apply)
{
    auto* c = new QCheckBox;
    c->setChecked(value);
    bool* const loadingFlag = loading;
    QObject::connect(c, &QCheckBox::toggled, host, [loadingFlag, apply](bool on) {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(on);
    });
    form->addRow(label, c);
}

void PropertyBinder::combo(QFormLayout* form, const QString& label, const QStringList& items,
                           const QString& current, const std::function<void(const QString&)>& apply)
{
    auto* c = new QComboBox;
    c->addItems(items);
    const int idx = c->findText(current, Qt::MatchFixedString);
    c->setCurrentIndex(idx >= 0 ? idx : 0);
    bool* const loadingFlag = loading;
    QObject::connect(c, &QComboBox::currentTextChanged, host, [loadingFlag, apply](const QString& t) {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(t);
    });
    form->addRow(label, c);
}

void PropertyBinder::comboValues(QFormLayout* form, const QString& label, const QStringList& labels,
                                 const QStringList& values, const QString& currentValue,
                                 const std::function<void(const QString&)>& apply)
{
    auto* c = new QComboBox;
    const int n = qMin(labels.size(), values.size());
    for (int i = 0; i < n; ++i) {
        c->addItem(labels[i], values[i]);
    }
    int idx = c->findData(currentValue);
    if (idx < 0 && !currentValue.isEmpty()) {
        c->insertItem(0, currentValue, currentValue);
        idx = 0;
    }
    c->setCurrentIndex(idx >= 0 ? idx : 0);
    bool* const loadingFlag = loading;
    QObject::connect(c, &QComboBox::currentIndexChanged, host, [loadingFlag, c, apply](int) {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        apply(c->currentData().toString());
    });
    form->addRow(label, c);
}

void PropertyBinder::color(QFormLayout* form, const QString& label,
                           const std::optional<QColor>& value,
                           const std::function<void(std::optional<QColor>)>& apply)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* e = new QLineEdit(value ? ThemeColors::colorToHex(*value) : QString());
    e->setPlaceholderText(QStringLiteral("unset"));
    auto* swatch = new QPushButton;
    swatch->setFixedSize(28, 22);
    swatch->setFlat(true);
    const QString bg = value ? ThemeColors::colorToHex(*value) : QStringLiteral("#333333");
    swatch->setStyleSheet(QStringLiteral("background:%1; border:1px solid #555;").arg(bg));
    bool* const loadingFlag = loading;
    QWidget* const dlgHost = host;
    auto commit = [loadingFlag, e, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        const QString t = e->text().trimmed();
        if (t.isEmpty()) {
            apply(std::nullopt);
            return;
        }
        const QColor c = ThemeColors::parseColor(t, QColor());
        apply(c.isValid() ? std::optional<QColor>(c) : std::nullopt);
    };
    QObject::connect(e, &QLineEdit::editingFinished, host, commit);
    QObject::connect(swatch, &QPushButton::clicked, host, [loadingFlag, e, apply, dlgHost]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        const QColor start = ThemeColors::parseColor(e->text(), QColor(200, 200, 200));
        const QColor c = QColorDialog::getColor(start, dlgHost, QStringLiteral("Color"),
                                                QColorDialog::ShowAlphaChannel);
        if (!c.isValid()) {
            return;
        }
        e->setText(ThemeColors::colorToHex(c));
        apply(c);
    });
    h->addWidget(e, 1);
    h->addWidget(swatch);
    form->addRow(label, row);
}

void PropertyBinder::dim(QFormLayout* form, const QString& label, const DimSpec& value,
                         const std::function<void(DimSpec)>& apply)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* unit = new QComboBox;
    unit->addItems({QStringLiteral("unset"), QStringLiteral("px"), QStringLiteral("%")});
    if (!value.isSet()) {
        unit->setCurrentIndex(0);
    } else if (value.unit == DimSpec::Unit::Pixels) {
        unit->setCurrentIndex(1);
    } else {
        unit->setCurrentIndex(2);
    }
    auto* spin = new QDoubleSpinBox;
    spin->setRange(-10000, 10000);
    spin->setDecimals(1);
    spin->setValue(value.value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setEnabled(unit->currentIndex() != 0);
    bool* const loadingFlag = loading;
    auto commit = [loadingFlag, unit, spin, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        DimSpec d;
        if (unit->currentIndex() == 1) {
            d = DimSpec::pixels(spin->value());
        } else if (unit->currentIndex() == 2) {
            d = DimSpec::percent(spin->value());
        }
        apply(d);
    };
    QObject::connect(unit, &QComboBox::currentIndexChanged, host, [spin, commit](int idx) {
        spin->setEnabled(idx != 0);
        commit();
    });
    QObject::connect(spin, &QDoubleSpinBox::editingFinished, host, commit);
    h->addWidget(unit);
    h->addWidget(spin, 1);
    form->addRow(label, row);
}

void PropertyBinder::optionalReal(QFormLayout* form, const QString& label,
                                  const std::optional<double>& value, double min, double max,
                                  int decimals, const std::function<void(std::optional<double>)>& apply)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* unit = new QComboBox;
    unit->addItems({QStringLiteral("inherit"), QStringLiteral("set")});
    unit->setCurrentIndex(value.has_value() ? 1 : 0);
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value.value_or(0.0));
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setEnabled(value.has_value());
    bool* const loadingFlag = loading;
    auto commit = [loadingFlag, unit, spin, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        if (unit->currentIndex() == 0) {
            apply(std::nullopt);
        } else {
            apply(spin->value());
        }
    };
    QObject::connect(unit, &QComboBox::currentIndexChanged, host, [spin, commit](int idx) {
        spin->setEnabled(idx != 0);
        commit();
    });
    QObject::connect(spin, &QDoubleSpinBox::editingFinished, host, commit);
    h->addWidget(unit);
    h->addWidget(spin, 1);
    form->addRow(label, row);
}

void addChromeFields(PropertyBinder& b, QFormLayout* form, const LayoutChromeStyle& st,
                     const ChromeMutate& apply)
{
    b.heading(form, QStringLiteral("Look"));
    b.note(form, QStringLiteral("Empty / inherit uses the board default, then the theme."));
    b.color(form, QStringLiteral("Background"), st.background, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Background"), [&](LayoutChromeStyle& s) { s.background = c; });
    });
    b.color(form, QStringLiteral("Foreground"), st.foreground, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Foreground"), [&](LayoutChromeStyle& s) { s.foreground = c; });
    });
    b.color(form, QStringLiteral("Border"), st.borderColor, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Border"), [&](LayoutChromeStyle& s) { s.borderColor = c; });
    });
    b.optionalReal(form, QStringLiteral("Border width"), st.borderWidth, 0, 32, 1,
                   [apply](std::optional<double> v) {
                       apply(QStringLiteral("Border width"),
                             [&](LayoutChromeStyle& s) { s.borderWidth = v; });
                   });
    b.optionalReal(form, QStringLiteral("Corner radius"), st.radius, 0, 64, 1,
                   [apply](std::optional<double> v) {
                       apply(QStringLiteral("Radius"), [&](LayoutChromeStyle& s) { s.radius = v; });
                   });
    b.optionalReal(form, QStringLiteral("Frosted blur"), st.blur, 0, 64, 1,
                   [apply](std::optional<double> v) {
                       apply(QStringLiteral("Blur"), [&](LayoutChromeStyle& s) { s.blur = v; });
                   });
}

namespace {

QString msSequenceText(const LayoutDwellConfig& d)
{
    if (d.msSequence.isEmpty()) {
        return QString::number(d.ms);
    }
    QStringList parts;
    for (int n : d.msSequence) {
        parts.push_back(QString::number(n));
    }
    return parts.join(QLatin1Char(','));
}

void applyMsSequence(LayoutDwellConfig& d, const QString& text)
{
    d.msSequence.clear();
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        bool ok = false;
        const int n = p.trimmed().toInt(&ok);
        if (ok && n > 0) {
            d.msSequence.push_back(n);
        }
    }
    d.hasTiming = true;
    d.sectionPresent = true;
    d.ms = d.msSequence.isEmpty() ? 800 : d.msSequence.first();
    if (d.msSequence.isEmpty()) {
        d.msSequence = {d.ms};
    }
}

} // namespace

void addDwellFields(PropertyBinder& b, QFormLayout* form, const LayoutDwellConfig& dwell,
                    const DwellMutate& apply, bool boardLevel)
{
    b.heading(form, QStringLiteral("Gaze"));
    if (boardLevel) {
        b.check(form, QStringLiteral("Gaze can activate keys"), dwell.enabled, [apply](bool on) {
            apply(QStringLiteral("Gaze activation"), [&](LayoutDwellConfig& d) {
                d.sectionPresent = true;
                d.enabled = on;
            });
        });
        b.note(form, QStringLiteral("Off: looking at this board does not fire actions."));
    }

    const bool customTiming = dwell.hasTiming || !dwell.msSequence.isEmpty();
    b.check(form, QStringLiteral("Override hold times"), customTiming, [apply](bool on) {
        apply(QStringLiteral("Dwell timing"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.hasTiming = on;
            if (!on) {
                d.msSequence.clear();
                d.ms = 800;
            } else if (d.msSequence.isEmpty()) {
                d.msSequence = {d.ms > 0 ? d.ms : 800};
            }
        });
    });
    if (customTiming) {
        b.text(form, QStringLiteral("Hold times (ms)"), msSequenceText(dwell),
               [apply](const QString& t) {
                   apply(QStringLiteral("Dwell ms"),
                         [&](LayoutDwellConfig& d) { applyMsSequence(d, t); });
               });
        b.note(form, QStringLiteral("Comma-separated. Last value repeats while gaze holds."));
    }

    b.check(form, QStringLiteral("Override progress look"), dwell.hasProgressStyle, [apply](bool on) {
        apply(QStringLiteral("Progress style"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.hasProgressStyle = on;
            if (!on) {
                d.progressStyle = QStringLiteral("radial");
            }
        });
    });
    if (dwell.hasProgressStyle) {
        const QStringList styles = {QStringLiteral("radial"),
                                    QStringLiteral("fill"),
                                    QStringLiteral("border"),
                                    QStringLiteral("radial,fill"),
                                    QStringLiteral("radial,border"),
                                    QStringLiteral("fill,border"),
                                    QStringLiteral("radial,fill,border")};
        b.combo(form, QStringLiteral("Progress"), styles,
                dwell.progressStyle.isEmpty() ? QStringLiteral("radial") : dwell.progressStyle,
                [apply](const QString& t) {
                    apply(QStringLiteral("Progress style"), [&](LayoutDwellConfig& d) {
                        d.sectionPresent = true;
                        d.hasProgressStyle = true;
                        d.progressStyle = t;
                    });
                });
        b.text(form, QStringLiteral("Progress color"), dwell.progressColor,
               [apply](const QString& t) {
                   apply(QStringLiteral("Progress color"), [&](LayoutDwellConfig& d) {
                       d.sectionPresent = true;
                       d.hasProgressColor = !t.trimmed().isEmpty();
                       d.progressColor = t.trimmed();
                   });
               });
    }
}

void addActionFields(PropertyBinder& b, QFormLayout* form, const LayoutAction& action,
                     const QString& itemLabel, const ActionCatalog& catalog,
                     const ActionMutate& apply)
{
    b.heading(form, QStringLiteral("Do this"));
    QString preset = QStringLiteral("Custom");
    if (action.type == LayoutAction::Type::Unknown) {
        preset = QStringLiteral("None");
    } else if (action.type == LayoutAction::Type::TypeText
               && (action.text == itemLabel || action.text.isEmpty())) {
        preset = QStringLiteral("Type the label");
    } else if (action.type == LayoutAction::Type::Speak
               && (action.text == itemLabel || action.text.isEmpty())) {
        preset = QStringLiteral("Speak the label");
    } else if (action.type == LayoutAction::Type::Command) {
        preset = QStringLiteral("Run command");
    } else if (action.type == LayoutAction::Type::OpenLayout) {
        preset = QStringLiteral("Open layout");
    } else if (action.type == LayoutAction::Type::LoadLayout) {
        preset = QStringLiteral("Load layout");
    } else if (action.type == LayoutAction::Type::CloseLayout) {
        preset = QStringLiteral("Close layout");
    }
    const QStringList presets = {QStringLiteral("None"),
                                 QStringLiteral("Type the label"),
                                 QStringLiteral("Speak the label"),
                                 QStringLiteral("Run command"),
                                 QStringLiteral("Open layout"),
                                 QStringLiteral("Load layout"),
                                 QStringLiteral("Close layout"),
                                 QStringLiteral("Custom")};
    b.combo(form, QStringLiteral("Preset"), presets, preset, [apply, itemLabel](const QString& t) {
        apply(QStringLiteral("Action preset"), [&](LayoutAction& a) {
            a = LayoutAction{};
            if (t == QLatin1String("Type the label")) {
                a.type = LayoutAction::Type::TypeText;
                a.text = itemLabel;
            } else if (t == QLatin1String("Speak the label")) {
                a.type = LayoutAction::Type::Speak;
                a.text = itemLabel;
            } else if (t == QLatin1String("Run command")) {
                a.type = LayoutAction::Type::Command;
            } else if (t == QLatin1String("Open layout")) {
                a.type = LayoutAction::Type::OpenLayout;
            } else if (t == QLatin1String("Load layout")) {
                a.type = LayoutAction::Type::LoadLayout;
            } else if (t == QLatin1String("Close layout")) {
                a.type = LayoutAction::Type::CloseLayout;
            } else if (t == QLatin1String("Custom")) {
                a.type = LayoutAction::Type::Command;
            }
        });
    });
    if (preset == QLatin1String("Run command") || preset == QLatin1String("Custom")) {
        QStringList ids = catalog.commands;
        QStringList labels = catalog.commandLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        if (!action.name.isEmpty() && !ids.contains(action.name)) {
            ids.prepend(action.name);
            labels.prepend(friendlyCommandLabel(action.name));
        }
        if (ids.isEmpty()) {
            b.text(form, QStringLiteral("Command"), action.name, [apply](const QString& t) {
                apply(QStringLiteral("Command name"), [&](LayoutAction& a) {
                    a.type = LayoutAction::Type::Command;
                    a.name = t;
                });
            });
        } else {
            b.comboValues(form, QStringLiteral("Command"), labels, ids,
                          action.name.isEmpty() ? ids.first() : action.name,
                          [apply](const QString& t) {
                              apply(QStringLiteral("Command name"), [&](LayoutAction& a) {
                                  a.type = LayoutAction::Type::Command;
                                  a.name = t;
                              });
                          });
        }
    }
    if (preset == QLatin1String("Open layout") || preset == QLatin1String("Load layout")
        || preset == QLatin1String("Custom")) {
        QStringList ids = catalog.layoutIds;
        QStringList labels = catalog.layoutLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        if (!action.layoutId.isEmpty() && !ids.contains(action.layoutId)) {
            ids.prepend(action.layoutId);
            labels.prepend(action.layoutId);
        }
        if (ids.isEmpty()) {
            b.text(form, QStringLiteral("Layout"), action.layoutId, [apply](const QString& t) {
                apply(QStringLiteral("Action layout"), [&](LayoutAction& a) {
                    if (a.type != LayoutAction::Type::LoadLayout) {
                        a.type = LayoutAction::Type::OpenLayout;
                    }
                    a.layoutId = t;
                });
            });
        } else {
            b.comboValues(form, QStringLiteral("Layout"), labels, ids,
                          action.layoutId.isEmpty() ? ids.first() : action.layoutId,
                          [apply](const QString& t) {
                              apply(QStringLiteral("Action layout"), [&](LayoutAction& a) {
                                  if (a.type != LayoutAction::Type::LoadLayout) {
                                      a.type = LayoutAction::Type::OpenLayout;
                                  }
                                  a.layoutId = t;
                              });
                          });
        }
    }
    if (preset == QLatin1String("Custom") || preset == QLatin1String("Speak the label")
        || preset == QLatin1String("Type the label")) {
        b.text(form, QStringLiteral("Text"), action.text, [apply](const QString& t) {
            apply(QStringLiteral("Action text"), [&](LayoutAction& a) { a.text = t; });
        });
    }
    if (preset == QLatin1String("Custom")) {
        QStringList types = {QStringLiteral("(none)")};
        types.append(LayoutSchema::actionTypeNames());
        const QString cur = action.type == LayoutAction::Type::Unknown
                                ? QStringLiteral("(none)")
                                : LayoutSchema::actionTypeName(action.type);
        b.combo(form, QStringLiteral("Type"), types, cur, [apply](const QString& t) {
            apply(QStringLiteral("Action type"), [&](LayoutAction& a) {
                a.type = LayoutSchema::actionTypeFromName(t);
            });
        });
        b.text(form, QStringLiteral("Script"), action.source, [apply](const QString& t) {
            apply(QStringLiteral("Script"), [&](LayoutAction& a) { a.source = t; });
        });
        b.integer(form, QStringLiteral("Delay ms"), action.delayMs, 0, 10000, [apply](int v) {
            apply(QStringLiteral("Delay"), [&](LayoutAction& a) { a.delayMs = v; });
        });
    }
}

QStringList visibleWhenChoices()
{
    return {QString(), QStringLiteral("expanded"), QStringLiteral("!expanded"),
            QStringLiteral("quitConfirm"), QStringLiteral("!quitConfirm"),
            QStringLiteral("dwellSuspend"), QStringLiteral("!dwellSuspend")};
}

QStringList iconChoices()
{
    QStringList names = MouseIcons::names();
    names.prepend(QString());
    return names;
}

QString friendlyCommandLabel(const QString& commandId)
{
    static const QHash<QString, QString> k = {
        {QStringLiteral("expandMaster"), QStringLiteral("Open drawer")},
        {QStringLiteral("collapseMaster"), QStringLiteral("Close drawer")},
        {QStringLiteral("closeOtherViews"), QStringLiteral("Close other boards")},
        {QStringLiteral("quitApp"), QStringLiteral("Quit Gazer")},
        {QStringLiteral("toggleDwellSuspend"), QStringLiteral("Pause / resume dwell")},
        {QStringLiteral("openLayoutEditor"), QStringLiteral("Layout editor")},
        {QStringLiteral("openPreview"), QStringLiteral("Head preview")},
        {QStringLiteral("tab"), QStringLiteral("Tab key")},
        {QStringLiteral("enter"), QStringLiteral("Enter key")},
        {QStringLiteral("backspace"), QStringLiteral("Backspace")},
        {QStringLiteral("space"), QStringLiteral("Space")},
        {QStringLiteral("escape"), QStringLiteral("Escape")},
        {QStringLiteral("mouseLeftClick"), QStringLiteral("Left click")},
        {QStringLiteral("mouseRightClick"), QStringLiteral("Right click")},
        {QStringLiteral("mouseDwellMove"), QStringLiteral("Dwell-move cursor")},
        {QStringLiteral("toggleMagnifier"), QStringLiteral("Magnifier")},
        {QStringLiteral("toggleLookToScroll"), QStringLiteral("Look to scroll")},
        {QStringLiteral("toggleGazeReticle"), QStringLiteral("Gaze reticle")},
        {QStringLiteral("toggleGazeMouseFollow"), QStringLiteral("Cursor follows gaze")},
    };
    return k.value(commandId, commandId);
}

void addActionSeriesFields(PropertyBinder& b, QFormLayout* form, const QVector<LayoutAction>& acts,
                           const QString& itemLabel, const ActionCatalog& catalog, int selectedStep,
                           const std::function<void(int)>& selectStep,
                           const std::function<void(QVector<LayoutAction>)>& applyAll)
{
    b.heading(form, QStringLiteral("Actions"));
    const int step = acts.isEmpty() ? 0 : qBound(0, selectedStep, acts.size() - 1);
    if (acts.size() > 1) {
        QStringList labels;
        for (int i = 0; i < acts.size(); ++i) {
            labels.push_back(QStringLiteral("Step %1").arg(i + 1));
        }
        b.combo(form, QStringLiteral("Step"), labels, labels[step],
                [selectStep, labels](const QString& t) {
                    selectStep(labels.indexOf(t));
                });
    }

    const LayoutAction current = acts.isEmpty() ? LayoutAction{} : acts[step];
    addActionFields(b, form, current, itemLabel, catalog,
                    [applyAll, acts, step](const QString&, const auto& mut) {
                        QVector<LayoutAction> next = acts;
                        if (next.isEmpty()) {
                            LayoutAction a;
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
                         QVector<LayoutAction> next = acts;
                         LayoutAction a;
                         a.type = LayoutAction::Type::Command;
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
                             QVector<LayoutAction> next = acts;
                             next.removeAt(step);
                             selectStep(qMax(0, step - 1));
                             applyAll(next);
                         });
        rowLay->addWidget(rm);
    }
    form->addRow(row);
}

} // namespace gazer
