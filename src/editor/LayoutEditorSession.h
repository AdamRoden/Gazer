#pragma once

#include "layout/PageTypes.h"

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
    Grid,
    Dwell,
    Style,
    Item
};

struct EditorSelection {
    EditorTarget target = EditorTarget::None;
    QString itemId;
    QStringList itemIds;

    [[nodiscard]] bool matches(EditorTarget t, const QString& id) const
    {
        if (target != t) {
            return false;
        }
        if (t == EditorTarget::Item) {
            return itemIds.contains(id);
        }
        if (t == EditorTarget::Document) {
            return true;
        }
        return itemId == id;
    }
};

struct EditorIssue {
    QString message;
    QString itemId;
    EditorTarget target = EditorTarget::Document;
};

enum class EditorItemKind {
    Button,
    Label,
    Toggle,
    Tab,
    Slider,
    Zone
};

enum class EditorTemplate {
    Blank,
    Keyboard,
    KeyboardRow,
    SettingsRow,
    EdgeChip
};

struct EditorClip {
    enum class Kind { Cell, Zone };
    Kind kind = Kind::Cell;
    PageCell cell;
    PageZone zone;
    QString gridId;
};

/// In-memory XML page, undo stack, clipboard, and selection.
class LayoutEditorSession final : public QObject {
    Q_OBJECT

public:
    explicit LayoutEditorSession(QObject* parent = nullptr);

    [[nodiscard]] const PageDocument& document() const;
    [[nodiscard]] QString filePath() const { return m_filePath; }
    [[nodiscard]] bool isDirty() const { return m_dirty; }
    [[nodiscard]] EditorSelection selection() const { return m_sel; }
    [[nodiscard]] QUndoStack& undoStack() { return m_undo; }
    [[nodiscard]] bool hasClipboard() const { return !m_clipboard.isEmpty(); }
    [[nodiscard]] bool isItemSelected(const QString& id) const;

    [[nodiscard]] PageLeaf* selectedItem();
    [[nodiscard]] const PageLeaf* selectedItem() const;
    [[nodiscard]] PageLeaf* itemById(const QString& id);
    [[nodiscard]] const PageLeaf* itemById(const QString& id) const;
    [[nodiscard]] bool selectedIsZone() const;

    void newDocument();
    void newFromTemplate(EditorTemplate tmpl, const QString& id, const QString& name);
    void setPlaceKind(std::optional<EditorItemKind> kind);
    [[nodiscard]] std::optional<EditorItemKind> placeKind() const { return m_placeKind; }
    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool importFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool save(QString* error = nullptr);
    [[nodiscard]] bool saveTo(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool exportTo(const QString& path, QString* error = nullptr);
    void closeDocument();

    void setSelection(EditorSelection sel);
    void selectItem(const QString& itemId, bool additive = false);
    void selectItems(const QStringList& ids);
    void selectTarget(EditorTarget target);
    void selectGrid(const QString& gridId);
    void selectByTarget(EditorTarget target, const QString& id);
    [[nodiscard]] QString selectedGridId() const;
    [[nodiscard]] const PageGrid* selectedGrid() const;

    void edit(const QString& label, const std::function<void(PageDocument&)>& fn);

    void addItem(EditorItemKind kind);
    void addItemAt(EditorItemKind kind, int row, int col, const QString& gridId = {});
    void addZoneAt(const QPoint& virtTopLeft);
    void duplicateSelected();
    void deleteSelected();
    void cutSelected();
    void copySelected();
    void pasteClipboard();
    void moveItemToCell(const QString& itemId, int row, int col);
    void moveSelected(int dRow, int dCol);
    void nudgeSelected(int dRow, int dCol, int freePx);
    void resizeItem(const QString& itemId, int rowSpan, int colSpan, double widthUnits);
    void resizeFreeItem(const QString& itemId, const PageDim& width, const PageDim& height);
    void raiseSelected();
    void lowerSelected();
    void convertSelectedToFree();
    void convertSelectedToCell();
    void setItemLabel(const QString& itemId, const QString& label);
    void addTopGrid();
    void addSubGrid();
    void addNamedStyle();
    void addNamedDwell();
    void addGridRow();
    void addGridColumn();
    void packGrid();
    void equalizeSelectedWidths();
    void alignSelectedRow();
    void applyChromeToSelected(const std::function<void(PageChrome&)>& mut, const QString& undoLabel);
    void setActions(const QString& itemId, QVector<PageAction> acts);
    void snapWindowTo(const QPoint& virtualTopLeft, const QSize& virtualScreen);
    [[nodiscard]] QString uniqueItemId(const QString& stem) const;
    [[nodiscard]] QVector<EditorIssue> validate(const QStringList& catalogIds = {}) const;
    void notify(const QString& msg);

signals:
    void documentChanged();
    void selectionChanged();
    void dirtyChanged(bool dirty);
    void filePathChanged(const QString& path);
    void statusMessage(const QString& msg);
    void placeKindChanged();

private:
    class EditCommand;
    friend class EditCommand;

    [[nodiscard]] PageDocument& currentDoc();
    void restoreDocument(PageDocument doc);
    void replaceDocument(PageDocument doc, const QString& path, bool dirty);
    void setDirty(bool dirty);
    void resetUndo();
    [[nodiscard]] QStringList actionItemIds(const QString& itemId) const;

    PageDocument m_doc;
    QString m_filePath;
    bool m_dirty = false;
    EditorSelection m_sel;
    QUndoStack m_undo;
    QVector<EditorClip> m_clipboard;
    std::optional<EditorItemKind> m_placeKind;
};

} // namespace gazer
