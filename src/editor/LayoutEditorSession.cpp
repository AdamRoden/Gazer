#include "editor/LayoutEditorSession.h"

#include "layout/LayoutLoader.h"
#include "layout/LayoutWriter.h"
#include "utils/Log.h"

#include <QRegularExpression>
#include <QUndoCommand>

#include <algorithm>

namespace gazer {

class LayoutEditorSession::SnapshotCommand final : public QUndoCommand {
public:
    SnapshotCommand(LayoutEditorSession* session, LayoutDocument before, LayoutDocument after,
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
    LayoutDocument m_before;
    LayoutDocument m_after;
    bool m_virgin = true;
};

namespace {

LayoutDocument makeBlankDocument()
{
    LayoutDocument d;
    d.schemaVersion = 1;
    d.id = QStringLiteral("untitled");
    d.name = QStringLiteral("Untitled");
    d.autoClose = true;
    d.grid.columns = 4;
    d.grid.rows = 2;
    d.grid.gapPx = 8;
    d.grid.marginPx = 8;
    d.placement.specified = true;
    d.placement.anchor = LayoutWindowPlacement::Anchor::BottomCenter;
    d.placement.width = DimSpec::pixels(800);
    d.placement.height = DimSpec::pixels(280);
    d.placement.marginPx = 8;
    return d;
}

QString roleForKind(EditorItemKind kind)
{
    switch (kind) {
    case EditorItemKind::Label:
        return QStringLiteral("label");
    case EditorItemKind::Toggle:
        return QStringLiteral("toggle");
    case EditorItemKind::Tab:
        return QStringLiteral("tab");
    case EditorItemKind::Slider:
        return QStringLiteral("slider");
    case EditorItemKind::Button:
    case EditorItemKind::Unbounded:
        break;
    }
    return {};
}

} // namespace

LayoutEditorSession::LayoutEditorSession(QObject* parent)
    : QObject(parent)
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { setDirty(!clean); });
    newDocument();
}

LayoutItem* LayoutEditorSession::selectedItem()
{
    if (m_sel.target != EditorTarget::Item) {
        return nullptr;
    }
    return itemById(m_sel.itemId);
}

const LayoutItem* LayoutEditorSession::selectedItem() const
{
    if (m_sel.target != EditorTarget::Item) {
        return nullptr;
    }
    return itemById(m_sel.itemId);
}

LayoutItem* LayoutEditorSession::itemById(const QString& id)
{
    for (LayoutItem& it : m_doc.items) {
        if (it.id == id) {
            return &it;
        }
    }
    return nullptr;
}

const LayoutItem* LayoutEditorSession::itemById(const QString& id) const
{
    return m_doc.findItem(id);
}

void LayoutEditorSession::newDocument()
{
    m_doc = makeBlankDocument();
    m_filePath.clear();
    m_sel = {EditorTarget::Document, {}};
    m_clipboard.reset();
    resetUndo();
    emit filePathChanged(m_filePath);
    emit selectionChanged();
    emit documentChanged();
}

bool LayoutEditorSession::loadFromFile(const QString& path, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    m_doc = std::move(doc);
    m_filePath = path;
    m_sel = {EditorTarget::Document, {}};
    resetUndo();
    emit filePathChanged(m_filePath);
    emit selectionChanged();
    emit documentChanged();
    emit statusMessage(QStringLiteral("Opened %1").arg(path));
    return true;
}

bool LayoutEditorSession::loadFromJson(const QByteArray& json, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromJson(json, doc, error)) {
        return false;
    }
    m_doc = std::move(doc);
    m_sel = {EditorTarget::Document, {}};
    resetUndo();
    m_undo.resetClean();
    emit selectionChanged();
    emit documentChanged();
    return true;
}

bool LayoutEditorSession::importFromFile(const QString& path, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    m_doc = std::move(doc);
    m_filePath.clear();
    m_sel = {EditorTarget::Document, {}};
    resetUndo();
    m_undo.resetClean();
    emit filePathChanged(m_filePath);
    emit selectionChanged();
    emit documentChanged();
    emit statusMessage(QStringLiteral("Imported %1").arg(path));
    return true;
}

bool LayoutEditorSession::save(QString* error)
{
    if (m_filePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No file path");
        }
        return false;
    }
    return saveTo(m_filePath, error);
}

