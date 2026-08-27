#include "editor/LayoutEditorWindow.h"

#include "editor/LayoutEditorCanvas.h"
#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorProperties.h"
#include "editor/LayoutEditorSession.h"
#include "editor/LayoutEditorToolbox.h"
#include "layout/PageEdit.h"
#include "ui/AppIcon.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSet>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <optional>

namespace gazer {

namespace {

QString fluentStyleSheet(const ThemeColors& t)
{
    const auto h = [](const QColor& c) { return ThemeColors::colorToHex(c); };
    return QStringLiteral(R"(
        QMainWindow, QSplitter, QDialog {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI";
            font-size: 13px;
        }
        QLabel, QCheckBox, QTabBar, QTabWidget, QScrollArea {
            background: transparent;
            color: %2;
            font-family: "Segoe UI";
            font-size: 13px;
        }
        QWidget#editorToolbox, QWidget#editorProperties {
            background-color: %1;
            color: %2;
        }
        QMenuBar {
            background: %1;
            color: %2;
            padding: 2px 6px;
        }
        QMenuBar::item:selected { background: %3; }
        QMenu {
            background: %4;
            color: %2;
            border: 1px solid %5;
        }
        QMenu::item:selected { background: %3; }
        QToolBar {
            background: %4;
            border: none;
            padding: 4px 8px;
            spacing: 4px;
        }
        QToolBar QToolButton {
            background: transparent;
            color: %2;
            padding: 6px 10px;
            border-radius: 4px;
        }
        QToolBar QToolButton:hover { background: %3; }
        QToolBar QToolButton:checked { background: %6; color: %2; }
        QStatusBar {
            background: %1;
            color: %7;
        }
        QSplitter::handle { background: %5; width: 1px; }
        QTabWidget::pane { border: none; background: transparent; }
        QScrollArea { border: none; background: transparent; }
        QTreeWidget, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit {
            background: %8;
            color: %2;
            border: 1px solid %5;
            border-radius: 4px;
            selection-background-color: %6;
            selection-color: %2;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            padding: 4px 6px;
            min-height: 24px;
            min-width: 0px;
        }
        QWidget#editorProperties QLineEdit,
        QWidget#editorProperties QSpinBox,
        QWidget#editorProperties QDoubleSpinBox,
        QWidget#editorProperties QComboBox {
            min-width: 0px;
        }
        QComboBox QAbstractItemView {
            background: %8;
            color: %2;
            selection-background-color: %6;
        }
        QTabBar::tab {
            background: transparent;
            color: %7;
            padding: 8px 14px;
            border: none;
        }
        QTabBar::tab:selected {
            color: %2;
            border-bottom: 2px solid %9;
        }
        QCheckBox { color: %2; spacing: 8px; }
        QHeaderView::section { background: %8; color: %7; border: none; }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: %1;
            width: 10px;
            height: 10px;
        }
        QScrollBar::handle { background: %5; border-radius: 4px; }
        QLabel#panelTitle {
            font-size: 16px;
            font-weight: 600;
            color: %2;
            padding: 4px 2px 8px 2px;
        }
        QLabel#fieldHeading {
            font-size: 12px;
            font-weight: 600;
            color: %7;
            padding-top: 8px;
        }
        QLabel#fieldNote { color: %7; }
        QPushButton {
            background: %8;
            color: %2;
            border: 1px solid %5;
            border-radius: 4px;
            padding: 4px 10px;
        }
        QPushButton:hover { background: %3; }
        QTreeWidget::item { padding: 4px 2px; }
        QTreeWidget::item:selected { background: %6; }
        QTreeWidget::item:hover { background: %3; }
    )")
        .arg(h(t.bgMain), h(t.text), h(t.bgSurfaceHover), h(t.bgSurface), h(t.border),
             h(t.cellActive), h(t.textSecondary), h(t.bgSurfaceActive), h(t.accent));
}

} // namespace

