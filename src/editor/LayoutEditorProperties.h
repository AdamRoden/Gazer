#pragma once

#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorSession.h"

#include <QWidget>
#include <functional>

class QTabWidget;
class QTreeWidget;
class QFormLayout;

namespace gazer {

/// Right-hand inspector: Style / Layout / Interaction plus the item hierarchy.
class LayoutEditorProperties final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorProperties(LayoutEditorSession& session, QWidget* parent = nullptr);

    void showStyleTab();
    void showLayoutTab();
    void showInteractionTab();
    void setActionCatalog(ActionCatalog catalog) { m_catalog = std::move(catalog); }

private:
    void rebuild();
    void rebuildHierarchy();
    void fillStyle(QFormLayout* form);
    void fillLayout(QFormLayout* form);
    void fillInteraction(QFormLayout* form);
    void clearLayout(QFormLayout* form);

    void applyItem(const std::function<void(LayoutItem&)>& fn, const QString& undoLabel);
    void applyDoc(const std::function<void(LayoutDocument&)>& fn, const QString& undoLabel);

    LayoutEditorSession& m_session;
    QTabWidget* m_tabs = nullptr;
    QFormLayout* m_styleForm = nullptr;
    QFormLayout* m_layoutForm = nullptr;
    QFormLayout* m_interactForm = nullptr;
    QTreeWidget* m_hierarchy = nullptr;
    bool m_loading = false;
    bool m_applying = false;
    int m_actionStep = 0;
    ActionCatalog m_catalog;
};

} // namespace gazer
