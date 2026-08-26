#include "editor/LayoutEditorProperties.h"

#include "editor/LayoutEditorFields.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStringList>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace gazer {

LayoutEditorProperties::LayoutEditorProperties(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Properties"));
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    m_tabs = new QTabWidget(this);
    auto makePage = [](QFormLayout** outForm, QScrollArea** outScroll) {
        auto* page = new QWidget;
        page->setAutoFillBackground(false);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(8, 10, 8, 8);
        form->setSpacing(8);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::DontWrapRows);
        page->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        *outForm = form;
        auto* scroll = new QScrollArea;
        scroll->setWidget(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        *outScroll = scroll;
        return scroll;
    };
    m_tabs->addTab(makePage(&m_pages[0].form, &m_pages[0].scroll), QStringLiteral("Page"));
    m_tabs->addTab(makePage(&m_pages[1].form, &m_pages[1].scroll), QStringLiteral("Style"));
    m_tabs->addTab(makePage(&m_pages[2].form, &m_pages[2].scroll), QStringLiteral("Dwell"));
    m_tabs->addTab(makePage(&m_pages[3].form, &m_pages[3].scroll), QStringLiteral("Action"));
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->setUsesScrollButtons(false);
    root->addWidget(m_tabs, 1);

    connect(&m_session, &LayoutEditorSession::selectionChanged, this, [this]() {
        m_actionStep = 0;
        rebuild();
    });
    connect(&m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        if (m_applying || m_loading) {
            return;
        }
        if (currentShape() != m_shape) {
            rebuild();
        }
    });
    rebuild();
}

LayoutEditorProperties::Kind LayoutEditorProperties::currentKind() const
{
    switch (m_session.selection().target) {
    case EditorTarget::Grid:
        return Kind::Grid;
    case EditorTarget::Item:
        return m_session.selectedIsZone() ? Kind::Zone : Kind::Cell;
    case EditorTarget::Style:
        return Kind::Style;
    case EditorTarget::Dwell:
        return Kind::Dwell;
    case EditorTarget::Document:
    case EditorTarget::None:
        break;
    }
    return Kind::Page;
}

void LayoutEditorProperties::showPageTab()
{
    m_session.selectTarget(EditorTarget::Document);
    m_tabs->setCurrentIndex(0);
}

void LayoutEditorProperties::showGridTab()
{
    const QString gid = m_session.selectedGridId();
    if (gid.isEmpty()) {
        m_session.selectTarget(EditorTarget::Grid);
    } else {
        m_session.selectGrid(gid);
    }
    m_tabs->setCurrentIndex(0);
}

void LayoutEditorProperties::showDwellTab()
{
    const Kind kind = currentKind();
    if (kind == Kind::Cell || kind == Kind::Zone) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (m_tabs->isTabVisible(i) && m_tabs->tabText(i) == QLatin1String("Action")) {
                m_tabs->setCurrentIndex(i);
                return;
            }
        }
    }
    if (kind == Kind::Dwell) {
        m_tabs->setCurrentIndex(0);
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isTabVisible(i) && m_tabs->tabText(i) == QLatin1String("Dwell")) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
    m_tabs->setCurrentIndex(0);
}

void LayoutEditorProperties::showPlacementTab()
{
    const Kind kind = currentKind();
    if (kind != Kind::Grid && kind != Kind::Cell && kind != Kind::Zone) {
        showGridTab();
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isTabVisible(i) && m_tabs->tabText(i) == QLatin1String("Placement")) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
}

void LayoutEditorProperties::showStyleTab()
{
    selectStyleTab();
}

void LayoutEditorProperties::selectStyleTab()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isTabVisible(i) && m_tabs->tabText(i) == QLatin1String("Style")) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
    const QStringList keys = sortedKeys(m_session.document().styles.keys());
    if (!keys.isEmpty()) {
        m_session.setSelection({EditorTarget::Style, keys.first(), {}});
        return;
    }
    m_session.selectTarget(EditorTarget::Document);
}