LayoutEditorWindow::LayoutEditorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Gazer Page Editor"));
    const QIcon icon = loadAppIcon();
    if (!icon.isNull()) {
        setWindowIcon(icon);
    }
    resize(1400, 860);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect ag = screen->availableGeometry();
        resize(qMin(1480, ag.width() - 40), qMin(900, ag.height() - 60));
        move(ag.center() - QPoint(width() / 2, height() / 2));
    }

    m_session = new LayoutEditorSession(this);
    buildUi();
    applyFluentTheme();
    updateTitle();
    updateActions();

    connect(m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        updateTitle();
        updateActions();
    });
    connect(m_session, &LayoutEditorSession::selectionChanged, this, [this]() {
        updateTitle();
        updateActions();
    });
    connect(m_session, &LayoutEditorSession::dirtyChanged, this, [this](bool) { updateTitle(); });
    connect(m_session, &LayoutEditorSession::filePathChanged, this, [this](const QString&) {
        updateTitle();
    });
    connect(m_session, &LayoutEditorSession::statusMessage, this, [this](const QString& m) {
        statusBar()->showMessage(m, 4000);
    });
    connect(&m_session->undoStack(), &QUndoStack::canUndoChanged, this,
            &LayoutEditorWindow::updateActions);
    connect(&m_session->undoStack(), &QUndoStack::canRedoChanged, this,
            &LayoutEditorWindow::updateActions);
}

LayoutEditorWindow::~LayoutEditorWindow() = default;

void LayoutEditorWindow::setLayoutsDirectory(const QString& dir)
{
    m_layoutsDir = dir;
}

void LayoutEditorWindow::setUserLayoutsDirectory(const QString& dir)
{
    m_userDir = dir;
    QDir().mkpath(dir);
}

void LayoutEditorWindow::syncActionCatalog()
{
    if (!m_props) {
        return;
    }
    ActionCatalog cat;
    cat.commands = m_commandNames;
    for (const QString& n : m_commandNames) {
        cat.commandLabels.push_back(friendlyCommandLabel(n));
    }
    cat.layoutIds = m_catalogIds;
    cat.layoutLabels = m_catalogLabels.isEmpty() ? m_catalogIds : m_catalogLabels;
    m_props->setActionCatalog(cat);
}

void LayoutEditorWindow::setCatalog(const QStringList& ids, const QStringList& labels)
{
    m_catalogIds = ids;
    m_catalogLabels = labels;
    syncActionCatalog();
}

void LayoutEditorWindow::setCommandNames(const QStringList& names)
{
    m_commandNames = names;
    syncActionCatalog();
}

void LayoutEditorWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    applyFluentTheme();
    if (m_canvas) {
        m_canvas->setTheme(theme);
    }
}

void LayoutEditorWindow::setTestHandler(TestHandler handler)
{
    m_test = std::move(handler);
}

void LayoutEditorWindow::showAndRaise()
{
    show();
    raise();
    activateWindow();
}

void LayoutEditorWindow::closeEvent(QCloseEvent* event)
{
    if (!m_session->isDirty()) {
        event->accept();
        return;
    }
    const auto r = QMessageBox::question(
        this, QStringLiteral("Unsaved page"),
        QStringLiteral("Save changes to %1?").arg(m_session->document().id),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) {
        event->ignore();
        return;
    }
    if (r == QMessageBox::Save) {
        save();
        if (m_session->isDirty()) {
            event->ignore();
            return;
        }
    }
    event->accept();
}

QAction* LayoutEditorWindow::makeAction(const QString& text, const QKeySequence& shortcut,
                                        const std::function<void()>& slot)
{
    auto* a = new QAction(text, this);
    a->setShortcut(shortcut);
    connect(a, &QAction::triggered, this, slot);
    addAction(a);
    return a;
}

