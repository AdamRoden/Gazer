#include "editor/LayoutEditorFields.h"

#include "layout/ChromeBlur.h"
#include "layout/PageActionParse.h"
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
                    QStringLiteral("h-prop"), QStringLiteral("expr")});
    fitWidth(unit);
    fitWidth(row);
    const bool isExpr = value.unit == PageDim::Unit::Expression;
    if (!value.isSet()) {
        unit->setCurrentIndex(0);
    } else if (value.unit == PageDim::Unit::Pixels) {
        unit->setCurrentIndex(1);
    } else if (value.unit == PageDim::Unit::HeightProportion) {
        unit->setCurrentIndex(3);
    } else if (isExpr) {
        unit->setCurrentIndex(4);
    } else {
        unit->setCurrentIndex(2);
    }
    auto* spin = new QDoubleSpinBox;
    spin->setRange(-10000, 10000);
    spin->setDecimals(3);
    spin->setValue(value.value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spin->setEnabled(unit->currentIndex() != 0 && !isExpr);
    fitWidth(spin);
    auto* expr = new QLineEdit(value.expr);
    expr->setPlaceholderText(QStringLiteral("A_ScreenHeight/9*16"));
    fitWidth(expr);
    expr->setVisible(isExpr);
    spin->setVisible(!isExpr);
    bool* const loadingFlag = loading;
    auto commit = [loadingFlag, unit, spin, expr, apply]() {
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
        } else if (unit->currentIndex() == 4) {
            QString err;
            d = PageDimParse::parse(expr->text(), &err);
            if (d.unit == PageDim::Unit::Unset) {
                return;
            }
        }
        apply(d);
    };
    auto sync = [unit, spin, expr]() {
        const bool exprOn = unit->currentIndex() == 4;
        spin->setVisible(!exprOn);
        expr->setVisible(exprOn);
        spin->setEnabled(unit->currentIndex() > 0 && !exprOn);
        expr->setEnabled(exprOn);
    };
    QObject::connect(unit, &QComboBox::currentIndexChanged, host, [sync, commit](int) {
        sync();
        commit();
    });
    QObject::connect(spin, &QDoubleSpinBox::editingFinished, host, commit);
    QObject::connect(expr, &QLineEdit::editingFinished, host, commit);
    h->addWidget(unit);
    h->addWidget(spin, 1);
    h->addWidget(expr, 1);
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

} // namespace gazer
