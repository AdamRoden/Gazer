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

void LayoutEditorProperties::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    rebuild();
}

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

    auto* header = new QWidget;
    header->setObjectName(QStringLiteral("selectionHeader"));
    auto* headerLay = new QVBoxLayout(header);
    headerLay->setContentsMargins(10, 8, 10, 8);
    headerLay->setSpacing(1);
    m_headerKind = new QLabel;
    m_headerKind->setObjectName(QStringLiteral("selectionKind"));
    m_headerTitle = new QLabel;
    m_headerTitle->setObjectName(QStringLiteral("selectionTitle"));
    m_headerTitle->setWordWrap(true);
    m_headerId = new QLabel;
    m_headerId->setObjectName(QStringLiteral("selectionId"));
    m_headerId->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLay->addWidget(m_headerKind);
    headerLay->addWidget(m_headerTitle);
    headerLay->addWidget(m_headerId);
    root->addWidget(header);

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
    m_tabs->addTab(makePage(&m_pages[4].form, &m_pages[4].scroll), QStringLiteral("Placement"));
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
        titles = {QStringLiteral("Page"), QStringLiteral("Dwell"), QStringLiteral("Style")};
        break;
    case Kind::Grid:
        titles = {QStringLiteral("Grid"), QStringLiteral("Placement"), QStringLiteral("Style")};
        break;
    case Kind::Cell:
        titles = {QStringLiteral("Cell"), QStringLiteral("Placement"), QStringLiteral("Action"),
                  QStringLiteral("Dwell"), QStringLiteral("Style")};
        break;
    case Kind::Zone:
        titles = {QStringLiteral("Zone"), QStringLiteral("Placement"), QStringLiteral("Action"),
                  QStringLiteral("Dwell"), QStringLiteral("Style")};
        break;
    case Kind::Style:
        titles = {QStringLiteral("Style")};
        break;
    case Kind::Dwell:
        titles = {QStringLiteral("Dwell")};
        break;
    }
    for (int i = 0; i < kTabCount; ++i) {
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
    int scrollY[kTabCount] = {};
    for (int i = 0; i < kTabCount; ++i) {
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
        fillDwell(m_pages[1].form);
        fillStyle(m_pages[2].form);
        break;
    case Kind::Grid:
        fillGrid(m_pages[0].form);
        fillPlacement(m_pages[1].form);
        fillStyle(m_pages[2].form);
        break;
    case Kind::Cell:
        fillCell(m_pages[0].form);
        fillPlacement(m_pages[1].form);
        fillAction(m_pages[2].form);
        fillDwell(m_pages[3].form);
        fillStyle(m_pages[4].form);
        break;
    case Kind::Zone:
        fillZone(m_pages[0].form);
        fillPlacement(m_pages[1].form);
        fillAction(m_pages[2].form);
        fillDwell(m_pages[3].form);
        fillStyle(m_pages[4].form);
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
    for (int i = 0; i < kTabCount; ++i) {
        if (m_pages[i].scroll) {
            m_pages[i].scroll->verticalScrollBar()->setValue(scrollY[i]);
        }
    }
    m_shape = currentShape();
    syncHeader();
    m_loading = false;
}

void LayoutEditorProperties::syncHeader()
{
    if (!m_headerKind || !m_headerTitle || !m_headerId) {
        return;
    }
    const Kind kind = currentKind();
    const EditorSelection sel = m_session.selection();
    const PageDocument& d = m_session.document();
    QString kindText;
    QString title;
    QString id;
    switch (kind) {
    case Kind::Page:
        kindText = QStringLiteral("PAGE");
        title = d.name.isEmpty() ? d.id : d.name;
        id = d.id;
        break;
    case Kind::Grid:
        kindText = QStringLiteral("GRID");
        if (const PageGrid* g = m_session.selectedGrid()) {
            title = g->id.isEmpty() ? (g->nested ? QStringLiteral("Subgrid")
                                                 : QStringLiteral("Grid"))
                                    : g->id;
            id = g->id;
        }
        break;
    case Kind::Cell:
        kindText = QStringLiteral("CELL");
        break;
    case Kind::Zone:
        kindText = QStringLiteral("ZONE");
        break;
    case Kind::Style:
        kindText = QStringLiteral("STYLE");
        title = sel.itemId;
        id = sel.itemId;
        break;
    case Kind::Dwell:
        kindText = QStringLiteral("DWELL");
        title = sel.itemId;
        id = sel.itemId;
        break;
    }
    if (kind == Kind::Cell || kind == Kind::Zone) {
        if (sel.itemIds.size() > 1) {
            kindText = QStringLiteral("ITEMS");
            title = QStringLiteral("%1 selected").arg(sel.itemIds.size());
            id = sel.itemIds.join(QStringLiteral(", "));
        } else if (const PageLeaf* it = m_session.selectedItem()) {
            title = it->label.isEmpty() ? it->id : it->label;
            id = it->id;
        }
    }
    m_headerKind->setText(kindText);
    m_headerTitle->setText(title);
    m_headerId->setText(id);
    m_headerId->setVisible(!id.isEmpty() && id != title);
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
            s.hasAction = true;
            s.actionType = a.type;
            s.actionVerb = a.verb;
            s.actionTargetKind = a.targetKind;
            s.zoomMode = a.zoomMode;
            s.moveMode = a.moveMode;
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

} // namespace gazer