bool LayoutEditorSession::saveTo(const QString& path, QString* error)
{
    if (!LayoutWriter::saveToFile(m_doc, path, error)) {
        return false;
    }
    m_filePath = path;
    m_undo.setClean();
    emit filePathChanged(m_filePath);
    emit statusMessage(QStringLiteral("Saved %1").arg(path));
    return true;
}

bool LayoutEditorSession::exportTo(const QString& path, QString* error)
{
    if (!LayoutWriter::saveToFile(m_doc, path, error)) {
        return false;
    }
    emit statusMessage(QStringLiteral("Exported %1").arg(path));
    return true;
}

void LayoutEditorSession::closeDocument()
{
    newDocument();
}

void LayoutEditorSession::setSelection(EditorSelection sel)
{
    if (sel.target == EditorTarget::Item && !itemById(sel.itemId)) {
        sel = {EditorTarget::Document, {}};
    }
    if (m_sel.target == sel.target && m_sel.itemId == sel.itemId) {
        return;
    }
    m_sel = std::move(sel);
    emit selectionChanged();
}

void LayoutEditorSession::selectItem(const QString& itemId)
{
    if (itemId.isEmpty()) {
        setSelection({EditorTarget::Document, {}});
        return;
    }
    setSelection({EditorTarget::Item, itemId});
}

void LayoutEditorSession::selectTarget(EditorTarget target)
{
    setSelection({target, {}});
}

void LayoutEditorSession::edit(const QString& label, const std::function<void(LayoutDocument&)>& fn)
{
    LayoutDocument before = m_doc;
    fn(m_doc);
    ensureGridFits(m_doc);
    if (LayoutWriter::toBytes(before) == LayoutWriter::toBytes(m_doc)) {
        m_doc = std::move(before);
        return;
    }
    m_undo.push(new SnapshotCommand(this, std::move(before), m_doc, label));
    emit documentChanged();
}

void LayoutEditorSession::addItem(EditorItemKind kind)
{
    int row = 0;
    int col = 0;
    if (!findEmptyCell(row, col)) {
        row = m_doc.grid.rows;
        col = 0;
    }
    const QString stem = kind == EditorItemKind::Unbounded ? QStringLiteral("edge")
                                                           : QStringLiteral("item");
    LayoutItem item;
    item.id = uniqueItemId(stem);
    item.label = kind == EditorItemKind::Unbounded ? QStringLiteral("Edge")
                                                   : QStringLiteral("Button");
    item.role = roleForKind(kind);
    item.applyKind();
    if (kind == EditorItemKind::Label || kind == EditorItemKind::Slider) {
        item.interactive = false;
    }
    if (kind == EditorItemKind::Unbounded) {
        item.unbounded = true;
        item.hasDwellRegion = true;
        item.dwellRegion.screenAnchor = LayoutDwellRegion::ScreenAnchor::Bottom;
        item.dwellRegion.width = DimSpec::pixels(160);
        item.dwellRegion.height = DimSpec::pixels(48);
        item.label = QStringLiteral("Edge");
    } else {
        item.row = row;
        item.col = col;
        item.action.type = LayoutAction::Type::Command;
        item.action.name = QStringLiteral("enter");
        item.actions = {item.action};
    }
    const QString newId = item.id;
    edit(QStringLiteral("Add %1").arg(item.label), [&](LayoutDocument& d) {
        d.items.push_back(item);
        expandGridForItem(d, item);
    });
    selectItem(newId);
}

void LayoutEditorSession::duplicateSelected()
{
    const LayoutItem* src = selectedItem();
    if (!src) {
        return;
    }
    LayoutItem copy = *src;
    copy.id = uniqueItemId(src->id);
    if (copy.participatesInBoardGrid()) {
        copy.col = src->col + src->colSpan;
        if (copy.col >= m_doc.grid.columns) {
            copy.col = 0;
            copy.row = src->row + src->rowSpan;
        }
    }
    const QString newId = copy.id;
    edit(QStringLiteral("Duplicate %1").arg(src->id), [&](LayoutDocument& d) {
        d.items.push_back(copy);
        expandGridForItem(d, copy);
    });
    selectItem(newId);
}