void LayoutEditorProperties::clearLayout(QFormLayout* form)
{
    while (form->count() > 0) {
        QLayoutItem* it = form->takeAt(0);
        if (QWidget* w = it->widget()) {
            w->hide();
            w->setParent(nullptr);
            delete w;
        }
        delete it;
    }
}

void LayoutEditorProperties::syncTabs(Kind kind)
{
    QStringList titles;
    switch (kind) {
    case Kind::Page:
        titles = {QStringLiteral("Page")};
        break;
    case Kind::Grid:
        titles = {QStringLiteral("Grid"), QStringLiteral("Style"), QStringLiteral("Placement")};
        break;
    case Kind::Cell:
        titles = {QStringLiteral("Cell"), QStringLiteral("Style"), QStringLiteral("Placement"),
                  QStringLiteral("Action")};
        break;
    case Kind::Zone:
        titles = {QStringLiteral("Zone"), QStringLiteral("Style"), QStringLiteral("Placement"),
                  QStringLiteral("Action")};
        break;
    case Kind::Style:
        titles = {QStringLiteral("Style")};
        break;
    case Kind::Dwell:
        titles = {QStringLiteral("Dwell")};
        break;
    }
    for (int i = 0; i < 4; ++i) {
        if (i < titles.size()) {
            m_tabs->setTabText(i, titles[i]);
            m_tabs->setTabVisible(i, true);
        } else {
            m_tabs->setTabVisible(i, false);
        }
    }
    m_kind = kind;
}

void LayoutEditorProperties::rebuild()
{
    if (m_loading || m_applying) {
        return;
    }
    m_loading = true;
    const Kind kind = currentKind();
    QString keep = m_tabs->tabText(m_tabs->currentIndex());
    if (kind == Kind::Page && (keep == QLatin1String("Style") || keep == QLatin1String("Dwell"))) {
        keep = QStringLiteral("Page");
    }
    if (keep == QLatin1String("Dwell") && (kind == Kind::Cell || kind == Kind::Zone)) {
        keep = QStringLiteral("Action");
    }
    int scrollY[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (m_pages[i].scroll) {
            scrollY[i] = m_pages[i].scroll->verticalScrollBar()->value();
        }
    }
    syncTabs(kind);
    for (auto& page : m_pages) {
        clearLayout(page.form);
    }
    switch (kind) {
    case Kind::Page:
        fillPage(m_pages[0].form);
        break;
    case Kind::Grid:
        fillGrid(m_pages[0].form);
        fillStyle(m_pages[1].form);
        fillPlacement(m_pages[2].form);
        break;
    case Kind::Cell:
        fillCell(m_pages[0].form);
        fillStyle(m_pages[1].form);
        fillPlacement(m_pages[2].form);
        fillAction(m_pages[3].form);
        break;
    case Kind::Zone:
        fillZone(m_pages[0].form);
        fillStyle(m_pages[1].form);
        fillPlacement(m_pages[2].form);
        fillAction(m_pages[3].form);
        break;
    case Kind::Style:
        fillStyle(m_pages[0].form);
        break;
    case Kind::Dwell:
        fillDwell(m_pages[0].form);
        break;
    }
    int idx = 0;
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->isTabVisible(i) && m_tabs->tabText(i) == keep) {
            idx = i;
            break;
        }
    }
    m_tabs->setCurrentIndex(idx);
    for (int i = 0; i < 4; ++i) {
        if (m_pages[i].scroll) {
            m_pages[i].scroll->verticalScrollBar()->setValue(scrollY[i]);
        }
    }
    m_shape = currentShape();
    m_loading = false;
}

