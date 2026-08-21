#pragma once

class QAction;
class QTreeWidget;
class QTreeWidgetItem;

#include <QWidget>

namespace gazer {

class LayoutEditorSession;

/// Left rail: add-item actions, templates, and the board element tree.
class LayoutEditorToolbox final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorToolbox(QWidget* parent = nullptr);

    void bindSession(LayoutEditorSession& session);

    QTreeWidgetItem* addSection(const QString& title);
    void addAction(QTreeWidgetItem* section, QAction* action);

private:
    void rebuildHierarchy();

    LayoutEditorSession* m_session = nullptr;
    QTreeWidget* m_actions = nullptr;
    QTreeWidget* m_hierarchy = nullptr;
    QString m_treeKey;
};

} // namespace gazer
