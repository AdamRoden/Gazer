#pragma once

#include "layout/LayoutTypes.h"

#include <QObject>
#include <QString>
#include <QUndoStack>
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
};

enum class EditorItemKind {
    Button,
    Label,
    Toggle,
    Tab,
    Slider,
    Unbounded
};

/// In-memory layout document, undo stack, clipboard, and selection for the designer.
class LayoutEditorSession final : public QObject {
    Q_OBJECT

public:
    explicit LayoutEditorSession(QObject* parent = nullptr);

    [[nodiscard]] const LayoutDocument& document() const { return m_doc; }
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] EditorSelection selection() const { return m_sel; }
    [[nodiscard]] QUndoStack& undoStack() { return m_undo; }
    [[nodiscard]] bool hasClipboard() const { return m_clipboard.has_value(); }

    [[nodiscard]] LayoutItem* selectedItem();
    [[nodiscard]] const LayoutItem* selectedItem() const;
    [[nodiscard]] LayoutItem* itemById(const QString& id);
    [[nodiscard]] const LayoutItem* itemById(const QString& id) const;

    void newDocument();
    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool loadFromJson(const QByteArray& json, QString* error = nullptr);
    [[nodiscard]] bool importFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool save(QString* error = nullptr);
    [[nodiscard]] bool saveTo(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool exportTo(const QString& path, QString* error = nullptr);
    void closeDocument();

    void setSelection(EditorSelection sel);
    void selectItem(const QString& itemId);
    void selectTarget(EditorTarget target);

    /// Snapshot-undo mutation. No-op if the document is unchanged.
    void edit(const QString& label, const std::function<void(LayoutDocument&)>& fn);

    void addItem(EditorItemKind kind);
    void duplicateSelected();
    void deleteSelected();
    void cutSelected();
    void copySelected();
    void pasteClipboard();
    void moveItemToCell(const QString& itemId, int row, int col);
    [[nodiscard]] QString uniqueItemId(const QString& stem) const;
    void notify(const QString& msg);

signals:
    void documentChanged();
    void selectionChanged();
    void dirtyChanged(bool dirty);
    void filePathChanged(const QString& path);
    void statusMessage(const QString& msg);

private:
    class SnapshotCommand;
    friend class SnapshotCommand;

    void restoreDocument(LayoutDocument doc);
    void setDirty(bool dirty);
    void resetUndo();
    void ensureGridFits(LayoutDocument& doc);
    void expandGridForItem(LayoutDocument& doc, const LayoutItem& item);
    [[nodiscard]] bool findEmptyCell(int& row, int& col) const;

    LayoutDocument m_doc;
    QString m_filePath;
    bool m_dirty = false;
    EditorSelection m_sel;
    QUndoStack m_undo;
    std::optional<LayoutItem> m_clipboard;
};

} // namespace gazer