LayoutEditorProperties::Shape LayoutEditorProperties::currentShape() const
{
    Shape s;
    const EditorSelection sel = m_session.selection();
    s.kind = currentKind();
    s.itemKey = QString::number(int(sel.target)) + QLatin1Char('|') + sel.itemId + QLatin1Char('|')
                + sel.itemIds.join(QLatin1Char(','));
    const PageDocument& d = m_session.document();
    s.autoClose = d.autoClose;
    s.namedStyles = d.styles.size();
    s.namedDwells = d.dwells.size();
    if (const PageGrid* g = m_session.selectedGrid()) {
        s.gridNested = g->nested;
        s.gridAutoClose = g->autoClose;
    }
    s.dwellTiming = d.dwell.activation.has_value();
    s.actionStep = m_actionStep;
    if (const PageLeaf* it = m_session.selectedItem()) {
        s.role = it->role;
        s.loop = it->actionLoop;
        s.customDwell = it->dwell.hasAny();
        s.itemActions = it->actions.size();
        if (!it->actions.isEmpty()) {
            const PageAction& a = it->actions[qBound(0, m_actionStep, it->actions.size() - 1)];
            s.actionType = int(a.type) + int(a.verb) * 16 + int(a.targetKind) * 64
                           + int(a.zoomMode) * 256;
            s.moveMode = int(a.moveMode);
        }
    }
    return s;
}

void LayoutEditorProperties::rebuildIfNeeded()
{
    if (currentShape() != m_shape) {
        QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
    }
}

void LayoutEditorProperties::applyItem(const std::function<void(PageLeaf&)>& fn,
                                       const QString& undoLabel)
{
    const QStringList ids = m_session.selection().itemIds;
    m_applying = true;
    m_session.edit(undoLabel, [&](PageDocument& d) {
        for (const QString& id : ids) {
            if (PageLeaf* it = PageEdit::findLeaf(d, id)) {
                fn(*it);
            }
        }
    });
    m_applying = false;
    rebuildIfNeeded();
}

void LayoutEditorProperties::applyDoc(const std::function<void(PageDocument&)>& fn,
                                      const QString& undoLabel)
{
    m_applying = true;
    m_session.edit(undoLabel, fn);
    m_applying = false;
    rebuildIfNeeded();
}

void LayoutEditorProperties::applyGrid(const std::function<void(PageGrid&)>& fn,
                                       const QString& undoLabel)
{
    const QString id = m_session.selectedGridId();
    applyDoc(
        [&](PageDocument& doc) {
            PageGrid* g = id.isEmpty() ? PageEdit::primaryGrid(doc) : PageEdit::findGrid(doc, id);
            if (g) {
                fn(*g);
            }
        },
        undoLabel);
}

void LayoutEditorProperties::applyZone(const std::function<void(PageZone&)>& fn,
                                       const QString& undoLabel)
{
    const QString id = m_session.selection().itemId;
    applyDoc(
        [&](PageDocument& doc) {
            if (PageZone* z = PageEdit::findZone(doc, id)) {
                fn(*z);
            }
        },
        undoLabel);
}

void LayoutEditorProperties::applyCell(const std::function<void(PageCell&)>& fn,
                                       const QString& undoLabel)
{
    const QString id = m_session.selection().itemId;
    applyDoc(
        [&](PageDocument& doc) {
            if (PageCell* c = PageEdit::findCell(doc, id)) {
                fn(*c);
            }
        },
        undoLabel);
}

void LayoutEditorProperties::selectNamedStyle(const QString& id)
{
    if (id.isEmpty()) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    m_session.setSelection({EditorTarget::Style, id, {}});
}

void LayoutEditorProperties::selectNamedDwell(const QString& id)
{
    if (id.isEmpty()) {
        m_session.selectTarget(EditorTarget::Document);
        return;
    }
    m_session.setSelection({EditorTarget::Dwell, id, {}});
}

void LayoutEditorProperties::renameNamedStyle(const QString& from, const QString& to)
{
    if (to.isEmpty() || to == from || m_session.document().styles.contains(to)
        || PageEdit::allIds(m_session.document()).contains(to)) {
        rebuild();
        return;
    }
    applyDoc(
        [&](PageDocument& doc) {
            if (!doc.styles.contains(from) || doc.styles.contains(to)) {
                return;
            }
            const PageChrome st = doc.styles.take(from);
            doc.styles.insert(to, st);
            PageEdit::remapStyleId(doc, from, to);
        },
        QStringLiteral("Rename style"));
    m_session.setSelection({EditorTarget::Style, to, {}});
}

