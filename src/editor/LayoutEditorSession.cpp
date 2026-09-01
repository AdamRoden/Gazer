#include "editor/LayoutEditorSession.h"

#include "editor/LayoutEditorKeyboard.h"
#include "layout/PageEdit.h"
#include "layout/PageWriter.h"

#include <QRegularExpression>
#include <QUndoCommand>

namespace gazer {

class LayoutEditorSession::EditCommand final : public QUndoCommand {
public:
    EditCommand(LayoutEditorSession* session, PageDocument before, PageDocument after,
                const QString& text)
        : QUndoCommand(text)
        , m_session(session)
        , m_before(std::move(before))
        , m_after(std::move(after))
    {
    }

    void undo() override { m_session->restoreDocument(m_before); }
    void redo() override
    {
        if (m_virgin) {
            m_virgin = false;
            return;
        }
        m_session->restoreDocument(m_after);
    }

private:
    LayoutEditorSession* m_session = nullptr;
    PageDocument m_before;
    PageDocument m_after;
    bool m_virgin = true;
};

LayoutEditorSession::LayoutEditorSession(QObject* parent)
    : QObject(parent)
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { setDirty(!clean); });
    newDocument();
}

const PageDocument& LayoutEditorSession::document() const
{
    return m_doc;
}

PageDocument& LayoutEditorSession::currentDoc()
{
    return m_doc;
}

PageLeaf* LayoutEditorSession::selectedItem()
{
    if (m_sel.target != EditorTarget::Item) {
        return nullptr;
    }
    return itemById(m_sel.itemId);
}

const PageLeaf* LayoutEditorSession::selectedItem() const
{
    if (m_sel.target != EditorTarget::Item) {
        return nullptr;
    }
    return itemById(m_sel.itemId);
}

PageLeaf* LayoutEditorSession::itemById(const QString& id)
{
    return PageEdit::findLeaf(currentDoc(), id);
}

const PageLeaf* LayoutEditorSession::itemById(const QString& id) const
{
    return PageEdit::findLeaf(document(), id);
}

bool LayoutEditorSession::selectedIsZone() const
{
    return m_sel.target == EditorTarget::Item && PageEdit::isZone(document(), m_sel.itemId);
}

void LayoutEditorSession::newDocument()
{
    replaceDocument(makeBlankDocument(), {}, false);
}

void LayoutEditorSession::newFromTemplate(EditorTemplate tmpl, const QString& id, const QString& name)
{
    replaceDocument(makeTemplateDocument(tmpl, id, name), {}, true);
}

void LayoutEditorSession::closeDocument()
{
    newDocument();
}

void LayoutEditorSession::setSelection(EditorSelection sel)
{
    if (sel.target == EditorTarget::Item) {
        QStringList ids;
        for (const QString& id : sel.itemIds) {
            if (itemById(id)) {
                ids.push_back(id);
            }
        }
        if (!sel.itemId.isEmpty() && itemById(sel.itemId) && !ids.contains(sel.itemId)) {
            ids.prepend(sel.itemId);
        }
        sel.itemIds = ids;
        sel.itemId = ids.isEmpty() ? QString() : ids.first();
        if (sel.itemId.isEmpty()) {
            sel = {EditorTarget::Document, {}, {}};
        }
    } else if (sel.target == EditorTarget::Grid) {
        sel.itemIds.clear();
        if (sel.itemId.isEmpty() || !PageEdit::findGrid(document(), sel.itemId)) {
            const PageGrid* g = PageEdit::primaryGrid(document());
            sel.itemId = g ? g->id : QString();
        }
        if (sel.target == EditorTarget::Grid && sel.itemId.isEmpty()) {
            sel = {EditorTarget::Document, {}, {}};
        }
    } else if (sel.target == EditorTarget::Style) {
        sel.itemIds.clear();
        if (sel.itemId.isEmpty() || !document().styles.contains(sel.itemId)) {
            sel = {EditorTarget::Document, {}, {}};
        }
    } else if (sel.target == EditorTarget::Dwell) {
        sel.itemIds.clear();
        if (sel.itemId.isEmpty() || !document().dwells.contains(sel.itemId)) {
            sel = {EditorTarget::Document, {}, {}};
        }
    } else {
        sel.itemId.clear();
        sel.itemIds.clear();
    }
    if (m_sel.target == sel.target && m_sel.itemId == sel.itemId && m_sel.itemIds == sel.itemIds) {
        return;
    }
    m_sel = std::move(sel);
    emit selectionChanged();
}

bool LayoutEditorSession::isItemSelected(const QString& id) const
{
    return m_sel.target == EditorTarget::Item && m_sel.itemIds.contains(id);
}

