#pragma once

#include "layout/LayoutTypes.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <optional>

class QFormLayout;
class QWidget;

namespace gazer {

/// Shared form widgets + chrome / dwell / action field groups for the inspector.
struct PropertyBinder {
    QWidget* host = nullptr;
    bool* loading = nullptr;

    [[nodiscard]] bool isLoading() const { return loading && *loading; }

    void heading(QFormLayout* form, const QString& text);
    void note(QFormLayout* form, const QString& text);
    void text(QFormLayout* form, const QString& label, const QString& value,
              const std::function<void(const QString&)>& apply);
    void integer(QFormLayout* form, const QString& label, int value, int min, int max,
                 const std::function<void(int)>& apply);
    void real(QFormLayout* form, const QString& label, double value, double min, double max,
              int decimals, const std::function<void(double)>& apply);
    void check(QFormLayout* form, const QString& label, bool value,
               const std::function<void(bool)>& apply);
    void combo(QFormLayout* form, const QString& label, const QStringList& items,
               const QString& current, const std::function<void(const QString&)>& apply);
    void comboValues(QFormLayout* form, const QString& label, const QStringList& labels,
                     const QStringList& values, const QString& currentValue,
                     const std::function<void(const QString&)>& apply);
    void color(QFormLayout* form, const QString& label, const std::optional<QColor>& value,
               const std::function<void(std::optional<QColor>)>& apply);
    void dim(QFormLayout* form, const QString& label, const DimSpec& value,
             const std::function<void(DimSpec)>& apply);
};

using ChromeMutate = std::function<void(const QString& undoLabel,
                                        const std::function<void(LayoutChromeStyle&)>& mut)>;
using DwellMutate = std::function<void(const QString& undoLabel,
                                       const std::function<void(LayoutDwellConfig&)>& mut)>;
using ActionMutate = std::function<void(const QString& undoLabel,
                                        const std::function<void(LayoutAction&)>& mut)>;

void addChromeFields(PropertyBinder& b, QFormLayout* form, const LayoutChromeStyle& st,
                     const ChromeMutate& apply);
void addDwellFields(PropertyBinder& b, QFormLayout* form, const LayoutDwellConfig& dwell,
                    const DwellMutate& apply);
struct ActionCatalog {
    QStringList commands;
    QStringList commandLabels;
    QStringList layoutIds;
    QStringList layoutLabels;
};

void addActionFields(PropertyBinder& b, QFormLayout* form, const LayoutAction& action,
                     const QString& itemLabel, const ActionCatalog& catalog,
                     const ActionMutate& apply);
void addActionSeriesFields(PropertyBinder& b, QFormLayout* form, const QVector<LayoutAction>& acts,
                           const QString& itemLabel, const ActionCatalog& catalog, int selectedStep,
                           const std::function<void(int)>& selectStep,
                           const std::function<void(QVector<LayoutAction>)>& applyAll);
[[nodiscard]] QString friendlyCommandLabel(const QString& commandId);

} // namespace gazer