void LayoutEditorProperties::renameNamedDwell(const QString& from, const QString& to)
{
    if (to.isEmpty() || to == from || m_session.document().dwells.contains(to)
        || PageEdit::allIds(m_session.document()).contains(to)) {
        rebuild();
        return;
    }
    applyDoc(
        [&](PageDocument& doc) {
            if (!doc.dwells.contains(from) || doc.dwells.contains(to)) {
                return;
            }
            const PageDwell dw = doc.dwells.take(from);
            doc.dwells.insert(to, dw);
            PageEdit::remapDwellId(doc, from, to);
        },
        QStringLiteral("Rename dwell"));
    m_session.setSelection({EditorTarget::Dwell, to, {}});
}

void LayoutEditorProperties::fillPage(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageDocument& d = m_session.document();
    b.text(form, QStringLiteral("Id"), d.id, [this](const QString& t) {
        applyDoc([&](PageDocument& doc) { doc.id = t.trimmed(); }, QStringLiteral("Page id"));
    });
    b.text(form, QStringLiteral("Name"), d.name, [this](const QString& t) {
        applyDoc([&](PageDocument& doc) { doc.name = t; }, QStringLiteral("Name"));
    });
    b.check(form, QStringLiteral("Process-lifetime root"), d.master, [this](bool on) {
        applyDoc([&](PageDocument& doc) { doc.master = on; }, QStringLiteral("Master"));
    });
    b.check(form, QStringLiteral("Auto-close when idle"), d.autoClose, [this](bool on) {
        applyDoc([&](PageDocument& doc) { doc.autoClose = on; }, QStringLiteral("Auto close"));
    });
    if (d.autoClose) {
        b.integer(form, QStringLiteral("Idle ms"), d.autoCloseIdleMs, -1, 120000, [this](int v) {
            applyDoc([&](PageDocument& doc) { doc.autoCloseIdleMs = v; }, QStringLiteral("Idle ms"));
        });
    }
    addChromeFields(b, form, d.style, [this](const QString& undo, const auto& mut) {
        applyDoc([&](PageDocument& doc) { mut(doc.style); }, undo);
    });
    addDwellFields(b, form, d.dwell, [this](const QString& undo, const auto& mut) {
        applyDoc([&](PageDocument& doc) { mut(doc.dwell); }, undo);
    });
}

