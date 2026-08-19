#include "editor/LayoutEditorFields.h"

#include "layout/LayoutSchema.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QColorDialog>
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

void addChromeFields(PropertyBinder& b, QFormLayout* form, const LayoutChromeStyle& st,
                     const ChromeMutate& apply)
{
    b.heading(form, QStringLiteral("Appearance"));
    b.color(form, QStringLiteral("Background"), st.background, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Background"), [&](LayoutChromeStyle& s) { s.background = c; });
    });
    b.color(form, QStringLiteral("Foreground"), st.foreground, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Foreground"), [&](LayoutChromeStyle& s) { s.foreground = c; });
    });
    b.color(form, QStringLiteral("Border"), st.borderColor, [apply](std::optional<QColor> c) {
        apply(QStringLiteral("Border"), [&](LayoutChromeStyle& s) { s.borderColor = c; });
    });
    b.real(form, QStringLiteral("Border width"), st.borderWidth.value_or(0.0), 0, 32, 1,
           [apply](double v) {
               apply(QStringLiteral("Border width"),
                     [&](LayoutChromeStyle& s) { s.borderWidth = v; });
           });
    b.real(form, QStringLiteral("Radius"), st.radius.value_or(14.0), 0, 64, 1, [apply](double v) {
        apply(QStringLiteral("Radius"), [&](LayoutChromeStyle& s) { s.radius = v; });
    });
    b.real(form, QStringLiteral("Blur"), st.blur.value_or(0.0), 0, 64, 1, [apply](double v) {
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
                    const DwellMutate& apply)
{
    b.heading(form, QStringLiteral("Dwell"));
    b.check(form, QStringLiteral("Enabled"), dwell.enabled, [apply](bool on) {
        apply(QStringLiteral("Dwell enabled"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.enabled = on;
        });
    });
    b.text(form, QStringLiteral("Ms sequence"), msSequenceText(dwell), [apply](const QString& t) {
        apply(QStringLiteral("Dwell ms"), [&](LayoutDwellConfig& d) { applyMsSequence(d, t); });
    });
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
    b.integer(form, QStringLiteral("Scan grace ms"), dwell.scanGraceMs, -1, 5000, [apply](int v) {
        apply(QStringLiteral("Scan grace"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.hasScanGrace = true;
            d.scanGraceMs = v;
        });
    });
    b.integer(form, QStringLiteral("Blink grace ms"), dwell.graceMs, -1, 5000, [apply](int v) {
        apply(QStringLiteral("Grace"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.hasGrace = true;
            d.graceMs = v;
        });
    });
    b.text(form, QStringLiteral("Progress color"), dwell.progressColor, [apply](const QString& t) {
        apply(QStringLiteral("Progress color"), [&](LayoutDwellConfig& d) {
            d.sectionPresent = true;
            d.hasProgressColor = !t.trimmed().isEmpty();
            d.progressColor = t.trimmed();
        });
    });
}

void addActionFields(PropertyBinder& b, QFormLayout* form, const LayoutAction& action,
                     const ActionMutate& apply)
{
    b.heading(form, QStringLiteral("Action"));
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
    b.text(form, QStringLiteral("Text"), action.text, [apply](const QString& t) {
        apply(QStringLiteral("Action text"), [&](LayoutAction& a) { a.text = t; });
    });
    b.text(form, QStringLiteral("Command"), action.name, [apply](const QString& t) {
        apply(QStringLiteral("Command name"), [&](LayoutAction& a) { a.name = t; });
    });
    b.text(form, QStringLiteral("Layout id"), action.layoutId, [apply](const QString& t) {
        apply(QStringLiteral("Action layout"), [&](LayoutAction& a) { a.layoutId = t; });
    });
    b.text(form, QStringLiteral("Script"), action.source, [apply](const QString& t) {
        apply(QStringLiteral("Script"), [&](LayoutAction& a) { a.source = t; });
    });
    b.integer(form, QStringLiteral("Delay ms"), action.delayMs, 0, 10000, [apply](int v) {
        apply(QStringLiteral("Delay"), [&](LayoutAction& a) { a.delayMs = v; });
    });
}

} // namespace gazer
