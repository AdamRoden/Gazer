#include "editor/LayoutEditorFields.h"

#include "layout/ChromeBlur.h"
#include "layout/PageDim.h"
#include "ui/KeySymbols.h"
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
#include <QSizePolicy>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>

namespace gazer {

void PropertyBinder::fitWidth(QWidget* w)
{
    w->setMinimumWidth(0);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (auto* c = qobject_cast<QComboBox*>(w)) {
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(4);
    }
}

void PropertyBinder::heading(QFormLayout* form, const QString& text)
{
    auto* lab = new QLabel(text);
    lab->setObjectName(QStringLiteral("fieldHeading"));
    form->addRow(lab, new QWidget);
}

void PropertyBinder::note(QFormLayout* form, const QString& text)
{
    auto* lab = new QLabel(text);
    lab->setWordWrap(true);
    lab->setObjectName(QStringLiteral("fieldNote"));
    form->addRow(QString(), lab);
}

void PropertyBinder::text(QFormLayout* form, const QString& label, const QString& value,
                          const std::function<void(const QString&)>& apply)
{
    auto* e = new QLineEdit(value);
    fitWidth(e);
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
    fitWidth(s);
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
    fitWidth(s);
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
    fitWidth(c);
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
    fitWidth(c);
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
    fitWidth(e);
    fitWidth(row);
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

void PropertyBinder::dim(QFormLayout* form, const QString& label, const PageDim& value,
                         const std::function<void(PageDim)>& apply)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* unit = new QComboBox;
    unit->addItems({QStringLiteral("unset"), QStringLiteral("px"), QStringLiteral("prop"),
                    QStringLiteral("h-prop")});
    fitWidth(unit);
    fitWidth(row);
    if (!value.isSet()) {
        unit->setCurrentIndex(0);
    } else if (value.unit == PageDim::Unit::Pixels) {
        unit->setCurrentIndex(1);
    } else if (value.unit == PageDim::Unit::HeightProportion) {
        unit->setCurrentIndex(3);
    } else {
        unit->setCurrentIndex(2);
    }
    auto* spin = new QDoubleSpinBox;
    spin->setRange(-10000, 10000);
    spin->setDecimals(3);
    spin->setValue(value.value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setEnabled(unit->currentIndex() != 0);
    fitWidth(spin);
    bool* const loadingFlag = loading;
    auto commit = [loadingFlag, unit, spin, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        PageDim d;
        if (unit->currentIndex() == 1) {
            d = PageDim::pixels(spin->value());
        } else if (unit->currentIndex() == 2) {
            d = PageDim::proportion(spin->value());
        } else if (unit->currentIndex() == 3) {
            d = PageDim::heightProportion(spin->value());
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
    fitWidth(unit);
    fitWidth(row);
    unit->setCurrentIndex(value.has_value() ? 1 : 0);
    auto* spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value.value_or(0.0));
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setEnabled(value.has_value());
    fitWidth(spin);
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

void PropertyBinder::optionalBox(QFormLayout* form, const QString& label,
                                 const std::optional<PageBox>& value, const QString& placeholder,
                                 const std::function<void(std::optional<PageBox>)>& apply)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    auto* unit = new QComboBox;
    unit->addItems({QStringLiteral("inherit"), QStringLiteral("set")});
    fitWidth(unit);
    fitWidth(row);
    unit->setCurrentIndex(value && value->isSet() ? 1 : 0);
    auto* e = new QLineEdit(value && value->isSet() ? value->toToken() : QString());
    e->setPlaceholderText(placeholder);
    fitWidth(e);
    e->setEnabled(unit->currentIndex() != 0);
    bool* const loadingFlag = loading;
    auto commit = [loadingFlag, unit, e, apply]() {
        if (loadingFlag && *loadingFlag) {
            return;
        }
        if (unit->currentIndex() == 0) {
            apply(std::nullopt);
            return;
        }
        const PageBox box = PageBox::fromToken(e->text());
        apply(box.isSet() ? std::optional<PageBox>(box) : std::nullopt);
    };
    QObject::connect(unit, &QComboBox::currentIndexChanged, host, [e, commit](int idx) {
        e->setEnabled(idx != 0);
        commit();
    });
    QObject::connect(e, &QLineEdit::editingFinished, host, commit);
    h->addWidget(unit);
    h->addWidget(e, 1);
    form->addRow(label, row);
}

void addChromeFields(PropertyBinder& b, QFormLayout* form, const PageChrome& st,
                     const ChromeMutate& apply, bool includeHeading)
{
    if (includeHeading) {
        b.heading(form, QStringLiteral("Style"));
        b.note(form, QStringLiteral("Empty inherits the named style, then the page, then the theme."));
    }
    b.color(form, QStringLiteral("Background"), st.background, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Background"), [&](PageChrome& s) { s.background = c; });
    });
    b.color(form, QStringLiteral("Foreground"), st.foreground, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Foreground"), [&](PageChrome& s) { s.foreground = c; });
    });
    b.color(form, QStringLiteral("Border"), st.borderColor, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Border"), [&](PageChrome& s) { s.borderColor = c; });
    });
    b.optionalBox(form, QStringLiteral("Thickness"), st.thickness,
                  QStringLiteral("all  or  t,r,b,l"), [apply](std::optional<PageBox> v) {
                      apply(QStringLiteral("Thickness"), [&](PageChrome& s) { s.thickness = v; });
                  });
    b.optionalBox(form, QStringLiteral("Radius"), st.radius, QStringLiteral("all  or  tl,tr,br,bl"),
                  [apply](std::optional<PageBox> v) {
                      apply(QStringLiteral("Radius"), [&](PageChrome& s) { s.radius = v; });
                  });
    b.optionalReal(form, QStringLiteral("Frosted blur"), st.blur, 0, kChromeBlurMax, 1,
                   [apply](std::optional<double> v) {
                       apply(QStringLiteral("Blur"), [&](PageChrome& s) { s.blur = v; });
                   });
    b.text(form, QStringLiteral("Progress style"),
           st.progressStyle ? st.progressStyle->toCsv() : QString(),
           [apply](const QString& raw) {
               const QString s = raw.trimmed();
               apply(QStringLiteral("Progress style"), [&](PageChrome& c) {
                   c.progressStyle =
                       s.isEmpty() ? std::nullopt : std::optional<ProgressStyle>(ProgressStyle::fromCsv(s));
               });
           });
    b.note(form, QStringLiteral("Empty inherits. Tokens: radial, border, fill, fillup, "
                                "filldown, fillleft, fillright."));
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

QString pageActionTypeName(PageActionType t)
{
    switch (t) {
    case PageActionType::Send:
        return QStringLiteral("Send");
    case PageActionType::Page:
        return QStringLiteral("Page");
    case PageActionType::Click:
        return QStringLiteral("Click");
    case PageActionType::Move:
        return QStringLiteral("Move");
    case PageActionType::MoveAndClick:
        return QStringLiteral("MoveAndClick");
    case PageActionType::Command:
        return QStringLiteral("Command");
    case PageActionType::Speak:
        return QStringLiteral("Speak");
    case PageActionType::Ahk:
        return QStringLiteral("AHK");
    case PageActionType::Unknown:
        break;
    }
    return QStringLiteral("(none)");
}

PageActionType pageActionTypeFromName(const QString& s)
{
    const QString n = s.trimmed().toLower();
    if (n == QLatin1String("send")) {
        return PageActionType::Send;
    }
    if (n == QLatin1String("page")) {
        return PageActionType::Page;
    }
    if (n == QLatin1String("click")) {
        return PageActionType::Click;
    }
    if (n == QLatin1String("move")) {
        return PageActionType::Move;
    }
    if (n == QLatin1String("moveandclick")) {
        return PageActionType::MoveAndClick;
    }
    if (n == QLatin1String("command")) {
        return PageActionType::Command;
    }
    if (n == QLatin1String("speak")) {
        return PageActionType::Speak;
    }
    if (n == QLatin1String("ahk")) {
        return PageActionType::Ahk;
    }
    return PageActionType::Unknown;
}

} // namespace

