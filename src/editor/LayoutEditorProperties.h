#pragma once

#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorSession.h"

#include <QWidget>
#include <array>
#include <functional>

class QTabWidget;
class QFormLayout;
class QScrollArea;

namespace gazer {

/// Right-hand inspector. Board selection: Board / Window / Grid / Gaze.
/// Item selection: Item / Layout / Action.
class LayoutEditorProperties final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorProperties(LayoutEditorSession& session, QWidget* parent = nullptr);

    void showBoardTab();
    void showWindowTab();
    void showGridTab();
    void showDwellTab();
    void showStyleTab();
    void showLayoutTab();
    void showInteractionTab();
    void setActionCatalog(ActionCatalog catalog) { m_catalog = std::move(catalog); }

private:
    void rebuild();
    void syncTabs(bool itemSelected);
    void fillBoard(QFormLayout* form);
    void fillWindow(QFormLayout* form);
    void fillGrid(QFormLayout* form);
    void fillBoardDwell(QFormLayout* form);
    void fillItem(QFormLayout* form);
    void fillItemLayout(QFormLayout* form);
    void fillItemAction(QFormLayout* form);
    void clearLayout(QFormLayout* form);

    void applyItem(const std::function<void(LayoutItem&)>& fn, const QString& undoLabel);
    void applyDoc(const std::function<void(LayoutDocument&)>& fn, const QString& undoLabel);
    struct Shape {
        bool item = false;
        QString itemKey;
        bool windowShown = false;
        bool autoClose = false;
        bool dwellTiming = false;
        bool dwellProgress = false;
        int children = 0;
        int hook = 0;
        int hookSteps = 0;
        int actionStep = 0;
        int actionType = -1;
        bool unbounded = false;
        QString role;
        bool loop = false;
        bool customDwell = false;
        bool itemTiming = false;
        bool itemProgress = false;
        bool embed = false;
        int itemActions = 0;
        bool operator==(const Shape&) const = default;
    };
    struct Page {
        QFormLayout* form = nullptr;
        QScrollArea* scroll = nullptr;
    };

    [[nodiscard]] Shape currentShape() const;
    void rebuildIfNeeded();

    LayoutEditorSession& m_session;
    QTabWidget* m_tabs = nullptr;
    std::array<Page, 4> m_pages{};
    bool m_loading = false;
    bool m_applying = false;
    bool m_itemMode = false;
    int m_actionStep = 0;
    int m_lifecycleHook = 0;
    Shape m_shape;
    ActionCatalog m_catalog;
};

} // namespace gazer
