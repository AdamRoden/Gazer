#include "editor/LayoutEditorToolbox.h"

#include "editor/LayoutEditorSession.h"
#include "layout/PageEdit.h"

#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <functional>
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
        } else if (kind == QLatin1String("grid")) {
            m_session->selectGrid(id);
        } else if (kind == QLatin1String("style")) {
            m_session->setSelection({EditorTarget::Style, id, {}});
        } else if (kind == QLatin1String("dwell")) {
            m_session->setSelection({EditorTarget::Dwell, id, {}});
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
        const QString kind = item ? item->data(0, Qt::UserRole).toString() : QString();
        const QString id = item ? item->data(0, Qt::UserRole + 1).toString() : QString();
        if (kind == QLatin1String("item")) {
            if (!m_session->isItemSelected(id)) {
                m_session->selectItem(id);
            }
        } else if (kind == QLatin1String("grid")) {
            m_session->selectGrid(id);
        } else if (kind == QLatin1String("style")) {
            m_session->setSelection({EditorTarget::Style, id, {}});
        } else if (kind == QLatin1String("dwell")) {
            m_session->setSelection({EditorTarget::Dwell, id, {}});
        }
        QMenu menu(this);
        auto* dup = menu.addAction(QStringLiteral("Duplicate"));
        auto* del = menu.addAction(QStringLiteral("Delete"));
        menu.addSeparator();
        auto* addSub = menu.addAction(QStringLiteral("Add subgrid"));
        auto* toFree = menu.addAction(QStringLiteral("Convert to zone"));
        auto* toCell = menu.addAction(QStringLiteral("Convert to cell"));
        menu.addSeparator();
        auto* raise = menu.addAction(QStringLiteral("Bring forward"));
        auto* lower = menu.addAction(QStringLiteral("Send backward"));
        const EditorTarget target = m_session->selection().target;
        const bool hasItem = target == EditorTarget::Item;
        const bool hasGrid = target == EditorTarget::Grid;
        const bool hasNamed = target == EditorTarget::Style || target == EditorTarget::Dwell;
        dup->setEnabled(hasItem);
        del->setEnabled(hasItem || hasGrid || hasNamed);
        addSub->setEnabled(hasGrid || hasItem);
        toFree->setEnabled(hasItem);
        toCell->setEnabled(hasItem);
        raise->setEnabled(hasItem);
        lower->setEnabled(hasItem);
        QAction* chosen = menu.exec(m_hierarchy->mapToGlobal(pos));
        if (chosen == dup) {
            m_session->duplicateSelected();
        } else if (chosen == del) {
            m_session->deleteSelected();
        } else if (chosen == addSub) {
            m_session->addSubGrid();
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
    const PageDocument& d = m_session->document();
    const EditorSelection sel = m_session->selection();
    QString key = d.id + QLatin1Char('\n') + d.name + QLatin1Char('\n');
    PageEdit::forEachGrid(d, [&](const PageGrid& g) {
        key += QLatin1Char('G') + g.id + QLatin1Char('\n');
        for (const PageCell& it : g.cells) {
            key += it.id + QLatin1Char('\t') + it.label + QLatin1Char('\n');
        }
    });
    for (const PageZone& it : d.zones) {
        key += it.id + QLatin1Char('\t') + it.label + QLatin1Char('\n');
    }
    QStringList styleKeys = d.styles.keys();
    styleKeys.sort();
    key += styleKeys.join(QLatin1Char(','));
    QStringList dwellKeys = d.dwells.keys();
    dwellKeys.sort();
    key += dwellKeys.join(QLatin1Char(','));
    if (key == m_treeKey && m_hierarchy->topLevelItemCount() > 0) {
        const QSignalBlocker block(m_hierarchy);
        QTreeWidgetItemIterator iter(m_hierarchy);
        while (*iter) {
            QTreeWidgetItem* item = *iter;
            const QString kind = item->data(0, Qt::UserRole).toString();
            const QString id = item->data(0, Qt::UserRole + 1).toString();
            bool selected = false;
            if (kind == QLatin1String("item")) {
                selected = sel.target == EditorTarget::Item && sel.itemIds.contains(id);
            } else if (kind == QLatin1String("grid")) {
                selected = sel.target == EditorTarget::Grid && sel.itemId == id;
            } else if (kind == QLatin1String("style")) {
                selected = sel.target == EditorTarget::Style && sel.itemId == id;
            } else if (kind == QLatin1String("dwell")) {
                selected = sel.target == EditorTarget::Dwell && sel.itemId == id;
            } else if (kind == QLatin1String("document")) {
                selected = sel.target == EditorTarget::Document && id.isEmpty();
            }
            item->setSelected(selected);
            if (selected) {
                m_hierarchy->setCurrentItem(item);
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

    const bool boardSel = sel.target == EditorTarget::Document;
    const QString boardName = d.name.isEmpty() ? d.id : d.name;
    auto* root = add(nullptr, boardName.isEmpty() ? QStringLiteral("Page") : boardName,
                     QStringLiteral("document"), {}, boardSel);
    root->setExpanded(true);

    if (!d.styles.isEmpty()) {
        auto* styles = add(root, QStringLiteral("Styles"), QStringLiteral("document"), {}, false);
        styles->setExpanded(true);
        for (const QString& id : styleKeys) {
            add(styles, id, QStringLiteral("style"), id,
                sel.target == EditorTarget::Style && sel.itemId == id);
        }
    }
    if (!d.dwells.isEmpty()) {
        auto* dwells = add(root, QStringLiteral("Dwells"), QStringLiteral("document"), {}, false);
        dwells->setExpanded(true);
        for (const QString& id : dwellKeys) {
            add(dwells, id, QStringLiteral("dwell"), id,
                sel.target == EditorTarget::Dwell && sel.itemId == id);
        }
    }
    if (!d.zones.isEmpty()) {
        auto* zones = add(root, QStringLiteral("Zones"), QStringLiteral("document"), {}, false);
        zones->setExpanded(true);
        for (const PageZone& it : d.zones) {
            QString label = it.label.isEmpty() ? it.id : it.label;
            if (!it.label.isEmpty() && it.label != it.id) {
                label = QStringLiteral("%1  (%2)").arg(it.label, it.id);
            }
            add(zones, label, QStringLiteral("item"), it.id,
                sel.target == EditorTarget::Item && sel.itemIds.contains(it.id));
        }
    }

    auto addLeaf = [&](QTreeWidgetItem* parent, const PageLeaf& it) {
        QString label = it.label.isEmpty() ? it.id : it.label;
        if (!it.label.isEmpty() && it.label != it.id) {
            label = QStringLiteral("%1  (%2)").arg(it.label, it.id);
        }
        add(parent, label, QStringLiteral("item"), it.id,
            sel.target == EditorTarget::Item && sel.itemIds.contains(it.id));
    };
    std::function<void(QTreeWidgetItem*, const PageGrid&)> addGrid =
        [&](QTreeWidgetItem* parent, const PageGrid& g) {
            const QString kindLabel =
                g.nested ? QStringLiteral("SubGrid") : QStringLiteral("Grid");
            const QString label = g.id.isEmpty() ? kindLabel
                                                 : QStringLiteral("%1  (%2)").arg(kindLabel, g.id);
            auto* node = add(parent, label, QStringLiteral("grid"), g.id,
                             sel.target == EditorTarget::Grid && sel.itemId == g.id);
            node->setExpanded(true);
            for (const PageCell& it : g.cells) {
                addLeaf(node, it);
            }
            for (const PageGrid& sub : g.subGrids) {
                addGrid(node, sub);
            }
        };
    for (const PageGrid& g : d.grids) {
        addGrid(root, g);
    }
}

} // namespace gazer