void LayoutEditorProperties::fillGrid(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageGrid* g = m_session.selectedGrid();
    if (!g) {
        b.note(form, QStringLiteral("This page has no grid. Add a grid from the Layout menu."));
        return;
    }
    b.text(form, QStringLiteral("Id"), g->id, [this](const QString& t) {
        const QString next = t.trimmed();
        const QString cur = m_session.selectedGridId();
        if (next == cur) {
            return;
        }
        if (next.isEmpty() || PageEdit::allIds(m_session.document()).contains(next)) {
            rebuild();
            return;
        }
        applyGrid([&](PageGrid& grid) { grid.id = next; }, QStringLiteral("Grid id"));
        m_session.selectGrid(next);
    });
    QString chrome = QStringLiteral("none");
    if (g->rootSlot == PageRootSlot::Drawer) {
        chrome = QStringLiteral("drawer");
    } else if (g->rootSlot == PageRootSlot::Quit) {
        chrome = QStringLiteral("quit");
    }
    b.comboValues(form, QStringLiteral("Chrome slot"),
                  {QStringLiteral("none"), QStringLiteral("drawer"), QStringLiteral("quit")},
                  {QStringLiteral("none"), QStringLiteral("drawer"), QStringLiteral("quit")}, chrome,
                  [this](const QString& t) {
                      applyGrid(
                          [&](PageGrid& grid) {
                              if (t == QLatin1String("drawer")) {
                                  grid.rootSlot = PageRootSlot::Drawer;
                              } else if (t == QLatin1String("quit")) {
                                  grid.rootSlot = PageRootSlot::Quit;
                              } else {
                                  grid.rootSlot = PageRootSlot::None;
                              }
                          },
                          QStringLiteral("Chrome slot"));
                  });
    b.check(form, QStringLiteral("Shell (always on top)"), g->shell, [this](bool on) {
        applyGrid([&](PageGrid& grid) { grid.shell = on; }, QStringLiteral("Shell"));
    });
    if (g->rootSlot == PageRootSlot::None) {
        b.check(form, QStringLiteral("Show"), g->show, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.show = on; }, QStringLiteral("Grid show"));
        });
    }
    b.check(form, QStringLiteral("Auto-close when idle"), g->autoClose, [this](bool on) {
        applyGrid([&](PageGrid& grid) { grid.autoClose = on; }, QStringLiteral("Grid auto close"));
    });
    if (g->autoClose) {
        b.integer(form, QStringLiteral("Idle ms"), g->autoCloseIdleMs, -1, 120000, [this](int v) {
            applyGrid([&](PageGrid& grid) { grid.autoCloseIdleMs = v; },
                      QStringLiteral("Grid idle ms"));
        });
    }
    b.heading(form, QStringLiteral("Cells"));
    b.integer(form, QStringLiteral("Columns"), g->columns, 1, 48, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.columns = v; }, QStringLiteral("Columns"));
    });
    b.integer(form, QStringLiteral("Rows"), g->rows, 1, 48, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.rows = v; }, QStringLiteral("Rows"));
    });
    QString weights;
    for (double w : g->rowWeights) {
        if (!weights.isEmpty()) {
            weights += QLatin1Char(',');
        }
        weights += QString::number(w, 'g', 8);
    }
    b.text(form, QStringLiteral("Row weights"), weights, [this](const QString& t) {
        applyGrid([&](PageGrid& grid) { grid.rowWeights = parseRowWeights(t); },
                  QStringLiteral("Row weights"));
    });
    b.integer(form, QStringLiteral("Gap px"), g->gapPx, 0, 64, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.gapPx = v; }, QStringLiteral("Gap"));
    });
    b.integer(form, QStringLiteral("Inset px"), g->marginPx, 0, 200, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.marginPx = v; }, QStringLiteral("Grid margin"));
    });
}

void LayoutEditorProperties::fillLeafIdentity(QFormLayout* form, const PageLeaf& item)
{
    PropertyBinder b{this, &m_loading};
    b.text(form, QStringLiteral("Id"), item.id, [this](const QString& t) {
        const QString next = t.trimmed();
        const QString cur = m_session.selection().itemId;
        if (next == cur) {
            return;
        }
        if (next.isEmpty() || PageEdit::allIds(m_session.document()).contains(next)) {
            rebuild();
            return;
        }
        applyDoc(
            [&](PageDocument& doc) {
                if (PageLeaf* leaf = PageEdit::findLeaf(doc, cur)) {
                    leaf->id = next;
                }
            },
            QStringLiteral("Id"));
        m_session.selectItem(next);
    });
    b.text(form, QStringLiteral("Label"), item.label, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.label = t; }, QStringLiteral("Label"));
    });
    b.text(form, QStringLiteral("Caption"), item.caption, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.caption = t; }, QStringLiteral("Caption"));
    });
    b.combo(form, QStringLiteral("Icon"), iconChoices(), item.icon, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.icon = t; }, QStringLiteral("Icon"));
    });
    b.combo(form, QStringLiteral("Role"),
            {QString(), QStringLiteral("label"), QStringLiteral("tab"), QStringLiteral("toggle"),
             QStringLiteral("slider"), QStringLiteral("preview")},
            item.role, [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.role = t; }, QStringLiteral("Role"));
            });
    b.combo(form, QStringLiteral("Text style"),
            {QString(), QStringLiteral("caption"), QStringLiteral("body"), QStringLiteral("title"),
             QStringLiteral("section")},
            item.textStyle, [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.textStyle = t; }, QStringLiteral("Text style"));
            });
    b.text(form, QStringLiteral("Setting key"), item.settingKey, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.settingKey = t; }, QStringLiteral("Setting key"));
    });
    b.text(form, QStringLiteral("Cluster"), item.cluster, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.cluster = t; }, QStringLiteral("Cluster"));
    });
    QStringList slotNames = {QString(), QStringLiteral("dec"), QStringLiteral("value"),
                             QStringLiteral("inc"), QStringLiteral("edit")};
    if (!item.clusterSlot.isEmpty() && !slotNames.contains(item.clusterSlot)) {
        slotNames.push_back(item.clusterSlot);
    }
    b.combo(form, QStringLiteral("Cluster slot"), slotNames, item.clusterSlot,
            [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.clusterSlot = t; }, QStringLiteral("Cluster slot"));
            });
    b.check(form, QStringLiteral("Show"), item.show, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.show = on; }, QStringLiteral("Show"));
    });
    fillVisibleWhen(form, item);
    b.check(form, QStringLiteral("Shell (always on top)"), item.shell, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.shell = on; }, QStringLiteral("Shell"));
    });
}

