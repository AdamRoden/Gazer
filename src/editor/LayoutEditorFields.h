#pragma once

#include "layout/PageTypes.h"

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
    void card(QFormLayout* form, const QString& title,
              const std::function<void(QFormLayout*)>& fill);
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
    void dim(QFormLayout* form, const QString& label, const PageDim& value,
             const std::function<void(PageDim)>& apply);
    void optionalReal(QFormLayout* form, const QString& label, const std::optional<double>& value,
                      double min, double max, int decimals,
                      const std::function<void(std::optional<double>)>& apply);
    void optionalBox(QFormLayout* form, const QString& label, const std::optional<PageBox>& value,
                     const QString& placeholder,
                     const std::function<void(std::optional<PageBox>)>& apply);

private:
    static void fitWidth(QWidget* w);
};

using ChromeMutate = std::function<void(const QString& undoLabel,
                                        const std::function<void(PageChrome&)>& mut)>;
using DwellMutate = std::function<void(const QString& undoLabel,
                                       const std::function<void(PageDwell&)>& mut)>;
using ActionMutate = std::function<void(const QString& undoLabel,
                                        const std::function<void(PageAction&)>& mut)>;

void addChromeFields(PropertyBinder& b, QFormLayout* form, const PageChrome& st,
                     const ChromeMutate& apply, bool includeItemPaint = true);
void addOffsetSizeFields(PropertyBinder& b, QFormLayout* form, const PageDimPair& offset,
                         const PageDimPair& size, const std::function<void(PageDim)>& applyOffX,
                         const std::function<void(PageDim)>& applyOffY,
                         const std::function<void(PageDim)>& applyW,
                         const std::function<void(PageDim)>& applyH);
void addPlacementGeometry(PropertyBinder& b, QFormLayout* form, bool desktopMode, PageAnchor anchor,
                          const PageDimPair& offset, const PageDimPair& size,
                          const std::function<void(bool)>& applyDesktop,
                          const std::function<void(const QString&)>& applyAnchor,
                          const std::function<void(PageDim)>& applyOffX,
                          const std::function<void(PageDim)>& applyOffY,
                          const std::function<void(PageDim)>& applyW,
                          const std::function<void(PageDim)>& applyH);
void addDwellFields(PropertyBinder& b, QFormLayout* form, const PageDwell& dwell,
                    const DwellMutate& apply, bool includeHeading = true,
                    const QStringList& inheritIds = {}, const QString& inheritCurrent = {},
                    const std::function<void(const QString&)>& inheritApply = {});
struct ActionCatalog {
    QStringList commands;
    QStringList commandLabels;
    QStringList layoutIds;
    QStringList layoutLabels;
};

void addActionFields(PropertyBinder& b, QFormLayout* form, const PageAction& action,
                     const QString& itemLabel, const ActionCatalog& catalog,
                     const ActionMutate& apply);
void addActionSeriesFields(PropertyBinder& b, QFormLayout* form, const QVector<PageAction>& acts,
                           const QString& itemLabel, const ActionCatalog& catalog, int selectedStep,
                           const std::function<void(int)>& selectStep,
                           const std::function<void(QVector<PageAction>)>& applyAll);
void addOptionalIdCombo(PropertyBinder& b, QFormLayout* form, const QString& label,
                        const QStringList& ids, const QString& current,
                        const std::function<void(const QString&)>& apply);
void addNamedChromeEditor(PropertyBinder& b, QFormLayout* form,
                          const QHash<QString, PageChrome>& styles, const QString& selectedId,
                          const std::function<void(const QString&)>& selectId,
                          const std::function<void()>& addNew,
                          const std::function<void(const QString& from, const QString& to)>& rename,
                          const std::function<void(const QString&)>& remove,
                          const ChromeMutate& apply);
void addNamedDwellEditor(PropertyBinder& b, QFormLayout* form,
                         const QHash<QString, PageDwell>& dwells, const QString& selectedId,
                         const std::function<void(const QString&)>& selectId,
                         const std::function<void()>& addNew,
                         const std::function<void(const QString& from, const QString& to)>& rename,
                         const std::function<void(const QString&)>& remove,
                         const DwellMutate& apply);
[[nodiscard]] QString friendlyCommandLabel(const QString& commandId);
[[nodiscard]] QStringList visibleWhenChoices();
[[nodiscard]] QStringList iconChoices();
[[nodiscard]] QStringList pageAnchorNames();
[[nodiscard]] QStringList sortedKeys(const QStringList& keys);

} // namespace gazer