void LayoutEditorSession::deleteSelected()
{
    if (m_sel.target != EditorTarget::Item || m_sel.itemId.isEmpty()) {
        return;
    }
    const QString id = m_sel.itemId;
    edit(QStringLiteral("Delete %1").arg(id), [&](LayoutDocument& d) {
        d.items.erase(std::remove_if(d.items.begin(), d.items.end(),
                                     [&](const LayoutItem& it) { return it.id == id; }),
                      d.items.end());
    });
    setSelection({EditorTarget::Document, {}});
}

void LayoutEditorSession::cutSelected()
{
    copySelected();
    deleteSelected();
}

void LayoutEditorSession::copySelected()
{
    const LayoutItem* src = selectedItem();
    if (!src) {
        return;
    }
    m_clipboard = *src;
    emit statusMessage(QStringLiteral("Copied %1").arg(src->id));
}

void LayoutEditorSession::pasteClipboard()
{
    if (!m_clipboard) {
        return;
    }
    LayoutItem item = *m_clipboard;
    item.id = uniqueItemId(item.id);
    if (item.participatesInBoardGrid()) {
        int row = 0;
        int col = 0;
        if (findEmptyCell(row, col)) {
            item.row = row;
            item.col = col;
        } else {
            item.row = m_doc.grid.rows;
            item.col = 0;
        }
    }
    const QString newId = item.id;
    edit(QStringLiteral("Paste %1").arg(item.id), [&](LayoutDocument& d) {
        d.items.push_back(item);
        expandGridForItem(d, item);
    });
    selectItem(newId);
}

void LayoutEditorSession::moveItemToCell(const QString& itemId, int row, int col)
{
    row = qMax(0, row);
    col = qMax(0, col);
    edit(QStringLiteral("Move %1").arg(itemId), [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (it.id != itemId) {
                continue;
            }
            it.row = row;
            it.col = col;
            expandGridForItem(d, it);
            break;
        }
    });
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
    if (!itemById(base)) {
        return base;
    }
    for (int n = 2; n < 10000; ++n) {
        const QString id = QStringLiteral("%1_%2").arg(base).arg(n);
        if (!itemById(id)) {
            return id;
        }
    }
    return base + QStringLiteral("_x");
}

void LayoutEditorSession::restoreDocument(LayoutDocument doc)
{
    const QString keepItem = m_sel.itemId;
    const EditorTarget keepTarget = m_sel.target;
    m_doc = std::move(doc);
    if (keepTarget == EditorTarget::Item && !itemById(keepItem)) {
        m_sel = {EditorTarget::Document, {}};
        emit selectionChanged();
    }
    emit documentChanged();
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
    m_undo.setClean();
}

void LayoutEditorSession::ensureGridFits(LayoutDocument& doc)
{
    int maxRow = 0;
    int maxCol = 0;
    for (const LayoutItem& it : doc.items) {
        if (!it.participatesInBoardGrid()) {
            continue;
        }
        maxRow = qMax(maxRow, it.row + it.rowSpan - 1);
        maxCol = qMax(maxCol, it.col + it.colSpan - 1);
    }
    doc.grid.rows = qMax(doc.grid.rows, maxRow + 1);
    doc.grid.columns = qMax(doc.grid.columns, maxCol + 1);
    if (doc.grid.rows < 1) {
        doc.grid.rows = 1;
    }
    if (doc.grid.columns < 1) {
        doc.grid.columns = 1;
    }
}

void LayoutEditorSession::expandGridForItem(LayoutDocument& doc, const LayoutItem& item)
{
    if (!item.participatesInBoardGrid()) {
        return;
    }
    doc.grid.rows = qMax(doc.grid.rows, item.row + item.rowSpan);
    doc.grid.columns = qMax(doc.grid.columns, item.col + item.colSpan);
}

bool LayoutEditorSession::findEmptyCell(int& row, int& col) const
{
    const int rows = qMax(1, m_doc.grid.rows);
    const int cols = qMax(1, m_doc.grid.columns);
    QVector<QVector<bool>> used(rows, QVector<bool>(cols, false));
    for (const LayoutItem& it : m_doc.items) {
        if (!it.participatesInBoardGrid()) {
            continue;
        }
        for (int r = it.row; r < it.row + it.rowSpan && r < rows; ++r) {
            for (int c = it.col; c < it.col + it.colSpan && c < cols; ++c) {
                if (r >= 0 && c >= 0) {
                    used[r][c] = true;
                }
            }
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!used[r][c]) {
                row = r;
                col = c;
                return true;
            }
        }
    }
    return false;
}

} // namespace gazer