void LayoutEditorProperties::fillVisibleWhen(QFormLayout* form, const PageLeaf& item)
{
    PropertyBinder b{this, &m_loading};
    QStringList whenVals = visibleWhenChoices();
    QStringList whenLabels = {QStringLiteral("(always)"), QStringLiteral("expanded"),
                              QStringLiteral("!expanded"), QStringLiteral("quitConfirm"),
                              QStringLiteral("!quitConfirm"), QStringLiteral("dwellSuspend"),
                              QStringLiteral("!dwellSuspend")};
    if (!item.visibleWhen.isEmpty() && !whenVals.contains(item.visibleWhen)) {
        whenVals.prepend(item.visibleWhen);
        whenLabels.prepend(item.visibleWhen);
    }
    b.comboValues(form, QStringLiteral("Visible when"), whenLabels, whenVals, item.visibleWhen,
                  [this](const QString& t) {
                      applyItem([&](PageLeaf& it) { it.visibleWhen = t; },
                                QStringLiteral("Visible when"));
                  });
}

void LayoutEditorProperties::fillCell(QFormLayout* form)
{
    const PageLeaf* item = m_session.selectedItem();
    if (!item || !PageEdit::findCell(m_session.document(), item->id)) {
        return;
    }
    fillLeafIdentity(form, *item);
}

void LayoutEditorProperties::fillZone(QFormLayout* form)
{
    const PageLeaf* item = m_session.selectedItem();
    if (!item || !PageEdit::findZone(m_session.document(), item->id)) {
        return;
    }
    fillLeafIdentity(form, *item);
}