void LayoutEditorSession::selectItem(const QString& itemId, bool additive)
{
    if (itemId.isEmpty()) {
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    if (additive && m_sel.target == EditorTarget::Item) {
        QStringList ids = m_sel.itemIds;
        if (ids.contains(itemId)) {
            ids.removeAll(itemId);
        } else {
            ids.push_back(itemId);
        }
        if (ids.isEmpty()) {
            setSelection({EditorTarget::Document, {}, {}});
            return;
        }
        setSelection({EditorTarget::Item, ids.first(), ids});
        return;
    }
    setSelection({EditorTarget::Item, itemId, {itemId}});
}

void LayoutEditorSession::selectItems(const QStringList& ids)
{
    if (ids.isEmpty()) {
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    setSelection({EditorTarget::Item, ids.first(), ids});
}

void LayoutEditorSession::selectTarget(EditorTarget target)
{
    setSelection({target, {}, {}});
}

void LayoutEditorSession::selectGrid(const QString& gridId)
{
    if (gridId.isEmpty() || !PageEdit::findGrid(document(), gridId)) {
        setSelection({EditorTarget::Document, {}, {}});
        return;
    }
    setSelection({EditorTarget::Grid, gridId, {}});
}

void LayoutEditorSession::selectByTarget(EditorTarget target, const QString& id)
{
    switch (target) {
    case EditorTarget::Item:
        selectItem(id);
        return;
    case EditorTarget::Grid:
        selectGrid(id);
        return;
    case EditorTarget::Style:
    case EditorTarget::Dwell:
        setSelection({target, id, {}});
        return;
    case EditorTarget::Document:
        selectTarget(EditorTarget::Document);
        return;
    case EditorTarget::None:
        break;
    }
}

QString LayoutEditorSession::selectedGridId() const
{
    if (const PageGrid* g = selectedGrid()) {
        return g->id;
    }
    return {};
}

const PageGrid* LayoutEditorSession::selectedGrid() const
{
    if (m_sel.target == EditorTarget::Grid && !m_sel.itemId.isEmpty()) {
        if (const PageGrid* g = PageEdit::findGrid(document(), m_sel.itemId)) {
            return g;
        }
    }
    if (m_sel.target == EditorTarget::Item) {
        if (const PageGrid* g = PageEdit::gridOwningCell(document(), m_sel.itemId)) {
            return g;
        }
    }
    return PageEdit::primaryGrid(document());
}

void LayoutEditorSession::setPlaceKind(std::optional<EditorItemKind> kind)
{
    m_placeKind = kind;
    emit placeKindChanged();
    if (kind) {
        emit statusMessage(QStringLiteral("Click a grid cell to place"));
    }
}

void LayoutEditorSession::edit(const QString& label, const std::function<void(PageDocument&)>& fn)
{
    PageDocument before = currentDoc();
    fn(currentDoc());
    PageEdit::ensureGridFits(currentDoc());
    if (PageWriter::toBytes(before) == PageWriter::toBytes(currentDoc())) {
        currentDoc() = std::move(before);
        return;
    }
    m_undo.push(new EditCommand(this, std::move(before), currentDoc(), label));
    emit documentChanged();
}

void LayoutEditorSession::restoreDocument(PageDocument doc)
{
    m_doc = std::move(doc);
    if (m_sel.target == EditorTarget::Item) {
        QStringList ids;
        for (const QString& id : m_sel.itemIds) {
            if (itemById(id)) {
                ids.push_back(id);
            }
        }
        if (ids.isEmpty()) {
            m_sel = {EditorTarget::Document, {}, {}};
            emit selectionChanged();
        } else if (ids != m_sel.itemIds) {
            m_sel = {EditorTarget::Item, ids.first(), ids};
            emit selectionChanged();
        }
    } else if (m_sel.target == EditorTarget::Grid) {
        if (!m_sel.itemId.isEmpty() && !PageEdit::findGrid(document(), m_sel.itemId)) {
            const PageGrid* g = PageEdit::primaryGrid(document());
            m_sel.itemId = g ? g->id : QString();
            if (m_sel.target == EditorTarget::Grid && m_sel.itemId.isEmpty()) {
                m_sel = {EditorTarget::Document, {}, {}};
            }
            emit selectionChanged();
        }
    } else if (m_sel.target == EditorTarget::Style && !document().styles.contains(m_sel.itemId)) {
        m_sel = {EditorTarget::Document, {}, {}};
        emit selectionChanged();
    } else if (m_sel.target == EditorTarget::Dwell && !document().dwells.contains(m_sel.itemId)) {
        m_sel = {EditorTarget::Document, {}, {}};
        emit selectionChanged();
    }
    emit documentChanged();
}

void LayoutEditorSession::notify(const QString& msg)
{
    emit statusMessage(msg);
}

QString LayoutEditorSession::uniqueItemId(const QString& stem) const
{
    QString base = stem;
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]+")), QStringLiteral("_"));
    if (base.isEmpty()) {
        base = QStringLiteral("item");
    }
    const QStringList taken = PageEdit::allIds(document());
    auto used = [&](const QString& id) {
        return taken.contains(id) || document().styles.contains(id) || document().dwells.contains(id);
    };
    if (!used(base)) {
        return base;
    }
    for (int n = 2; n < 10000; ++n) {
        const QString id = QStringLiteral("%1_%2").arg(base).arg(n);
        if (!used(id)) {
            return id;
        }
    }
    return base + QStringLiteral("_x");
}

void LayoutEditorSession::replaceDocument(PageDocument doc, const QString& path, bool dirty)
{
    m_doc = std::move(doc);
    m_filePath = path;
    m_sel = {EditorTarget::Document, {}, {}};
    m_placeKind.reset();
    resetUndo();
    setDirty(dirty);
    emit filePathChanged(m_filePath);
    emit selectionChanged();
    emit documentChanged();
    emit placeKindChanged();
}

void LayoutEditorSession::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }
    m_dirty = dirty;
    emit dirtyChanged(m_dirty);
}

void LayoutEditorSession::resetUndo()
{
    m_undo.clear();
}

} // namespace gazer
