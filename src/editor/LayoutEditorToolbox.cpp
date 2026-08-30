#include "editor/LayoutEditorToolbox.h"

#include "editor/LayoutEditorMenu.h"
#include "editor/LayoutEditorSession.h"
#include "layout/PageEdit.h"

#include <functional>
#include <optional>

#include <QAction>
#include <QGridLayout>
#include <QSizePolicy>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVariant>
#include <QVBoxLayout>

namespace gazer {

LayoutEditorToolbox::LayoutEditorToolbox(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* addTitle = new QLabel(QStringLiteral("Add"));
    addTitle->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(addTitle);

    auto* palette = new QWidget(this);
    m_paletteLayout = new QVBoxLayout(palette);
    m_paletteLayout->setContentsMargins(0, 0, 0, 0);
    m_paletteLayout->setSpacing(8);
    layout->addWidget(palette);

    m_elementsTitle = new QLabel(QStringLiteral("Elements"));
    m_elementsTitle->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(m_elementsTitle);

    m_hierarchy = new QTreeWidget(this);
    m_hierarchy->setHeaderHidden(true);
    m_hierarchy->setRootIsDecorated(true);
    m_hierarchy->setIndentation(14);
    m_hierarchy->setAnimated(true);
    m_hierarchy->setIconSize(QSize(16, 16));
    m_hierarchy->setUniformRowHeights(true);
    m_hierarchy->header()->setStretchLastSection(true);
    layout->addWidget(m_hierarchy, 1);

    connect(m_hierarchy, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item || !m_session) {
            return;
        }
        const QVariant targetV = item->data(0, Qt::UserRole);
        if (!targetV.isValid()) {
            return;
        }
        m_session->selectByTarget(EditorTarget(targetV.toInt()),
                                  item->data(0, Qt::UserRole + 1).toString());
    });
    m_hierarchy->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_hierarchy, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_session) {
            return;
        }
        QTreeWidgetItem* item = m_hierarchy->itemAt(pos);
        const QVariant targetV = item ? item->data(0, Qt::UserRole) : QVariant();
        if (targetV.isValid()) {
            const auto target = EditorTarget(targetV.toInt());
            const QString id = item->data(0, Qt::UserRole + 1).toString();
            if (target != EditorTarget::Item || !m_session->isItemSelected(id)) {
                m_session->selectByTarget(target, id);
            }
        }
        execEditorItemMenu(this, m_hierarchy->mapToGlobal(pos), *m_session,
                           EditorItemMenuOpts{true, true});
    });
}

void LayoutEditorToolbox::bindSession(LayoutEditorSession& session)
{
    m_session = &session;
    connect(m_session, &LayoutEditorSession::selectionChanged, this,
            &LayoutEditorToolbox::rebuildHierarchy);
    connect(m_session, &LayoutEditorSession::documentChanged, this,
            &LayoutEditorToolbox::rebuildHierarchy);
    connect(m_session, &LayoutEditorSession::placeKindChanged, this,
            &LayoutEditorToolbox::syncPlaceButtons);
    rebuildHierarchy();
    syncPlaceButtons();
}

void LayoutEditorToolbox::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    refreshPaletteIcons();
    m_treeKey.clear();
    if (m_session) {
        rebuildHierarchy();
    }
}

void LayoutEditorToolbox::addPlaceAction(const QString& section, QAction* action, EditorGlyph glyph,
                                         EditorItemKind kind, const QString& label)
{
    PaletteItem item;
    item.section = section;
    item.action = action;
    item.glyph = glyph;
    item.place = kind;
    item.label = label;
    addPaletteItem(std::move(item));
}

void LayoutEditorToolbox::addPaletteAction(const QString& section, QAction* action,
                                           EditorGlyph glyph, const QString& label)
{
    PaletteItem item;
    item.section = section;
    item.action = action;
    item.glyph = glyph;
    item.label = label;
    addPaletteItem(std::move(item));
}