void LayoutEditorWindow::buildUi()
{
    auto* actNew = makeAction(QStringLiteral("&New"), QKeySequence::New, [this]() { newFile(); });
    auto* actOpen = makeAction(QStringLiteral("&Open…"), QKeySequence::Open, [this]() { open(); });
    m_save = makeAction(QStringLiteral("&Save"), QKeySequence::Save, [this]() { save(); });
    auto* actSaveAs =
        makeAction(QStringLiteral("Save &As…"), QKeySequence::SaveAs, [this]() { saveAs(); });
    auto* actImport =
        makeAction(QStringLiteral("&Import…"), QKeySequence(), [this]() { importFile(); });
    auto* actExport =
        makeAction(QStringLiteral("&Export…"), QKeySequence(), [this]() { exportFile(); });
    auto* actClose =
        makeAction(QStringLiteral("&Close"), QKeySequence::Close, [this]() { closeDoc(); });

    m_undo = makeAction(QStringLiteral("&Undo"), QKeySequence::Undo,
                        [this]() { m_session->undoStack().undo(); });
    m_redo = makeAction(QStringLiteral("&Redo"), QKeySequence::Redo,
                        [this]() { m_session->undoStack().redo(); });
    m_cut = makeAction(QStringLiteral("Cu&t"), QKeySequence::Cut,
                       [this]() { m_session->cutSelected(); });
    m_copy = makeAction(QStringLiteral("&Copy"), QKeySequence::Copy,
                        [this]() { m_session->copySelected(); });
    m_paste = makeAction(QStringLiteral("&Paste"), QKeySequence::Paste,
                         [this]() { m_session->pasteClipboard(); });
    m_delete = makeAction(QStringLiteral("&Delete"), QKeySequence::Delete,
                          [this]() { m_session->deleteSelected(); });

    m_grid = makeAction(QStringLiteral("Show &grid"), QKeySequence(), []() {});
    m_grid->setCheckable(true);
    m_grid->setChecked(true);
    m_fit = makeAction(QStringLiteral("&Fit grid"), QKeySequence(QStringLiteral("Ctrl+0")), []() {});
    m_fit->setCheckable(true);
    m_fit->setChecked(true);

    auto* actAddBtn = makeAction(QStringLiteral("Add &button"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Button);
    });
    auto* actAddLabel = makeAction(QStringLiteral("Add &label"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Label);
    });
    auto* actAddToggle = makeAction(QStringLiteral("Add &toggle"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Toggle);
    });
    auto* actAddTab = makeAction(QStringLiteral("Add t&ab"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Tab);
    });
    auto* actAddSlider = makeAction(QStringLiteral("Add &slider"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Slider);
    });
    auto* actAddEdge = makeAction(QStringLiteral("Add &zone"), QKeySequence(), [this]() {
        m_session->setPlaceKind(EditorItemKind::Zone);
    });
    auto* actDup = makeAction(QStringLiteral("D&uplicate selected"),
                              QKeySequence(QStringLiteral("Ctrl+D")),
                              [this]() { m_session->duplicateSelected(); });

    auto* actDoc = makeAction(QStringLiteral("&Page properties"), QKeySequence(), [this]() {
        m_props->showPageTab();
    });
    auto* actGrid = makeAction(QStringLiteral("Edit &grid"), QKeySequence(), [this]() {
        m_props->showGridTab();
    });
    auto* actDwell = makeAction(QStringLiteral("Edit &dwell"), QKeySequence(), [this]() {
        m_props->showDwellTab();
    });
    auto* actTest =
        makeAction(QStringLiteral("&Test on desktop"), QKeySequence(QStringLiteral("F5")), [this]() {
            testLive();
        });
    m_testMode = makeAction(QStringLiteral("Test on &canvas"), QKeySequence(QStringLiteral("F6")),
                            []() {});
    m_testMode->setCheckable(true);

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(actNew);
    fileMenu->addAction(actOpen);
    fileMenu->addAction(m_save);
    fileMenu->addAction(actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(actImport);
    fileMenu->addAction(actExport);
    fileMenu->addSeparator();
    fileMenu->addAction(actClose);

    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->addAction(m_undo);
    editMenu->addAction(m_redo);
    editMenu->addSeparator();
    editMenu->addAction(m_cut);
    editMenu->addAction(m_copy);
    editMenu->addAction(m_paste);
    editMenu->addAction(m_delete);
    editMenu->addAction(actDup);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->addAction(m_grid);
    viewMenu->addAction(m_fit);
    viewMenu->addAction(m_testMode);

    auto* actAddGrid = makeAction(QStringLiteral("Add g&rid"), QKeySequence(),
                                  [this]() { m_session->addTopGrid(); });
    auto* actAddSub = makeAction(QStringLiteral("Add s&ubgrid"), QKeySequence(),
                                 [this]() { m_session->addSubGrid(); });
    auto* actAddStyle = makeAction(QStringLiteral("Add named st&yle"), QKeySequence(),
                                   [this]() { m_session->addNamedStyle(); });
    auto* actAddDwell = makeAction(QStringLiteral("Add named d&well"), QKeySequence(),
                                   [this]() { m_session->addNamedDwell(); });
    auto* layoutMenu = menuBar()->addMenu(QStringLiteral("&Layout"));
    layoutMenu->addAction(actAddBtn);
    layoutMenu->addAction(actAddLabel);
    layoutMenu->addAction(actAddToggle);
    layoutMenu->addAction(actAddTab);
    layoutMenu->addAction(actAddSlider);
    layoutMenu->addAction(actAddEdge);
    layoutMenu->addAction(actAddGrid);
    layoutMenu->addAction(actAddSub);
    layoutMenu->addAction(actAddStyle);
    layoutMenu->addAction(actAddDwell);
    layoutMenu->addSeparator();
    auto* addRow = makeAction(QStringLiteral("Add r&ow"), QKeySequence(),
                              [this]() { m_session->addGridRow(); });
    auto* addCol = makeAction(QStringLiteral("Add co&lumn"), QKeySequence(),
                              [this]() { m_session->addGridColumn(); });
    auto* pack = makeAction(QStringLiteral("&Pack grid"), QKeySequence(),
                            [this]() { m_session->packGrid(); });
    auto* eq = makeAction(QStringLiteral("E&qualize widths"), QKeySequence(),
                          [this]() { m_session->equalizeSelectedWidths(); });
    auto* align = makeAction(QStringLiteral("A&lign to row"), QKeySequence(),
                             [this]() { m_session->alignSelectedRow(); });
    auto* raise = makeAction(QStringLiteral("Bring &forward"),
                             QKeySequence(QStringLiteral("Ctrl+]")),
                             [this]() { m_session->raiseSelected(); });
    auto* lower = makeAction(QStringLiteral("Send back&ward"),
                             QKeySequence(QStringLiteral("Ctrl+[")),
                             [this]() { m_session->lowerSelected(); });
    auto* toFree = makeAction(QStringLiteral("Convert to &zone"), QKeySequence(),
                              [this]() { m_session->convertSelectedToFree(); });
    auto* toCell = makeAction(QStringLiteral("Convert to ce&ll"), QKeySequence(),
                              [this]() { m_session->convertSelectedToCell(); });
    layoutMenu->addAction(addRow);
    layoutMenu->addAction(addCol);
    layoutMenu->addAction(pack);
    layoutMenu->addAction(eq);
    layoutMenu->addAction(align);
    layoutMenu->addAction(raise);
    layoutMenu->addAction(lower);
    layoutMenu->addAction(toFree);
    layoutMenu->addAction(toCell);
    layoutMenu->addSeparator();
    layoutMenu->addAction(actDoc);
    layoutMenu->addAction(actGrid);
    layoutMenu->addAction(actDwell);

    auto* tb = addToolBar(QStringLiteral("Main"));
    tb->setMovable(false);
    tb->setIconSize(QSize(16, 16));
    tb->addAction(actNew);
    tb->addAction(actOpen);
    tb->addAction(m_save);
    tb->addSeparator();
    tb->addAction(m_undo);
    tb->addAction(m_redo);
    tb->addSeparator();
    tb->addAction(actTest);
    tb->addAction(m_testMode);
    tb->addSeparator();
    tb->addAction(m_fit);
    tb->addAction(m_grid);
    m_layerCombo = new QComboBox;
    m_layerCombo->setMinimumWidth(110);
    tb->addWidget(m_layerCombo);
    connect(m_layerCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0) {
            m_session->setLayer(i);
        }
    });

    auto* split = new QSplitter(Qt::Horizontal, this);
    m_toolbox = new LayoutEditorToolbox(split);
    m_canvas = new LayoutEditorCanvas(*m_session, split);
    m_props = new LayoutEditorProperties(*m_session, split);
    m_toolbox->setObjectName(QStringLiteral("editorToolbox"));
    m_props->setObjectName(QStringLiteral("editorProperties"));
    m_toolbox->bindSession(*m_session);
    m_toolbox->setMinimumWidth(240);
    m_props->setMinimumWidth(360);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    split->setSizes({250, 820, 330});
    setCentralWidget(split);

    auto* elSec = m_toolbox->addSection(QStringLiteral("Elements"));
    m_toolbox->addAction(elSec, actAddBtn);
    m_toolbox->addAction(elSec, actAddLabel);
    m_toolbox->addAction(elSec, actAddToggle);
    m_toolbox->addAction(elSec, actAddTab);
    m_toolbox->addAction(elSec, actAddSlider);
    m_toolbox->addAction(elSec, actAddEdge);
    m_toolbox->addAction(elSec, actDup);
    auto* stSec = m_toolbox->addSection(QStringLiteral("Structure"));
    m_toolbox->addAction(stSec, actAddGrid);
    m_toolbox->addAction(stSec, actAddSub);
    m_toolbox->addAction(stSec, actAddStyle);
    m_toolbox->addAction(stSec, actAddDwell);

    auto* actEsc = makeAction(QStringLiteral("Cancel place"), QKeySequence(Qt::Key_Escape),
                              [this]() { m_session->setPlaceKind(std::nullopt); });
    actEsc->setShortcutContext(Qt::WindowShortcut);

    connect(m_grid, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setShowGrid);
    connect(m_fit, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setFitBoard);
    connect(m_testMode, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setTestMode);
    connect(m_session, &LayoutEditorSession::layerChanged, this, &LayoutEditorWindow::refreshLayers);
    refreshLayers();

    auto* ready = new QLabel(QStringLiteral("Ready"));
    statusBar()->addWidget(ready);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void LayoutEditorWindow::applyFluentTheme()
{
    setStyleSheet(fluentStyleSheet(m_theme));
}

