#include "editor/LayoutEditorToolbox.h"

#include "editor/LayoutEditorSession.h"

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVariant>
#include <QVector>
#include <QVBoxLayout>

namespace gazer {

LayoutEditorToolbox::LayoutEditorToolbox(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* addTitle = new QLabel(QStringLiteral("Add"));
    addTitle->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(addTitle);

    m_actions = new QTreeWidget(this);
    m_actions->setHeaderHidden(true);
    m_actions->setRootIsDecorated(true);
    m_actions->setIndentation(14);
    m_actions->setAnimated(true);
    m_actions->setFocusPolicy(Qt::NoFocus);
    m_actions->header()->setStretchLastSection(true);
    m_actions->setMaximumHeight(280);
    layout->addWidget(m_actions);

    connect(m_actions, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem* item, int) {
        if (!item) {
            return;
        }
        if (auto* action = item->data(0, Qt::UserRole).value<QAction*>()) {
            action->trigger();
        }
    });

    auto* elTitle = new QLabel(QStringLiteral("Elements"));
    elTitle->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(elTitle);

    m_hierarchy = new QTreeWidget(this);
    m_hierarchy->setHeaderHidden(true);
    m_hierarchy->setRootIsDecorated(true);
    m_hierarchy->setIndentation(14);
    m_hierarchy->header()->setStretchLastSection(true);
    layout->addWidget(m_hierarchy, 1);

    connect(m_hierarchy, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || !m_session) {
            return;
        }
        const QString kind = item->data(0, Qt::UserRole).toString();
        const QString id = item->data(0, Qt::UserRole + 1).toString();
        if (kind == QLatin1String("item")) {
            m_session->selectItem(id);
        } else {
            m_session->selectTarget(EditorTarget::Document);
        }
    });
    m_hierarchy->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_hierarchy, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_session) {
            return;
        }
        QTreeWidgetItem* item = m_hierarchy->itemAt(pos);
        if (item && item->data(0, Qt::UserRole).toString() == QLatin1String("item")) {
            const QString id = item->data(0, Qt::UserRole + 1).toString();
            if (!m_session->isItemSelected(id)) {
                m_session->selectItem(id);
            }
        }
        QMenu menu(this);
        auto* dup = menu.addAction(QStringLiteral("Duplicate"));
        auto* del = menu.addAction(QStringLiteral("Delete"));
        menu.addSeparator();
        auto* toFree = menu.addAction(QStringLiteral("Convert to free item"));
        auto* toCell = menu.addAction(QStringLiteral("Convert to cell"));
        menu.addSeparator();
        auto* raise = menu.addAction(QStringLiteral("Bring forward"));
        auto* lower = menu.addAction(QStringLiteral("Send backward"));
        const bool has = m_session->selection().target == EditorTarget::Item;
        for (QAction* a : {dup, del, toFree, toCell, raise, lower}) {
            a->setEnabled(has);
        }
        QAction* chosen = menu.exec(m_hierarchy->mapToGlobal(pos));
        if (chosen == dup) {
            m_session->duplicateSelected();
        } else if (chosen == del) {
            m_session->deleteSelected();
        } else if (chosen == toFree) {
            m_session->convertSelectedToFree();
        } else if (chosen == toCell) {
            m_session->convertSelectedToCell();
        } else if (chosen == raise) {
            m_session->raiseSelected();
        } else if (chosen == lower) {
            m_session->lowerSelected();
        }
    });
}

void LayoutEditorToolbox::bindSession(LayoutEditorSession& session)
{
    m_session = &session;
    connect(m_session, &LayoutEditorSession::selectionChanged, this,
            &LayoutEditorToolbox::rebuildHierarchy);
    connect(m_session, &LayoutEditorSession::documentChanged, this,
            &LayoutEditorToolbox::rebuildHierarchy);
    rebuildHierarchy();
}

QTreeWidgetItem* LayoutEditorToolbox::addSection(const QString& title)
{
    auto* item = new QTreeWidgetItem(m_actions, {title});
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
    connect(action, &QAction::changed, m_actions, [item, action]() {
        item->setDisabled(!action->isEnabled());
        item->setText(0, action->text().remove(QLatin1Char('&')));
    });
}

void LayoutEditorToolbox::rebuildHierarchy()
{
    if (!m_hierarchy || !m_session) {
        return;
    }
    const LayoutDocument& d = m_session->document();
    const EditorSelection sel = m_session->selection();
    QString key = d.id + QLatin1Char('\n') + d.name + QLatin1Char('\n');
    for (const LayoutItem& it : d.items) {
        key += it.id + QLatin1Char('\t') + it.label + QLatin1Char('\n');
        key += it.isUnbounded() ? QLatin1Char('U') : QLatin1Char('C');
        key += QLatin1Char('\n');
    }
    if (key == m_treeKey && m_hierarchy->topLevelItemCount() > 0) {
        const QSignalBlocker block(m_hierarchy);
        QTreeWidgetItemIterator iter(m_hierarchy);
        while (*iter) {
            QTreeWidgetItem* item = *iter;
            if (item->data(0, Qt::UserRole).toString() == QLatin1String("item")) {
                const QString id = item->data(0, Qt::UserRole + 1).toString();
                item->setSelected(sel.target == EditorTarget::Item && sel.itemIds.contains(id));
                if (sel.target == EditorTarget::Item && sel.itemId == id) {
                    m_hierarchy->setCurrentItem(item);
                }
            }
            ++iter;
        }
        return;
    }
    m_treeKey = key;
    const QSignalBlocker block(m_hierarchy);
    m_hierarchy->clear();

    auto add = [&](QTreeWidgetItem* parent, const QString& label, const QString& kind,
                   const QString& id, bool selected) {
        auto* item = parent ? new QTreeWidgetItem(parent, {label})
                            : new QTreeWidgetItem(m_hierarchy, {label});
        item->setData(0, Qt::UserRole, kind);
        item->setData(0, Qt::UserRole + 1, id);
        if (selected) {
            m_hierarchy->setCurrentItem(item);
        }
        return item;
    };

    const bool boardSel = sel.target != EditorTarget::Item;
    const QString boardName = d.name.isEmpty() ? d.id : d.name;
    auto* root = add(nullptr, boardName.isEmpty() ? QStringLiteral("Board") : boardName,
                     QStringLiteral("document"), {}, boardSel);
    root->setExpanded(true);

    QVector<const LayoutItem*> cells;
    QVector<const LayoutItem*> edges;
    for (const LayoutItem& it : d.items) {
        if (it.isUnbounded()) {
            edges.push_back(&it);
        } else {
            cells.push_back(&it);
        }
    }

    auto addItems = [&](QTreeWidgetItem* parent, const QVector<const LayoutItem*>& list) {
        for (const LayoutItem* it : list) {
            QString label = it->label.isEmpty() ? it->id : it->label;
            if (!it->label.isEmpty() && it->label != it->id) {
                label = QStringLiteral("%1  (%2)").arg(it->label, it->id);
            }
            add(parent, label, QStringLiteral("item"), it->id,
                sel.target == EditorTarget::Item && sel.itemIds.contains(it->id));
        }
    };

    if (edges.isEmpty()) {
        addItems(root, cells);
        return;
    }

    auto* cellNode = add(root, QStringLiteral("Cells"), QStringLiteral("document"), {}, false);
    cellNode->setExpanded(true);
    addItems(cellNode, cells);
    auto* freeNode = add(root, QStringLiteral("Free"), QStringLiteral("document"), {}, false);
    freeNode->setExpanded(true);
    addItems(freeNode, edges);
}

} // namespace gazer
