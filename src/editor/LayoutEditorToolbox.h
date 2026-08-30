#pragma once

#include "editor/LayoutEditorIcons.h"
#include "ui/Theme.h"

#include <QVector>
#include <QWidget>
#include <optional>

class QAction;
class QGridLayout;
class QLabel;
class QToolButton;
class QTreeWidget;
class QVBoxLayout;

namespace gazer {

class LayoutEditorSession;

/// Left rail: add palette and the board element tree.
class LayoutEditorToolbox final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorToolbox(QWidget* parent = nullptr);

    void bindSession(LayoutEditorSession& session);
    void setTheme(const ThemeColors& theme);

    void addPlaceAction(const QString& section, QAction* action, EditorGlyph glyph,
                        EditorItemKind kind, const QString& label);
    void addPaletteAction(const QString& section, QAction* action, EditorGlyph glyph,
                          const QString& label);

private:
    struct PaletteItem {
        QString section;
        QAction* action = nullptr;
        EditorGlyph glyph = EditorGlyph::Button;
        std::optional<EditorItemKind> place;
        QString label;
        QToolButton* button = nullptr;
    };
    struct Section {
        QString title;
        QGridLayout* grid = nullptr;
        int count = 0;
    };

    void addPaletteItem(PaletteItem item);
    Section& ensureSection(const QString& title);
    void refreshPaletteIcons();
    void syncPlaceButtons();
    void rebuildHierarchy();

    LayoutEditorSession* m_session = nullptr;
    ThemeColors m_theme = ThemeColors::darkPreset();
    QVBoxLayout* m_paletteLayout = nullptr;
    QVector<Section> m_sections;
    QVector<PaletteItem> m_palette;
    QLabel* m_elementsTitle = nullptr;
    QTreeWidget* m_hierarchy = nullptr;
    QString m_treeKey;
};

} // namespace gazer
