#pragma once

#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorSession.h"

#include <QWidget>
#include <functional>

class QTabWidget;
class QFormLayout;

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

    LayoutEditorSession& m_session;
    QTabWidget* m_tabs = nullptr;
    QFormLayout* m_form0 = nullptr;
    QFormLayout* m_form1 = nullptr;
    QFormLayout* m_form2 = nullptr;
    QFormLayout* m_form3 = nullptr;
    bool m_loading = false;
    bool m_applying = false;
    bool m_itemMode = false;
    int m_actionStep = 0;
    ActionCatalog m_catalog;
};

} // namespace gazer