LayoutEditorToolbox::Section& LayoutEditorToolbox::ensureSection(const QString& title)
{
    for (Section& s : m_sections) {
        if (s.title == title) {
            return s;
        }
    }
    Section s;
    s.title = title;
    auto* lab = new QLabel(title);
    lab->setObjectName(QStringLiteral("panelSection"));
    m_paletteLayout->addWidget(lab);
    auto* wrap = new QWidget;
    s.grid = new QGridLayout(wrap);
    s.grid->setContentsMargins(0, 0, 0, 4);
    s.grid->setSpacing(6);
    s.grid->setColumnStretch(0, 1);
    s.grid->setColumnStretch(1, 1);
    m_paletteLayout->addWidget(wrap);
    m_sections.push_back(s);
    return m_sections.back();
}

void LayoutEditorToolbox::addPaletteItem(PaletteItem item)
{
    if (!item.action) {
        return;
    }
    Section& section = ensureSection(item.section);
    auto* btn = new QToolButton;
    btn->setObjectName(QStringLiteral("paletteButton"));
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setIconSize(QSize(20, 20));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setMinimumHeight(54);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setText(item.label);
    btn->setToolTip(item.action->toolTip().isEmpty() ? item.action->text().remove(QLatin1Char('&'))
                                                     : item.action->toolTip());
    btn->setCheckable(item.place.has_value());
    btn->setIcon(editorGlyphIcon(item.glyph, m_theme, 20));
    btn->setEnabled(item.action->isEnabled());
    const int row = section.count / 2;
    const int col = section.count % 2;
    section.grid->addWidget(btn, row, col);
    ++section.count;

    QAction* action = item.action;
    const std::optional<EditorItemKind> place = item.place;
    connect(btn, &QToolButton::clicked, this, [this, action, place]() {
        if (!m_session) {
            action->trigger();
            return;
        }
        if (place && m_session->placeKind() == place) {
            m_session->setPlaceKind(std::nullopt);
            return;
        }
        action->trigger();
    });
    connect(action, &QAction::changed, btn, [btn, action]() {
        btn->setEnabled(action->isEnabled());
    });
    item.button = btn;
    m_palette.push_back(std::move(item));
}

void LayoutEditorToolbox::refreshPaletteIcons()
{
    for (PaletteItem& item : m_palette) {
        if (item.button) {
            item.button->setIcon(editorGlyphIcon(item.glyph, m_theme, 20));
        }
    }
}

void LayoutEditorToolbox::syncPlaceButtons()
{
    const std::optional<EditorItemKind> place = m_session ? m_session->placeKind() : std::nullopt;
    for (const PaletteItem& item : m_palette) {
        if (!item.button) {
            continue;
        }
        const QSignalBlocker block(item.button);
        item.button->setChecked(place && item.place == place);
    }
}