void addDwellFields(PropertyBinder& b, QFormLayout* form, const PageDwell& dwell,
                    const DwellMutate& apply, bool includeHeading, const QStringList& inheritIds,
                    const QString& inheritCurrent,
                    const std::function<void(const QString&)>& inheritApply)
{
    if (includeHeading) {
        b.heading(form, QStringLiteral("Dwell"));
    }
    if (inheritApply) {
        addOptionalIdCombo(b, form, QStringLiteral("Inherit"), inheritIds, inheritCurrent,
                           inheritApply);
    }
    b.integer(form, QStringLiteral("Scan grace ms"), dwell.scanGrace.value_or(-1), -1, 5000,
              [apply](int v) {
                  apply(QStringLiteral("Scan grace"), [&](PageDwell& d) {
                      if (v < 0) {
                          d.scanGrace.reset();
                      } else {
                          d.scanGrace = v;
                      }
                  });
              });
    b.integer(form, QStringLiteral("Dwell grace ms"), dwell.dwellGrace.value_or(-1), -1, 5000,
              [apply](int v) {
                  apply(QStringLiteral("Dwell grace"), [&](PageDwell& d) {
                      if (v < 0) {
                          d.dwellGrace.reset();
                      } else {
                          d.dwellGrace = v;
                      }
                  });
              });
    b.text(form, QStringLiteral("Hold times (ms)"), activationText(dwell), [apply](const QString& t) {
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
    b.note(form, QStringLiteral("Comma-separated hold steps. Empty inherits Settings. −1 grace inherits."));
}

void addActionFields(PropertyBinder& b, QFormLayout* form, const PageAction& action,
                     const QString& itemLabel, const ActionCatalog& catalog,
                     const ActionMutate& apply)
{
    b.heading(form, QStringLiteral("Do this"));
    const QStringList types = {QStringLiteral("(none)"), QStringLiteral("Send"),
                               QStringLiteral("Page"), QStringLiteral("Command"),
                               QStringLiteral("Speak"), QStringLiteral("Click"),
                               QStringLiteral("Move"), QStringLiteral("MoveAndClick"),
                               QStringLiteral("AHK")};
    b.combo(form, QStringLiteral("Type"), types, pageActionTypeName(action.type),
            [apply](const QString& t) {
                apply(QStringLiteral("Action type"),
                      [&](PageAction& a) { a.type = pageActionTypeFromName(t); });
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
    if (action.type == PageActionType::Page) {
        QString verb = QStringLiteral("Open");
        if (action.verb == PageVerb::Close) {
            verb = QStringLiteral("Close");
        } else if (action.verb == PageVerb::Toggle) {
            verb = QStringLiteral("Toggle");
        }
        b.combo(form, QStringLiteral("Verb"),
                {QStringLiteral("Open"), QStringLiteral("Close"), QStringLiteral("Toggle")}, verb,
                [apply](const QString& t) {
                    apply(QStringLiteral("Page verb"), [&](PageAction& a) {
                        a.type = PageActionType::Page;
                        if (t == QLatin1String("Close")) {
                            a.verb = PageVerb::Close;
                        } else if (t == QLatin1String("Toggle")) {
                            a.verb = PageVerb::Toggle;
                        } else {
                            a.verb = PageVerb::Open;
                        }
                    });
                });
        QString kind = QStringLiteral("Page");
        if (action.targetKind == PageTargetKind::Grid) {
            kind = QStringLiteral("Grid");
        } else if (action.targetKind == PageTargetKind::Zone) {
            kind = QStringLiteral("Zone");
        }
        b.combo(form, QStringLiteral("Target kind"),
                {QStringLiteral("Page"), QStringLiteral("Grid"), QStringLiteral("Zone")}, kind,
                [apply](const QString& t) {
                    apply(QStringLiteral("Page kind"), [&](PageAction& a) {
                        a.type = PageActionType::Page;
                        if (t == QLatin1String("Grid")) {
                            a.targetKind = PageTargetKind::Grid;
                        } else if (t == QLatin1String("Zone")) {
                            a.targetKind = PageTargetKind::Zone;
                        } else {
                            a.targetKind = PageTargetKind::Page;
                        }
                    });
                });
        QStringList ids = catalog.layoutIds;
        QStringList labels = catalog.layoutLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        ids.prepend(QStringLiteral("self"));
        labels.prepend(QStringLiteral("self"));
        if (!action.targetId.isEmpty() && !ids.contains(action.targetId)) {
            ids.prepend(action.targetId);
            labels.prepend(action.targetId);
        }
        b.comboValues(form, QStringLiteral("Target id"), labels, ids, action.targetId,
                      [apply](const QString& t) {
                          apply(QStringLiteral("Page target"), [&](PageAction& a) {
                              a.type = PageActionType::Page;
                              a.targetId = t;
                          });
                      });
    }
    if (action.type == PageActionType::Speak) {
        b.text(form, QStringLiteral("Text"), action.speakText, [apply](const QString& t) {
            apply(QStringLiteral("Speak text"), [&](PageAction& a) {
                a.type = PageActionType::Speak;
                a.speakText = t;
            });
        });
    }
    if (action.type == PageActionType::Click || action.type == PageActionType::MoveAndClick) {
        b.combo(form, QStringLiteral("Button"),
                {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("middle")},
                action.button.isEmpty() ? QStringLiteral("left") : action.button,
                [apply](const QString& t) {
                    apply(QStringLiteral("Click button"), [&](PageAction& a) { a.button = t; });
                });
        b.integer(form, QStringLiteral("Count"), action.clickCount, 1, 16, [apply](int v) {
            apply(QStringLiteral("Click count"), [&](PageAction& a) { a.clickCount = v; });
        });
        b.combo(form, QStringLiteral("Click edge"),
                {QString(), QStringLiteral("Down"), QStringLiteral("Up")}, action.clickEdge,
                [apply](const QString& t) {
                    apply(QStringLiteral("Click edge"), [&](PageAction& a) { a.clickEdge = t; });
                });
        b.integer(form, QStringLiteral("Speed"), action.speed, 0, 10000, [apply](int v) {
            apply(QStringLiteral("Click speed"), [&](PageAction& a) { a.speed = v; });
        });
        if (action.type == PageActionType::MoveAndClick) {
            b.integer(form, QStringLiteral("Zoom"), action.zoomLevel, 0, 16, [apply](int v) {
                apply(QStringLiteral("Zoom"), [&](PageAction& a) { a.zoomLevel = v; });
            });
        }
    }
    if (action.type == PageActionType::Move) {
        QString mode = QStringLiteral("Gaze");
        if (action.moveMode == PageMoveMode::Absolute) {
            mode = QStringLiteral("Absolute");
        } else if (action.moveMode == PageMoveMode::Relative) {
            mode = QStringLiteral("Relative");
        }
        b.combo(form, QStringLiteral("Mode"),
                {QStringLiteral("Gaze"), QStringLiteral("Absolute"), QStringLiteral("Relative")},
                mode, [apply](const QString& t) {
                    apply(QStringLiteral("Move mode"), [&](PageAction& a) {
                        a.type = PageActionType::Move;
                        if (t == QLatin1String("Absolute")) {
                            a.moveMode = PageMoveMode::Absolute;
                        } else if (t == QLatin1String("Relative")) {
                            a.moveMode = PageMoveMode::Relative;
                        } else {
                            a.moveMode = PageMoveMode::Gaze;
                        }
                    });
                });
        if (action.moveMode == PageMoveMode::Gaze) {
            b.note(form, QStringLiteral("Jumps the cursor to the last gaze sample. Use Command "
                                       "mouseDwellMove for dwell-to-place."));
        }
        if (action.moveMode != PageMoveMode::Gaze) {
            b.dim(form, QStringLiteral("X"), action.moveX, [apply](PageDim v) {
                apply(QStringLiteral("Move X"), [&](PageAction& a) { a.moveX = v; });
            });
            b.dim(form, QStringLiteral("Y"), action.moveY, [apply](PageDim v) {
                apply(QStringLiteral("Move Y"), [&](PageAction& a) { a.moveY = v; });
            });
            b.integer(form, QStringLiteral("Speed"), action.speed, 0, 10000, [apply](int v) {
                apply(QStringLiteral("Move speed"), [&](PageAction& a) { a.speed = v; });
            });
            b.integer(form, QStringLiteral("Zoom"), action.zoomLevel, 0, 16, [apply](int v) {
                apply(QStringLiteral("Zoom"), [&](PageAction& a) { a.zoomLevel = v; });
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
}

QStringList visibleWhenChoices()
{
    return {QString(), QStringLiteral("expanded"), QStringLiteral("!expanded"),
            QStringLiteral("quitConfirm"), QStringLiteral("!quitConfirm"),
            QStringLiteral("dwellSuspend"), QStringLiteral("!dwellSuspend")};
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
        {QStringLiteral("closeOtherViews"), QStringLiteral("Close other pages")},
        {QStringLiteral("quitApp"), QStringLiteral("Quit Gazer")},
        {QStringLiteral("toggleDwellSuspend"), QStringLiteral("Pause / resume dwell")},
        {QStringLiteral("openLayoutEditor"), QStringLiteral("Page editor")},
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

    const PageAction current = acts.isEmpty() ? PageAction{} : acts[step];
    addActionFields(b, form, current, itemLabel, catalog,
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
    form->addRow(QStringLiteral("Steps"), row);
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
    addChromeFields(b, form, styles.value(selectedId), apply, false);
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
