#pragma once

#include <QWidget>

class QAction;
class QTreeWidget;
class QTreeWidgetItem;

namespace gazer {

/// Left-hand action catalog bound to the window's QAction objects.
class LayoutEditorToolbox final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorToolbox(QWidget* parent = nullptr);

    QTreeWidgetItem* addSection(const QString& title);
    void addAction(QTreeWidgetItem* section, QAction* action);

private:
    QTreeWidget* m_tree = nullptr;
};

} // namespace gazer