void LayoutEditorWindow::updateTitle()
{
    QString name = m_session->document().name;
    if (name.isEmpty()) {
        name = m_session->document().id;
    }
    QString pathPart;
    if (!m_session->filePath().isEmpty()) {
        pathPart = QFileInfo(m_session->filePath()).fileName();
    } else {
        pathPart = QStringLiteral("unsaved");
    }
    const QString dirty = m_session->isDirty() ? QStringLiteral("*") : QString();
    QString origin;
    if (!m_session->filePath().isEmpty()) {
        origin = isShippedPath(m_session->filePath()) ? QStringLiteral(" shipped")
                                                     : QStringLiteral(" user");
    }
    setWindowTitle(
        QStringLiteral("Gazer Page Editor — %1 [%2]%3%4").arg(name, pathPart, dirty, origin));

    const PageGrid* g = m_session->selectedGrid();
    const EditorSelection sel = m_session->selection();
    QString selText;
    if (sel.target == EditorTarget::Item && !sel.itemIds.isEmpty()) {
        selText = sel.itemIds.size() == 1
                      ? QStringLiteral("    %1").arg(sel.itemId)
                      : QStringLiteral("    %1 cells/zones").arg(sel.itemIds.size());
    } else if (sel.target == EditorTarget::Grid && !sel.itemId.isEmpty()) {
        selText = QStringLiteral("    grid %1").arg(sel.itemId);
    } else if (sel.target == EditorTarget::Style) {
        selText = QStringLiteral("    style %1").arg(sel.itemId);
    } else if (sel.target == EditorTarget::Dwell) {
        selText = QStringLiteral("    dwell %1").arg(sel.itemId);
    }
    const QStringList issues = m_session->validate(m_catalogIds);
    const QString warn =
        issues.isEmpty() ? QString()
                         : QStringLiteral("    %1 issue%2")
                               .arg(issues.size())
                               .arg(issues.size() == 1 ? QString() : QStringLiteral("s"));
    const int cols = g ? g->columns : 0;
    const int rows = g ? g->rows : 0;
    statusBar()->showMessage(QStringLiteral("Grid %1×%2    Ids %3%4%5")
                                 .arg(cols)
                                 .arg(rows)
                                 .arg(PageEdit::allIds(m_session->document()).size())
                                 .arg(selText, warn));
}

void LayoutEditorWindow::updateActions()
{
    const EditorTarget target = m_session->selection().target;
    const bool item = target == EditorTarget::Item;
    const bool deletable = item || target == EditorTarget::Grid || target == EditorTarget::Style
                           || target == EditorTarget::Dwell;
    m_undo->setEnabled(m_session->undoStack().canUndo());
    m_redo->setEnabled(m_session->undoStack().canRedo());
    m_cut->setEnabled(item);
    m_copy->setEnabled(item);
    m_paste->setEnabled(m_session->hasClipboard());
    m_delete->setEnabled(deletable);
    m_save->setEnabled(m_session->isDirty() || m_session->filePath().isEmpty());
}

void LayoutEditorWindow::refreshLayers()
{
    if (!m_layerCombo) {
        return;
    }
    const QSignalBlocker block(m_layerCombo);
    m_layerCombo->clear();
    const QVector<EditorLayer>& layers = m_session->layers();
    for (const EditorLayer& layer : layers) {
        m_layerCombo->addItem(layer.name);
    }
    m_layerCombo->setCurrentIndex(m_session->layerIndex());
    m_layerCombo->setVisible(layers.size() > 1);
}

} // namespace gazer
