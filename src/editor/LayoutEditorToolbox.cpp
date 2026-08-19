#include "editor/LayoutEditorToolbox.h"

#include <QAction>
#include <QHeaderView>
#include <QVariant>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace gazer {

LayoutEditorToolbox::LayoutEditorToolbox(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* title = new QLabel(QStringLiteral("Toolbox"));
    title->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(title);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(14);
    m_tree->setAnimated(true);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->header()->setStretchLastSection(true);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem* item, int) {
        if (!item) {
            return;
        }
        if (auto* action = item->data(0, Qt::UserRole).value<QAction*>()) {
            action->trigger();
        }
    });
}

QTreeWidgetItem* LayoutEditorToolbox::addSection(const QString& title)
{
    auto* item = new QTreeWidgetItem(m_tree, {title});
    item->setFlags(Qt::ItemIsEnabled);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    item->setExpanded(true);
    return item;
}

void LayoutEditorToolbox::addAction(QTreeWidgetItem* section, QAction* action)
{
    if (!section || !action) {
        return;
    }
    const QString label = action->text().remove(QLatin1Char('&'));
    auto* item = new QTreeWidgetItem(section, {label});
    item->setData(0, Qt::UserRole, QVariant::fromValue(action));
    item->setDisabled(!action->isEnabled());
    connect(action, &QAction::changed, m_tree, [item, action]() {
        item->setDisabled(!action->isEnabled());
        item->setText(0, action->text().remove(QLatin1Char('&')));
    });
}

} // namespace gazer
