#include "editor/LayoutEditorSession.h"

#include "editor/LayoutEditorKeyboard.h"
#include "layout/LayoutWriter.h"

#include <QRegularExpression>
#include <QUndoCommand>

namespace gazer {

class LayoutEditorSession::SnapshotCommand final : public QUndoCommand {
public:
    SnapshotCommand(LayoutEditorSession* session, QVector<EditorLayer> before,
                    QVector<EditorLayer> after, int layerIndex, const QString& text)
        : QUndoCommand(text)
        , m_session(session)
        , m_before(std::move(before))
        , m_after(std::move(after))
        , m_layerIndex(layerIndex)
    {
    }

    void undo() override { m_session->restoreProject(m_before, m_layerIndex); }
    void redo() override
    {
        if (m_virgin) {
            m_virgin = false;
            return;
        }
        m_session->restoreProject(m_after, m_layerIndex);
    }

private:
    LayoutEditorSession* m_session = nullptr;
    QVector<EditorLayer> m_before;
    QVector<EditorLayer> m_after;
    int m_layerIndex = 0;
    bool m_virgin = true;
};

namespace {

QByteArray familyFingerprint(const QVector<EditorLayer>& layers)
{
    QByteArray out;
    for (const EditorLayer& layer : layers) {
        out += layer.suffix.toUtf8();
        out += '\0';
        out += LayoutWriter::toBytes(layer.doc);
        out += '\0';
    }
    return out;
}

} // namespace

LayoutEditorSession::LayoutEditorSession(QObject* parent)
    : QObject(parent)
{
    connect(&m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { setDirty(!clean); });
    newDocument();
}

const LayoutDocument& LayoutEditorSession::document() const
{
    return m_layers[m_layerIndex].doc;
}

LayoutDocument& LayoutEditorSession::currentDoc()
{
    return m_layers[m_layerIndex].doc;
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
    for (LayoutItem& it : currentDoc().items) {
        if (it.id == id) {
            return &it;
        }
    }
    return nullptr;
}

const LayoutItem* LayoutEditorSession::itemById(const QString& id) const
{
    return document().findItem(id);
}

void LayoutEditorSession::newDocument()
{
    replaceProject(makeBlankLayers(), 0, {}, false);
}

void LayoutEditorSession::newFromTemplate(EditorTemplate tmpl, const QString& id, const QString& name)
{
    replaceProject(makeTemplateLayers(tmpl, id, name), 0, {}, true);
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

void LayoutEditorSession::setPlaceKind(std::optional<EditorItemKind> kind)
{
    m_placeKind = kind;
    emit placeKindChanged();
    if (kind) {
        emit statusMessage(QStringLiteral("Click a cell to place"));
    }
}

void LayoutEditorSession::setLayer(int index)
{
    if (index < 0 || index >= m_layers.size() || index == m_layerIndex) {
        return;
    }
    m_layerIndex = index;
    m_sel = {EditorTarget::Document, {}, {}};
    emit selectionChanged();
    emit documentChanged();
    emit layerChanged();
}

void LayoutEditorSession::edit(const QString& label, const std::function<void(LayoutDocument&)>& fn)
{
    const QVector<EditorLayer> before = m_layers;
    const int layer = m_layerIndex;
    fn(currentDoc());
    ensureGridFits(currentDoc());
    if (familyFingerprint(before) == familyFingerprint(m_layers)) {
        m_layers = before;
        return;
    }
    m_undo.push(new SnapshotCommand(this, before, m_layers, layer, label));
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
    auto taken = [this](const QString& id) { return document().findItem(id) != nullptr; };
    if (!taken(base)) {
        return base;
    }
    for (int n = 2; n < 10000; ++n) {
        const QString id = QStringLiteral("%1_%2").arg(base).arg(n);
        if (!taken(id)) {
            return id;
        }
    }
    return base + QStringLiteral("_x");
}

void LayoutEditorSession::restoreProject(QVector<EditorLayer> layers, int layerIndex)
{
    m_layers = std::move(layers);
    m_layerIndex = qBound(0, layerIndex, m_layers.size() - 1);
    const EditorTarget keepTarget = m_sel.target;
    const QStringList keepIds = m_sel.itemIds;
    if (keepTarget == EditorTarget::Item) {
        QStringList ids;
        for (const QString& id : keepIds) {
            if (itemById(id)) {
                ids.push_back(id);
            }
        }
        if (ids.isEmpty()) {
            m_sel = {EditorTarget::Document, {}, {}};
        } else {
            m_sel = {EditorTarget::Item, ids.first(), ids};
        }
        emit selectionChanged();
    }
    emit documentChanged();
    emit layerChanged();
}

void LayoutEditorSession::replaceProject(QVector<EditorLayer> layers, int layerIndex,
                                         const QString& path, bool dirty)
{
    if (layers.isEmpty()) {
        layers = makeBlankLayers();
    }
    m_layers = std::move(layers);
    m_layerIndex = qBound(0, layerIndex, m_layers.size() - 1);
    m_filePath = path;
    m_sel = {EditorTarget::Document, {}, {}};
    m_clipboard.clear();
    m_placeKind.reset();
    resetUndo();
    if (dirty) {
        m_undo.resetClean();
    }
    emit filePathChanged(m_filePath);
    emit selectionChanged();
    emit documentChanged();
    emit layerChanged();
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
    m_undo.setClean();
}

} // namespace gazer