void LayoutEditorProperties::fillPlacement(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Grid) {
        const PageGrid* g = m_session.selectedGrid();
        if (!g) {
            return;
        }
        if (g->nested) {
            b.integer(form, QStringLiteral("Row"), g->row, 0, 64, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.row = v; }, QStringLiteral("Row"));
            });
            b.integer(form, QStringLiteral("Column"), g->col, 0, 64, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.col = v; }, QStringLiteral("Column"));
            });
            b.integer(form, QStringLiteral("Row span"), g->rowSpan, 1, 16, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.rowSpan = v; }, QStringLiteral("Row span"));
            });
            b.integer(form, QStringLiteral("Column span"), g->colSpan, 1, 16, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.colSpan = v; }, QStringLiteral("Column span"));
            });
            return;
        }
        b.check(form, QStringLiteral("Desktop bounds"), g->desktopMode, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.desktopMode = on; }, QStringLiteral("Desktop mode"));
        });
        b.combo(form, QStringLiteral("Anchor"), pageAnchorNames(),
                PageDimParse::anchorName(g->anchor), [this](const QString& t) {
                    applyGrid(
                        [&](PageGrid& grid) {
                            bool ok = true;
                            grid.anchor = PageDimParse::parseAnchor(t, &ok);
                        },
                        QStringLiteral("Anchor"));
                });
        b.dim(form, QStringLiteral("Offset X"), g->offset.x, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.offset.x = v; }, QStringLiteral("Offset X"));
        });
        b.dim(form, QStringLiteral("Offset Y"), g->offset.y, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.offset.y = v; }, QStringLiteral("Offset Y"));
        });
        b.dim(form, QStringLiteral("Width"), g->size.x, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.size.x = v; }, QStringLiteral("Width"));
        });
        b.dim(form, QStringLiteral("Height"), g->size.y, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.size.y = v; }, QStringLiteral("Height"));
        });
        b.check(form, QStringLiteral("Drawer motion"), g->drawerMotion, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.drawerMotion = on; },
                      QStringLiteral("Drawer motion"));
        });
        return;
    }
    if (kind == Kind::Cell) {
        const PageLeaf* item = m_session.selectedItem();
        const PageCell* c = item ? PageEdit::findCell(m_session.document(), item->id) : nullptr;
        if (!c) {
            return;
        }
        b.integer(form, QStringLiteral("Row"), c->row, 0, 64, [this](int v) {
            applyCell([&](PageCell& cell) { cell.row = v; }, QStringLiteral("Row"));
        });
        b.integer(form, QStringLiteral("Column"), c->col, 0, 64, [this](int v) {
            applyCell([&](PageCell& cell) { cell.col = v; }, QStringLiteral("Column"));
        });
        b.integer(form, QStringLiteral("Row span"), c->rowSpan, 1, 16, [this](int v) {
            applyCell([&](PageCell& cell) { cell.rowSpan = v; }, QStringLiteral("Row span"));
        });
        b.integer(form, QStringLiteral("Column span"), c->colSpan, 1, 16, [this](int v) {
            applyCell([&](PageCell& cell) { cell.colSpan = v; }, QStringLiteral("Column span"));
        });
        return;
    }
    const PageLeaf* item = m_session.selectedItem();
    const PageZone* z = item ? PageEdit::findZone(m_session.document(), item->id) : nullptr;
    if (!z) {
        return;
    }
    b.heading(form, QStringLiteral("Progress zone (offset from page anchor)"));
    b.check(form, QStringLiteral("Desktop bounds"), z->desktopMode, [this](bool on) {
        applyZone([&](PageZone& zone) { zone.desktopMode = on; }, QStringLiteral("Desktop mode"));
    });
    b.combo(form, QStringLiteral("Anchor"), pageAnchorNames(), PageDimParse::anchorName(z->anchor),
            [this](const QString& t) {
                applyZone(
                    [&](PageZone& zone) {
                        bool ok = true;
                        zone.anchor = PageDimParse::parseAnchor(t, &ok);
                    },
                    QStringLiteral("Anchor"));
            });
    b.dim(form, QStringLiteral("Offset X"), z->offset.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.offset.x = v; }, QStringLiteral("Offset X"));
    });
    b.dim(form, QStringLiteral("Offset Y"), z->offset.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.offset.y = v; }, QStringLiteral("Offset Y"));
    });
    b.dim(form, QStringLiteral("Width"), z->size.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.size.x = v; }, QStringLiteral("Width"));
    });
    b.dim(form, QStringLiteral("Height"), z->size.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.size.y = v; }, QStringLiteral("Height"));
    });
    b.heading(form, QStringLiteral("Dwell zone (offset from progress anchor)"));
    b.dim(form, QStringLiteral("Offset X"), z->dwellOffset.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellOffset.x = v; }, QStringLiteral("Dwell offset X"));
    });
    b.dim(form, QStringLiteral("Offset Y"), z->dwellOffset.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellOffset.y = v; }, QStringLiteral("Dwell offset Y"));
    });
    b.dim(form, QStringLiteral("Width"), z->dwellSize.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellSize.x = v; }, QStringLiteral("Dwell width"));
    });
    b.dim(form, QStringLiteral("Height"), z->dwellSize.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellSize.y = v; }, QStringLiteral("Dwell height"));
    });
}

