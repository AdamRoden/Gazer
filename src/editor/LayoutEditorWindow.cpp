#include "editor/LayoutEditorWindow.h"

#include "editor/LayoutEditorCanvas.h"
#include "editor/LayoutEditorProperties.h"
#include "editor/LayoutEditorSession.h"
#include "editor/LayoutEditorToolbox.h"
#include "ui/AppIcon.h"

#include <QAction>
#include <QCloseEvent>
#include <QKeySequence>
#include <QMenu>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QGuiApplication>
#include <QScreen>

namespace gazer {

namespace {

QString fluentStyleSheet()
{
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background-color: #202020;
            color: #ffffff;
            font-family: "Segoe UI";
            font-size: 13px;
        }
        QMenuBar {
            background: #202020;
            color: #ffffff;
            padding: 2px 6px;
        }
        QMenuBar::item:selected { background: #2d2d2d; }
        QMenu {
            background: #2c2c2c;
            color: #ffffff;
            border: 1px solid #3f3f3f;
        }
        QMenu::item:selected { background: #3d3d3d; }
        QToolBar {
            background: #2b2b2b;
            border: none;
            padding: 4px 8px;
            spacing: 4px;
        }
        QToolBar QToolButton {
            background: transparent;
            color: #ffffff;
            padding: 6px 10px;
            border-radius: 4px;
        }
        QToolBar QToolButton:hover { background: #3a3a3a; }
        QToolBar QToolButton:checked { background: #1f4d6e; }
        QStatusBar {
            background: #1c1c1c;
            color: #c5c5c5;
        }
        QSplitter::handle { background: #2a2a2a; width: 1px; }
        QTreeWidget, QTabWidget::pane, QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit {
            background: #2c2c2c;
            color: #ffffff;
            border: 1px solid #3f3f3f;
            border-radius: 4px;
            selection-background-color: #1f4d6e;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            padding: 4px 8px;
            min-height: 24px;
        }
        QComboBox QAbstractItemView {
            background: #2c2c2c;
            color: #ffffff;
            selection-background-color: #1f4d6e;
        }
        QTabBar::tab {
            background: transparent;
            color: #c5c5c5;
            padding: 8px 14px;
            border: none;
        }
        QTabBar::tab:selected {
            color: #ffffff;
            border-bottom: 2px solid #60cdff;
        }
        QCheckBox { color: #ffffff; spacing: 8px; }
        QHeaderView::section { background: #2c2c2c; color: #c5c5c5; border: none; }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #202020;
            width: 10px;
            height: 10px;
        }
        QScrollBar::handle { background: #4a4a4a; border-radius: 4px; }
        QLabel#panelTitle {
            font-size: 16px;
            font-weight: 600;
            color: #ffffff;
            padding: 4px 2px 8px 2px;
        }
        QLabel#fieldHeading {
            font-size: 12px;
            font-weight: 600;
            color: #9aa0a6;
            padding-top: 8px;
        }
        QLabel#fieldNote { color: #9aa0a6; }
        QPushButton {
            background: #3c3c3c;
            color: #ffffff;
            border: 1px solid #4a4a4a;
            border-radius: 4px;
            padding: 4px 10px;
        }
        QPushButton:hover { background: #4a4a4a; }
        QTreeWidget::item { padding: 4px 2px; }
        QTreeWidget::item:selected { background: #1f4d6e; }
        QTreeWidget::item:hover { background: #333333; }
    )");
}

} // namespace

LayoutEditorWindow::LayoutEditorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Gazer Layout Editor"));
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
    connect(m_session, &LayoutEditorSession::selectionChanged, this,
            &LayoutEditorWindow::updateActions);
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

void LayoutEditorWindow::setTestHandler(TestHandler handler)
{
    m_test = std::move(handler);
}

bool LayoutEditorWindow::openFile(const QString& path, QString* error)
{
    if (!maybeSave()) {
        return false;
    }
    return m_session->loadFromFile(path, error);
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
        this, QStringLiteral("Unsaved layout"),
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
    m_fit = makeAction(QStringLiteral("&Fit board"), QKeySequence(QStringLiteral("Ctrl+0")), []() {});
    m_fit->setCheckable(true);

    auto* actAddBtn = makeAction(QStringLiteral("Add &button"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Button);
    });
    auto* actAddLabel = makeAction(QStringLiteral("Add &label"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Label);
    });
    auto* actAddToggle = makeAction(QStringLiteral("Add &toggle"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Toggle);
    });
    auto* actAddTab = makeAction(QStringLiteral("Add t&ab"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Tab);
    });
    auto* actAddSlider = makeAction(QStringLiteral("Add &slider"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Slider);
    });
    auto* actAddEdge = makeAction(QStringLiteral("Add &unbounded item"), QKeySequence(), [this]() {
        m_session->addItem(EditorItemKind::Unbounded);
    });
    auto* actDup = makeAction(QStringLiteral("D&uplicate selected"),
                              QKeySequence(QStringLiteral("Ctrl+D")),
                              [this]() { m_session->duplicateSelected(); });

    auto* actDoc = makeAction(QStringLiteral("&Document"), QKeySequence(), [this]() {
        m_session->selectTarget(EditorTarget::Document);
        m_props->showLayoutTab();
    });
    auto* actGrid = makeAction(QStringLiteral("Edit &grid"), QKeySequence(), [this]() {
        m_session->selectTarget(EditorTarget::Grid);
        m_props->showLayoutTab();
    });
    auto* actDwell = makeAction(QStringLiteral("Edit d&well"), QKeySequence(), [this]() {
        m_session->selectTarget(EditorTarget::Dwell);
        m_props->showInteractionTab();
    });
    auto* actStyle = makeAction(QStringLiteral("Edit st&yle"), QKeySequence(), [this]() {
        m_session->selectTarget(EditorTarget::Style);
        m_props->showStyleTab();
    });
    auto* actWin = makeAction(QStringLiteral("&Edit window"), QKeySequence(), [this]() {
        m_session->selectTarget(EditorTarget::Window);
        m_props->showLayoutTab();
    });
    auto* actTest =
        makeAction(QStringLiteral("&Test"), QKeySequence(QStringLiteral("F5")), [this]() {
            testLive();
        });

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

    auto* layoutMenu = menuBar()->addMenu(QStringLiteral("&Layout"));
    layoutMenu->addAction(actAddBtn);
    layoutMenu->addAction(actAddLabel);
    layoutMenu->addAction(actAddToggle);
    layoutMenu->addAction(actAddTab);
    layoutMenu->addAction(actAddSlider);
    layoutMenu->addAction(actAddEdge);
    layoutMenu->addSeparator();
    layoutMenu->addAction(actDoc);
    layoutMenu->addAction(actGrid);
    layoutMenu->addAction(actDwell);
    layoutMenu->addAction(actStyle);

    auto* winMenu = menuBar()->addMenu(QStringLiteral("&Window"));
    winMenu->addAction(actWin);

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
    tb->addSeparator();
    tb->addAction(m_fit);
    tb->addAction(m_grid);

    auto* split = new QSplitter(Qt::Horizontal, this);
    m_toolbox = new LayoutEditorToolbox(split);
    m_canvas = new LayoutEditorCanvas(*m_session, split);
    m_props = new LayoutEditorProperties(*m_session, split);
    m_toolbox->setMinimumWidth(220);
    m_props->setMinimumWidth(300);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    split->setSizes({250, 820, 330});
    setCentralWidget(split);

    auto* fileSec = m_toolbox->addSection(QStringLiteral("File"));
    m_toolbox->addAction(fileSec, actNew);
    m_toolbox->addAction(fileSec, actOpen);
    m_toolbox->addAction(fileSec, m_save);
    m_toolbox->addAction(fileSec, actSaveAs);
    m_toolbox->addAction(fileSec, actClose);
    m_toolbox->addAction(fileSec, actImport);
    m_toolbox->addAction(fileSec, actExport);

    auto* editSec = m_toolbox->addSection(QStringLiteral("Edit"));
    m_toolbox->addAction(editSec, m_undo);
    m_toolbox->addAction(editSec, m_redo);
    m_toolbox->addAction(editSec, m_cut);
    m_toolbox->addAction(editSec, m_copy);
    m_toolbox->addAction(editSec, m_paste);
    m_toolbox->addAction(editSec, m_delete);

    auto* winSec = m_toolbox->addSection(QStringLiteral("Window"));
    m_toolbox->addAction(winSec, actWin);

    auto* laySec = m_toolbox->addSection(QStringLiteral("Layout"));
    m_toolbox->addAction(laySec, actDoc);
    m_toolbox->addAction(laySec, actGrid);
    m_toolbox->addAction(laySec, actDwell);
    m_toolbox->addAction(laySec, actStyle);

    auto* elSec = m_toolbox->addSection(QStringLiteral("Elements"));
    m_toolbox->addAction(elSec, actAddBtn);
    m_toolbox->addAction(elSec, actAddLabel);
    m_toolbox->addAction(elSec, actAddToggle);
    m_toolbox->addAction(elSec, actAddTab);
    m_toolbox->addAction(elSec, actAddSlider);
    m_toolbox->addAction(elSec, actAddEdge);
    m_toolbox->addAction(elSec, actDup);

    connect(m_grid, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setShowGrid);
    connect(m_fit, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setFitBoard);

    auto* ready = new QLabel(QStringLiteral("Ready"));
    statusBar()->addWidget(ready);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void LayoutEditorWindow::applyFluentTheme()
{
    setStyleSheet(fluentStyleSheet());
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
    setWindowTitle(QStringLiteral("Gazer Layout Editor — %1 [%2]%3").arg(name, pathPart, dirty));

    const LayoutGrid& g = m_session->document().grid;
    statusBar()->showMessage(QStringLiteral("Ready    Grid: %1×%2    Items: %3")
                                 .arg(g.columns)
                                 .arg(g.rows)
                                 .arg(m_session->document().items.size()));
}

void LayoutEditorWindow::updateActions()
{
    const bool item = m_session->selection().target == EditorTarget::Item;
    m_undo->setEnabled(m_session->undoStack().canUndo());
    m_redo->setEnabled(m_session->undoStack().canRedo());
    m_cut->setEnabled(item);
    m_copy->setEnabled(item);
    m_paste->setEnabled(m_session->hasClipboard());
    m_delete->setEnabled(item);
    m_save->setEnabled(m_session->isDirty() || m_session->filePath().isEmpty());
}

bool LayoutEditorWindow::maybeSave()
{
    if (!m_session->isDirty()) {
        return true;
    }
    const auto r = QMessageBox::question(
        this, QStringLiteral("Unsaved layout"),
        QStringLiteral("Save changes to %1?").arg(m_session->document().id),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) {
        return false;
    }
    if (r == QMessageBox::Discard) {
        return true;
    }
    save();
    return !m_session->isDirty();
}

void LayoutEditorWindow::newFile()
{
    if (!maybeSave()) {
        return;
    }
    m_session->newDocument();
}

void LayoutEditorWindow::open()
{
    if (!maybeSave()) {
        return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Open layout"), defaultDir(), jsonFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->loadFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), err);
    }
}

void LayoutEditorWindow::save()
{
    if (m_session->filePath().isEmpty()) {
        saveAs();
        return;
    }
    QString err;
    if (!m_session->save(&err)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    }
}

void LayoutEditorWindow::saveAs()
{
    const QString suggest = m_session->filePath().isEmpty()
                                ? QDir(defaultDir()).filePath(m_session->document().id + QStringLiteral(".json"))
                                : m_session->filePath();
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Save layout"), suggest, jsonFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->saveTo(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    }
}

void LayoutEditorWindow::closeDoc()
{
    if (!maybeSave()) {
        return;
    }
    m_session->closeDocument();
}

void LayoutEditorWindow::importFile()
{
    if (!maybeSave()) {
        return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Import layout"), defaultDir(), jsonFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->importFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Import failed"), err);
    }
}

void LayoutEditorWindow::exportFile()
{
    const QString suggest =
        QDir(defaultDir()).filePath(m_session->document().id + QStringLiteral(".json"));
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Export layout"), suggest, jsonFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->exportTo(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Export failed"), err);
    }
}

void LayoutEditorWindow::testLive()
{
    if (!m_test) {
        QMessageBox::information(this, QStringLiteral("Test"),
                                 QStringLiteral("Live test is only available while Gazer is running."));
        return;
    }
    QString err;
    if (!m_test(m_session->document(), &err)) {
        QMessageBox::warning(this, QStringLiteral("Test failed"),
                             err.isEmpty() ? QStringLiteral("Could not open layout") : err);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Live board opened for %1").arg(m_session->document().id),
                             4000);
}

QString LayoutEditorWindow::defaultDir() const
{
    if (!m_layoutsDir.isEmpty()) {
        return m_layoutsDir;
    }
    if (!m_session->filePath().isEmpty()) {
        return QFileInfo(m_session->filePath()).absolutePath();
    }
    return QStringLiteral(".");
}

QString LayoutEditorWindow::jsonFilter() const
{
    return QStringLiteral("Layout JSON (*.json);;All files (*.*)");
}

} // namespace gazer
