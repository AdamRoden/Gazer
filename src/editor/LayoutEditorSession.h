#pragma once

#include "layout/LayoutTypes.h"

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QVector>
#include <functional>
#include <optional>

namespace gazer {

enum class EditorTarget {
    None,
    Document,
    Window,
    Grid,
    Dwell,
    Style,
    Item
};

struct EditorSelection {
    EditorTarget target = EditorTarget::None;
    QString itemId;
    QStringList itemIds;
};

enum class EditorItemKind {
    Button,
    Label,
    Toggle,
    Tab,
    Slider,
    Unbounded
};

enum class EditorTemplate {
    Blank,
    Keyboard,
    KeyboardRow,
    SettingsRow,
    EdgeChip
};

struct EditorLayer {
    QString name;
    QString suffix;
    LayoutDocument doc;
};

/// In-memory layout family, undo stack, clipboard, and selection for the designer.
/// `document()` is the current layer; there is no second copy of that board.
class LayoutEditorSession final : public QObject {
    Q_OBJECT

public:
    explicit LayoutEditorSession(QObject* parent = nullptr);

    [[nodiscard]] const LayoutDocument& document() const;
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] EditorSelection selection() const { return m_sel; }
    [[nodiscard]] QUndoStack& undoStack() { return m_undo; }
    [[nodiscard]] bool hasClipboard() const { return !m_clipboard.isEmpty(); }
    [[nodiscard]] bool isItemSelected(const QString& id) const;

    [[nodiscard]] LayoutItem* selectedItem();
    [[nodiscard]] const LayoutItem* selectedItem() const;
    [[nodiscard]] LayoutItem* itemById(const QString& id);
    [[nodiscard]] const LayoutItem* itemById(const QString& id) const;

    void newDocument();
    void newFromTemplate(EditorTemplate tmpl, const QString& id, const QString& name);
    [[nodiscard]] const QVector<EditorLayer>& layers() const { return m_layers; }
    [[nodiscard]] int layerIndex() const { return m_layerIndex; }
    void setLayer(int index);
    void setPlaceKind(std::optional<EditorItemKind> kind);
    [[nodiscard]] std::optional<EditorItemKind> placeKind() const { return m_placeKind; }
    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool loadFromJson(const QByteArray& json, QString* error = nullptr);
    [[nodiscard]] bool importFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool save(QString* error = nullptr);
    [[nodiscard]] bool saveTo(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool exportTo(const QString& path, QString* error = nullptr);
    void closeDocument();

    void setSelection(EditorSelection sel);
    void selectItem(const QString& itemId, bool additive = false);
    void selectItems(const QStringList& ids);
    void selectTarget(EditorTarget target);

    /// Snapshot-undo mutation of the whole family. No-op if nothing changed.
    void edit(const QString& label, const std::function<void(LayoutDocument&)>& fn);

    void addItem(EditorItemKind kind);
    void addItemAt(EditorItemKind kind, int row, int col);
    void duplicateSelected();
    void deleteSelected();
    void cutSelected();
    void copySelected();
    void pasteClipboard();
    void moveItemToCell(const QString& itemId, int row, int col);
    void moveSelected(int dRow, int dCol);
    void resizeItem(const QString& itemId, int rowSpan, int colSpan, double widthUnits);
    void setItemLabel(const QString& itemId, const QString& label);
    void addGridRow();
    void addGridColumn();
    void packGrid();
    void equalizeSelectedWidths();
    void alignSelectedRow();
    void applyChromeToSelected(const std::function<void(LayoutChromeStyle&)>& mut,
                               const QString& undoLabel);
    void setActions(const QString& itemId, QVector<LayoutAction> acts);
    void snapWindowTo(const QPoint& virtualTopLeft, const QSize& virtualScreen);
    [[nodiscard]] QString uniqueItemId(const QString& stem) const;
    void notify(const QString& msg);

signals:
    void documentChanged();
    void selectionChanged();
    void dirtyChanged(bool dirty);
    void filePathChanged(const QString& path);
    void statusMessage(const QString& msg);
    void layerChanged();
    void placeKindChanged();

private:
    class SnapshotCommand;
    friend class SnapshotCommand;

    [[nodiscard]] LayoutDocument& currentDoc();
    void restoreProject(QVector<EditorLayer> layers, int layerIndex);
    void replaceProject(QVector<EditorLayer> layers, int layerIndex, const QString& path,
                        bool dirty);
    void setDirty(bool dirty);
    void resetUndo();
    void ensureGridFits(LayoutDocument& doc);
    void expandGridForItem(LayoutDocument& doc, const LayoutItem& item);
    [[nodiscard]] bool findEmptyCell(int& row, int& col) const;
    [[nodiscard]] QStringList actionItemIds(const QString& itemId) const;

    QVector<EditorLayer> m_layers;
    int m_layerIndex = 0;
    QString m_filePath;
    bool m_dirty = false;
    EditorSelection m_sel;
    QUndoStack m_undo;
    QVector<LayoutItem> m_clipboard;
    std::optional<EditorItemKind> m_placeKind;
};

} // namespace gazer