void LayoutEditorProperties::fillStyle(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Style) {
        const QString styleId = m_session.selection().itemId;
        addNamedChromeEditor(
            b, form, m_session.document().styles, styleId,
            [this](const QString& id) { selectNamedStyle(id); },
            [this]() { m_session.addNamedStyle(); },
            [this](const QString& from, const QString& to) { renameNamedStyle(from, to); },
            [this](const QString& id) {
                applyDoc([&](PageDocument& doc) { doc.styles.remove(id); },
                         QStringLiteral("Delete style"));
                m_session.selectTarget(EditorTarget::Document);
            },
            [this, styleId](const QString& undo, const auto& mut) {
                applyDoc(
                    [&](PageDocument& doc) {
                        const auto it = doc.styles.find(styleId);
                        if (it != doc.styles.end()) {
                            mut(*it);
                        }
                    },
                    undo);
            });
        return;
    }
    if (kind == Kind::Grid) {
        const PageGrid* g = m_session.selectedGrid();
        if (!g) {
            return;
        }
        addOptionalIdCombo(b, form, QStringLiteral("Inherit"), m_session.document().styles.keys(),
                           g->styleId, [this](const QString& t) {
                               applyGrid([&](PageGrid& grid) { grid.styleId = t; },
                                         QStringLiteral("Grid style"));
                           });
        addChromeFields(b, form, g->style, [this](const QString& undo, const auto& mut) {
            applyGrid([&](PageGrid& grid) { mut(grid.style); }, undo);
        }, false);
        return;
    }
    const PageLeaf* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    addOptionalIdCombo(b, form, QStringLiteral("Inherit"), m_session.document().styles.keys(),
                       item->styleId, [this](const QString& t) {
                           applyItem([&](PageLeaf& it) { it.styleId = t; },
                                     QStringLiteral("Style"));
                       });
    addChromeFields(b, form, item->style, [this](const QString& undo, const auto& mut) {
        m_session.applyChromeToSelected(mut, undo);
        rebuildIfNeeded();
    }, false);
}

void LayoutEditorProperties::fillDwell(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Dwell) {
        const QString dwellId = m_session.selection().itemId;
        addNamedDwellEditor(
            b, form, m_session.document().dwells, dwellId,
            [this](const QString& id) { selectNamedDwell(id); },
            [this]() { m_session.addNamedDwell(); },
            [this](const QString& from, const QString& to) { renameNamedDwell(from, to); },
            [this](const QString& id) {
                applyDoc([&](PageDocument& doc) { doc.dwells.remove(id); },
                         QStringLiteral("Delete dwell"));
                m_session.selectTarget(EditorTarget::Document);
            },
            [this, dwellId](const QString& undo, const auto& mut) {
                applyDoc(
                    [&](PageDocument& doc) {
                        const auto it = doc.dwells.find(dwellId);
                        if (it != doc.dwells.end()) {
                            mut(*it);
                        }
                    },
                    undo);
            });
        return;
    }
}

void LayoutEditorProperties::fillAction(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageLeaf* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    b.check(form, QStringLiteral("Interactive"), item->interactive, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.interactive = on; }, QStringLiteral("Interactive"));
    });
    b.check(form, QStringLiteral("Still works while Sleep is on"), item->suspendExempt,
            [this](bool on) {
                applyItem([&](PageLeaf& it) { it.suspendExempt = on; },
                          QStringLiteral("Suspend exempt"));
            });
    b.check(form, QStringLiteral("Loop until activated again"), item->actionLoop, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.actionLoop = on; }, QStringLiteral("Action loop"));
    });
    b.text(form, QStringLiteral("Active-state key"), item->activeState, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.activeState = t; }, QStringLiteral("Active state"));
    });
    addActionSeriesFields(b, form, item->actions, item->label, m_catalog, m_actionStep,
                          [this](int step) {
                              if (m_actionStep == step) {
                                  return;
                              }
                              m_actionStep = step;
                              QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
                          },
                          [this, id = item->id](QVector<PageAction> next) {
                              m_session.setActions(id, std::move(next));
                              rebuildIfNeeded();
                          });
    addDwellFields(
        b, form, item->dwell,
        [this](const QString& undo, const auto& mut) {
            applyItem([&](PageLeaf& it) { mut(it.dwell); }, undo);
        },
        true, m_session.document().dwells.keys(), item->dwellId, [this](const QString& t) {
            applyItem([&](PageLeaf& it) { it.dwellId = t; }, QStringLiteral("Dwell"));
        });
}

} // namespace gazer
