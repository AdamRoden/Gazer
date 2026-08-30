#include "editor/LayoutEditorStyle.h"

#include <QPair>

namespace gazer {

QString editorStyleSheet(const ThemeColors& t)
{
    const auto hex = [](const QColor& c) { return ThemeColors::colorToHex(c); };
    QString ss = QStringLiteral(R"(
        QMainWindow, QSplitter, QDialog {
            background-color: @bgMain;
            color: @text;
            font-family: "Segoe UI Variable", "Segoe UI";
            font-size: 13px;
        }
        QLabel, QCheckBox, QTabBar, QTabWidget, QScrollArea {
            background: transparent;
            color: @text;
            font-family: "Segoe UI Variable", "Segoe UI";
            font-size: 13px;
        }
        QWidget#editorToolbox, QWidget#editorProperties {
            background-color: @bgMain;
            color: @text;
        }
        QMenuBar {
            background: @bgMain;
            color: @text;
            padding: 4px 8px;
            border-bottom: 1px solid @border;
        }
        QMenuBar::item { padding: 4px 8px; border-radius: 4px; }
        QMenuBar::item:selected { background: @bgHover; }
        QMenu {
            background: @bgSurface;
            color: @text;
            border: 1px solid @border;
            padding: 4px;
        }
        QMenu::item { padding: 6px 18px 6px 12px; border-radius: 4px; }
        QMenu::item:selected { background: @bgHover; }
        QMenu::separator { height: 1px; background: @border; margin: 4px 8px; }
        QToolBar {
            background: @bgSurface;
            border: none;
            border-bottom: 1px solid @border;
            padding: 6px 10px;
            spacing: 4px;
        }
        QToolBar QToolButton {
            background: transparent;
            color: @text;
            padding: 6px;
            border-radius: 6px;
            margin: 0 1px;
        }
        QToolBar QToolButton:hover { background: @bgHover; }
        QToolBar QToolButton:checked { background: @cellActive; }
        QToolBar QToolButton:disabled { color: @textMuted; }
        QToolBar::separator {
            width: 1px;
            background: @border;
            margin: 6px 6px;
        }
        QStatusBar {
            background: @bgSurface;
            color: @textMuted;
            border-top: 1px solid @border;
            padding: 2px 8px;
        }
        QStatusBar::item { border: none; }
        QLabel#statusChip, QLabel#statusChipDanger {
            background: @bgActive;
            color: @textMuted;
            border: 1px solid @border;
            border-radius: 10px;
            padding: 2px 9px;
            margin-left: 6px;
        }
        QLabel#statusChipDanger, QPushButton#statusChipDanger {
            color: @danger;
            border-color: @danger;
            background: @bgActive;
        }
        QPushButton#statusChipDanger {
            padding: 2px 9px;
            border-radius: 10px;
            margin-left: 6px;
        }
        QPushButton#statusChipDanger:hover { background: @bgHover; }
        QSplitter::handle {
            background: @bgMain;
            width: 6px;
            height: 6px;
        }
        QTabWidget::pane { border: none; background: transparent; }
        QScrollArea { border: none; background: transparent; }
        QTreeWidget, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit {
            background: @bgActive;
            color: @text;
            border: 1px solid @border;
            border-radius: 6px;
            selection-background-color: @cellActive;
            selection-color: @text;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            padding: 5px 8px;
            min-height: 26px;
            min-width: 0px;
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid @accent;
        }
        QWidget#editorProperties QLineEdit,
        QWidget#editorProperties QSpinBox,
        QWidget#editorProperties QDoubleSpinBox,
        QWidget#editorProperties QComboBox {
            min-width: 0px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background: @bgActive;
            color: @text;
            selection-background-color: @cellActive;
            border: 1px solid @border;
            outline: none;
        }
        QTabBar::tab {
            background: transparent;
            color: @textMuted;
            padding: 8px 12px;
            border: none;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            color: @text;
            border-bottom: 2px solid @accent;
        }
        QTabBar::tab:hover { color: @text; }
        QCheckBox { color: @text; spacing: 8px; }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid @border;
            border-radius: 4px;
            background: @bgActive;
        }
        QCheckBox::indicator:checked {
            background: @accent;
            border-color: @accent;
        }
        QHeaderView::section { background: @bgActive; color: @textMuted; border: none; }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: transparent;
            width: 10px;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: @border;
            border-radius: 5px;
            min-height: 24px;
            min-width: 24px;
        }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
            background: @textMuted;
        }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QLabel#panelTitle {
            font-size: 15px;
            font-weight: 600;
            color: @text;
            padding: 2px 2px 2px 2px;
        }
        QLabel#panelSection {
            font-size: 11px;
            font-weight: 600;
            color: @textMuted;
            padding: 4px 2px 0 2px;
        }
        QLabel#fieldHeading {
            font-size: 11px;
            font-weight: 600;
            color: @textMuted;
            padding-top: 4px;
        }
        QLabel#fieldNote { color: @textMuted; font-size: 12px; }
        QFrame#editorCard {
            background: @bgSurface;
            border: 1px solid @border;
            border-radius: 8px;
        }
        QWidget#selectionHeader {
            background: @bgSurface;
            border: 1px solid @border;
            border-radius: 8px;
        }
        QLabel#selectionKind {
            color: @accent;
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#selectionTitle {
            font-size: 15px;
            font-weight: 600;
            color: @text;
        }
        QLabel#selectionId {
            color: @textMuted;
            font-size: 11px;
        }
        QPushButton {
            background: @bgActive;
            color: @text;
            border: 1px solid @border;
            border-radius: 6px;
            padding: 5px 12px;
        }
        QPushButton:hover { background: @bgHover; }
        QTreeWidget {
            outline: none;
            padding: 4px;
        }
        QTreeWidget::item { padding: 5px 4px; border-radius: 4px; }
        QTreeWidget::item:selected { background: @cellActive; }
        QTreeWidget::item:hover { background: @bgHover; }
        QToolButton#paletteButton {
            background: @bgActive;
            color: @text;
            border: 1px solid @border;
            border-radius: 8px;
            padding: 8px 4px 6px 4px;
            font-size: 11px;
        }
        QToolButton#paletteButton:hover { background: @bgHover; }
        QToolButton#paletteButton:checked {
            background: @cellActive;
            border-color: @accent;
        }
        QPlainTextEdit#codeView {
            background: @bgSurface;
            color: @text;
            border: none;
            font-family: "Cascadia Mono", "Consolas", "Courier New";
            font-size: 12px;
            padding: 10px;
        }
    )");
    const QList<QPair<QString, QString>> tokens = {
        {QStringLiteral("@textMuted"), hex(t.textSecondary)},
        {QStringLiteral("@bgSurface"), hex(t.bgSurface)},
        {QStringLiteral("@bgActive"), hex(t.bgSurfaceActive)},
        {QStringLiteral("@cellActive"), hex(t.cellActive)},
        {QStringLiteral("@bgHover"), hex(t.bgSurfaceHover)},
        {QStringLiteral("@bgMain"), hex(t.bgMain)},
        {QStringLiteral("@border"), hex(t.border)},
        {QStringLiteral("@accent"), hex(t.accent)},
        {QStringLiteral("@danger"), hex(t.danger)},
        {QStringLiteral("@text"), hex(t.text)},
    };
    for (const auto& tok : tokens) {
        ss.replace(tok.first, tok.second);
    }
    return ss;
}

} // namespace gazer
