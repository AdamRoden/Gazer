#pragma once

#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorSession.h"

#include <QWidget>
#include <array>
#include <functional>

class QTabWidget;
class QFormLayout;
class QScrollArea;
class QLabel;

namespace gazer {

/// Right-hand inspector. Tabs follow the selected XML element.
class LayoutEditorProperties final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorProperties(LayoutEditorSession& session, QWidget* parent = nullptr);

    void showPageTab();
    void showGridTab();
    void showDwellTab();
    void showStyleTab();
    void showPlacementTab();
    void setActionCatalog(ActionCatalog catalog) { m_catalog = std::move(catalog); }

private:
    enum class Kind { Page, Grid, Cell, Zone, Style, Dwell };

    void rebuild();
    void syncHeader();
    void syncTabs(Kind kind);
    void fillPage(QFormLayout* form);
    void fillGrid(QFormLayout* form);
    void fillCell(QFormLayout* form);
    void fillZone(QFormLayout* form);
    void fillPlacement(QFormLayout* form);
    void fillStyle(QFormLayout* form);
    void fillDwell(QFormLayout* form);
    void fillAction(QFormLayout* form);
    void fillLeafIdentity(QFormLayout* form, const PageLeaf& item);
    void fillVisibleWhen(QFormLayout* form, const PageLeaf& item);
    void clearLayout(QFormLayout* form);

    void applyItem(const std::function<void(PageLeaf&)>& fn, const QString& undoLabel);
    void applyDoc(const std::function<void(PageDocument&)>& fn, const QString& undoLabel);
    void applyGrid(const std::function<void(PageGrid&)>& fn, const QString& undoLabel);
    void applyZone(const std::function<void(PageZone&)>& fn, const QString& undoLabel);
    void applyCell(const std::function<void(PageCell&)>& fn, const QString& undoLabel);

    [[nodiscard]] Kind currentKind() const;
    void selectStyleTab();
    void selectNamedStyle(const QString& id);
    void selectNamedDwell(const QString& id);
    void renameNamedStyle(const QString& from, const QString& to);
    void renameNamedDwell(const QString& from, const QString& to);

    struct Shape {
        Kind kind = Kind::Page;
        QString itemKey;
        bool autoClose = false;
        bool dwellTiming = false;
        int actionStep = 0;
        QString role;
        bool loop = false;
        bool customDwell = false;
        int itemActions = 0;
        bool gridNested = false;
        bool gridAutoClose = false;
        int namedStyles = 0;
        int namedDwells = 0;
        bool hasAction = false;
        PageActionType actionType = PageActionType::Unknown;
        PageVerb actionVerb = PageVerb::Open;
        PageTargetKind actionTargetKind = PageTargetKind::Page;
        PageZoomMode zoomMode = PageZoomMode::Off;
        PageMoveMode moveMode = PageMoveMode::Gaze;
        bool operator==(const Shape&) const = default;
    };
    struct Page {
        QFormLayout* form = nullptr;
        QScrollArea* scroll = nullptr;
    };

    [[nodiscard]] Shape currentShape() const;
    void rebuildIfNeeded();

    LayoutEditorSession& m_session;
    QLabel* m_headerKind = nullptr;
    QLabel* m_headerTitle = nullptr;
    QLabel* m_headerId = nullptr;
    QTabWidget* m_tabs = nullptr;
    static constexpr int kTabCount = 5;
    std::array<Page, kTabCount> m_pages{};
    bool m_loading = false;
    bool m_applying = false;
    Kind m_kind = Kind::Page;
    int m_actionStep = 0;
    Shape m_shape;
    ActionCatalog m_catalog;
};

} // namespace gazer