void LayoutEditorToolbox::rebuildHierarchy()
{
    if (!m_hierarchy || !m_session) {
        return;
    }
    const PageDocument& d = m_session->document();
    const EditorSelection sel = m_session->selection();
    QString key = d.id + QLatin1Char('\n') + d.name + QLatin1Char('\n');
    int itemCount = 0;
    PageEdit::forEachGrid(d, [&](const PageGrid& g) {
        key += QLatin1Char('G') + g.id + QLatin1Char('\n');
        itemCount += g.cells.size();
        for (const PageCell& it : g.cells) {
            key += it.id + QLatin1Char('\t') + it.label + QLatin1Char('\t') + it.role
                   + QLatin1Char('\n');
        }
    });
    itemCount += d.zones.size();
    for (const PageZone& it : d.zones) {
        key += it.id + QLatin1Char('\t') + it.label + QLatin1Char('\n');
    }
    QStringList styleKeys = d.styles.keys();
    styleKeys.sort();
    key += styleKeys.join(QLatin1Char(','));
    QStringList dwellKeys = d.dwells.keys();
    dwellKeys.sort();
    key += dwellKeys.join(QLatin1Char(','));
    if (m_elementsTitle) {
        m_elementsTitle->setText(itemCount == 0
                                     ? QStringLiteral("Elements")
                                     : QStringLiteral("Elements · %1").arg(itemCount));
    }
    auto selected = [&](EditorTarget t, const QString& id) { return sel.matches(t, id); };
    if (key == m_treeKey && m_hierarchy->topLevelItemCount() > 0) {
        const QSignalBlocker block(m_hierarchy);
        QTreeWidgetItemIterator iter(m_hierarchy);
        while (*iter) {
            QTreeWidgetItem* item = *iter;
            const QVariant targetV = item->data(0, Qt::UserRole);
            bool on = false;
            if (targetV.isValid()) {
                on = selected(EditorTarget(targetV.toInt()),
                              item->data(0, Qt::UserRole + 1).toString());
            }
            item->setSelected(on);
            if (on) {
                m_hierarchy->setCurrentItem(item);
            }
            ++iter;
        }
        return;
    }
    m_treeKey = key;
    const QSignalBlocker block(m_hierarchy);
    m_hierarchy->clear();

    auto add = [&](QTreeWidgetItem* parent, const QString& label, std::optional<EditorTarget> target,
                   const QString& id, EditorGlyph glyph, const QString& tip) {
        auto* item = parent ? new QTreeWidgetItem(parent, {label})
                            : new QTreeWidgetItem(m_hierarchy, {label});
        if (target) {
            item->setData(0, Qt::UserRole, int(*target));
            item->setData(0, Qt::UserRole + 1, id);
        }
        item->setIcon(0, editorGlyphIcon(glyph, m_theme, 16));
        if (!tip.isEmpty()) {
            item->setToolTip(0, tip);
        }
        if (target && selected(*target, id)) {
            m_hierarchy->setCurrentItem(item);
        }
        return item;
    };

    const QString boardName = d.name.isEmpty() ? d.id : d.name;
    auto* root = add(nullptr, boardName.isEmpty() ? QStringLiteral("Page") : boardName,
                     EditorTarget::Document, {}, EditorGlyph::Page, d.id);
    root->setExpanded(true);

    if (!d.styles.isEmpty()) {
        auto* styles = add(root, QStringLiteral("Styles"), std::nullopt, {}, EditorGlyph::Style, {});
        styles->setExpanded(true);
        for (const QString& id : styleKeys) {
            add(styles, id, EditorTarget::Style, id, EditorGlyph::Style, id);
        }
    }
    if (!d.dwells.isEmpty()) {
        auto* dwells = add(root, QStringLiteral("Dwells"), std::nullopt, {}, EditorGlyph::Dwell, {});
        dwells->setExpanded(true);
        for (const QString& id : dwellKeys) {
            add(dwells, id, EditorTarget::Dwell, id, EditorGlyph::Dwell, id);
        }
    }
    if (!d.zones.isEmpty()) {
        auto* zones = add(root, QStringLiteral("Zones"), std::nullopt, {}, EditorGlyph::Zone, {});
        zones->setExpanded(true);
        for (const PageZone& it : d.zones) {
            const QString label = it.label.isEmpty() ? it.id : it.label;
            add(zones, label, EditorTarget::Item, it.id, EditorGlyph::Zone, it.id);
        }
    }

    auto addLeaf = [&](QTreeWidgetItem* parent, const PageLeaf& it, bool zone) {
        const QString label = it.label.isEmpty() ? it.id : it.label;
        add(parent, label, EditorTarget::Item, it.id, glyphForLeaf(it, zone), it.id);
    };
    std::function<void(QTreeWidgetItem*, const PageGrid&)> addGrid =
        [&](QTreeWidgetItem* parent, const PageGrid& g) {
            const QString kindLabel =
                g.nested ? QStringLiteral("Subgrid") : QStringLiteral("Grid");
            const QString label = g.id.isEmpty() ? kindLabel : g.id;
            auto* node = add(parent, label, EditorTarget::Grid, g.id,
                             g.nested ? EditorGlyph::SubGrid : EditorGlyph::GridAdd, kindLabel);
            node->setExpanded(true);
            for (const PageCell& it : g.cells) {
                addLeaf(node, it, false);
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
