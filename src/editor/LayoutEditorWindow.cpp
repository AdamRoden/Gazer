#include "editor/LayoutEditorWindow.h"

#include "editor/LayoutEditorCanvas.h"
#include "editor/LayoutEditorCodeView.h"
#include "editor/LayoutEditorFields.h"
#include "editor/LayoutEditorProperties.h"
#include "editor/LayoutEditorSession.h"
#include "editor/LayoutEditorStyle.h"
#include "editor/LayoutEditorToolbox.h"
#include "layout/PageEdit.h"
#include "ui/AppIcon.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace gazer {

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
    refreshChrome();
    updateActions();

    connect(m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        refreshChrome();
        updateActions();
    });
    connect(m_session, &LayoutEditorSession::selectionChanged, this, [this]() {
        refreshChrome();
        updateActions();
    });
    connect(m_session, &LayoutEditorSession::dirtyChanged, this, [this](bool) { refreshChrome(); });
    connect(m_session, &LayoutEditorSession::filePathChanged, this, [this](const QString&) {
        refreshChrome();
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
    m_fit = makeAction(QStringLiteral("&Fit grid"), QKeySequence(QStringLiteral("Ctrl+0")),
                       [this]() { m_canvas->fitGrid(); });
    m_fitScreen = makeAction(QStringLiteral("Fit &screen"), QKeySequence(QStringLiteral("Ctrl+1")),
                             [this]() { m_canvas->fitScreen(); });
    m_zoomIn = makeAction(QStringLiteral("Zoom &in"), QKeySequence::ZoomIn,
                          [this]() { m_canvas->zoomBy(1.15); });
    m_zoomIn->setShortcuts({QKeySequence::ZoomIn, QKeySequence(QStringLiteral("Ctrl+="))});
    m_zoomOut = makeAction(QStringLiteral("Zoom &out"), QKeySequence::ZoomOut,
                           [this]() { m_canvas->zoomBy(1.0 / 1.15); });

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
    viewMenu->addAction(m_fitScreen);
    viewMenu->addAction(m_zoomIn);
    viewMenu->addAction(m_zoomOut);
    viewMenu->addSeparator();
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

    bindGlyph(actNew, EditorGlyph::FileNew);
    bindGlyph(actOpen, EditorGlyph::FileOpen);
    bindGlyph(m_save, EditorGlyph::FileSave);
    bindGlyph(m_undo, EditorGlyph::Undo);
    bindGlyph(m_redo, EditorGlyph::Redo);
    bindGlyph(m_cut, EditorGlyph::Cut);
    bindGlyph(m_copy, EditorGlyph::Copy);
    bindGlyph(m_paste, EditorGlyph::Paste);
    bindGlyph(m_delete, EditorGlyph::Delete);
    bindGlyph(actTest, EditorGlyph::TestLive);
    bindGlyph(m_testMode, EditorGlyph::TestCanvas);
    bindGlyph(m_fit, EditorGlyph::Fit);
    bindGlyph(m_fitScreen, EditorGlyph::FitScreen);
    bindGlyph(m_zoomIn, EditorGlyph::ZoomIn);
    bindGlyph(m_zoomOut, EditorGlyph::ZoomOut);
    bindGlyph(m_grid, EditorGlyph::Grid);
    bindGlyph(actAddBtn, EditorGlyph::Button);
    bindGlyph(actAddLabel, EditorGlyph::Label);
    bindGlyph(actAddToggle, EditorGlyph::Toggle);
    bindGlyph(actAddTab, EditorGlyph::Tab);
    bindGlyph(actAddSlider, EditorGlyph::Slider);
    bindGlyph(actAddEdge, EditorGlyph::Zone);
    bindGlyph(actDup, EditorGlyph::Duplicate);
    bindGlyph(actAddGrid, EditorGlyph::GridAdd);
    bindGlyph(actAddSub, EditorGlyph::SubGrid);
    bindGlyph(actAddStyle, EditorGlyph::Style);
    bindGlyph(actAddDwell, EditorGlyph::Dwell);

    auto* tb = addToolBar(QStringLiteral("Main"));
    tb->setMovable(false);
    tb->setIconSize(QSize(18, 18));
    tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
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
    tb->addAction(m_fitScreen);
    tb->addAction(m_zoomOut);
    tb->addAction(m_zoomIn);
    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setObjectName(QStringLiteral("statusChip"));
    m_zoomLabel->setMinimumWidth(48);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setToolTip(QStringLiteral("Canvas zoom"));
    tb->addWidget(m_zoomLabel);
    tb->addAction(m_grid);
    m_codeView = makeAction(QStringLiteral("&Code view"), QKeySequence(QStringLiteral("Ctrl+E")),
                            []() {});
    m_codeView->setCheckable(true);
    bindGlyph(m_codeView, EditorGlyph::Code);
    tb->addSeparator();
    tb->addAction(m_codeView);
    viewMenu->addAction(m_codeView);
    m_layerCombo = new QComboBox;
    m_layerCombo->setMinimumWidth(110);
    m_layerCombo->setToolTip(QStringLiteral("Keyboard layer"));
    tb->addWidget(m_layerCombo);
    connect(m_layerCombo, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (i >= 0) {
            m_session->setLayer(i);
        }
    });

    auto* split = new QSplitter(Qt::Horizontal, this);
    m_toolbox = new LayoutEditorToolbox(split);
    m_center = new QStackedWidget(split);
    m_canvas = new LayoutEditorCanvas(*m_session, m_center);
    m_code = new LayoutEditorCodeView(m_center);
    m_center->addWidget(m_canvas);
    m_center->addWidget(m_code);
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

    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddBtn, EditorGlyph::Button,
                              EditorItemKind::Button, QStringLiteral("Button"));
    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddLabel, EditorGlyph::Label,
                              EditorItemKind::Label, QStringLiteral("Label"));
    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddToggle, EditorGlyph::Toggle,
                              EditorItemKind::Toggle, QStringLiteral("Toggle"));
    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddTab, EditorGlyph::Tab,
                              EditorItemKind::Tab, QStringLiteral("Tab"));
    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddSlider, EditorGlyph::Slider,
                              EditorItemKind::Slider, QStringLiteral("Slider"));
    m_toolbox->addPlaceAction(QStringLiteral("Items"), actAddEdge, EditorGlyph::Zone,
                              EditorItemKind::Zone, QStringLiteral("Zone"));
    m_toolbox->addPaletteAction(QStringLiteral("Items"), actDup, EditorGlyph::Duplicate,
                                QStringLiteral("Duplicate"));
    m_toolbox->addPaletteAction(QStringLiteral("Structure"), actAddGrid, EditorGlyph::GridAdd,
                                QStringLiteral("Grid"));
    m_toolbox->addPaletteAction(QStringLiteral("Structure"), actAddSub, EditorGlyph::SubGrid,
                                QStringLiteral("Subgrid"));
    m_toolbox->addPaletteAction(QStringLiteral("Structure"), actAddStyle, EditorGlyph::Style,
                                QStringLiteral("Style"));
    m_toolbox->addPaletteAction(QStringLiteral("Structure"), actAddDwell, EditorGlyph::Dwell,
                                QStringLiteral("Dwell"));

    auto* actEsc = makeAction(QStringLiteral("Cancel place"), QKeySequence(Qt::Key_Escape),
                              [this]() { m_session->setPlaceKind(std::nullopt); });
    actEsc->setShortcutContext(Qt::WindowShortcut);

    connect(m_grid, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setShowGrid);
    connect(m_testMode, &QAction::toggled, m_canvas, &LayoutEditorCanvas::setTestMode);
    connect(m_canvas, &LayoutEditorCanvas::zoomChanged, this,
            &LayoutEditorWindow::updateZoomLabel);
    connect(m_codeView, &QAction::toggled, this, &LayoutEditorWindow::setCodeView);
    connect(m_session, &LayoutEditorSession::layerChanged, this, [this]() {
        refreshLayers();
        if (m_codeView && m_codeView->isChecked()) {
            refreshCodeView();
        }
    });
    connect(m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        if (m_codeView && m_codeView->isChecked() && m_code && !m_code->isDirty()) {
            refreshCodeView();
        }
    });
    refreshLayers();
    updateZoomLabel();

    auto makeChip = [](const QString& objectName) {
        auto* chip = new QLabel;
        chip->setObjectName(objectName);
        chip->setAlignment(Qt::AlignCenter);
        return chip;
    };
    m_chipGrid = makeChip(QStringLiteral("statusChip"));
    m_chipIds = makeChip(QStringLiteral("statusChip"));
    m_chipSel = makeChip(QStringLiteral("statusChip"));
    m_chipIssues = new QPushButton;
    m_chipIssues->setObjectName(QStringLiteral("statusChipDanger"));
    m_chipIssues->setCursor(Qt::PointingHandCursor);
    m_chipIssues->setFocusPolicy(Qt::NoFocus);
    m_chipIssues->setFlat(true);
    m_chipIssues->hide();
    m_chipSel->hide();
    connect(m_chipIssues, &QPushButton::clicked, this, &LayoutEditorWindow::showIssues);
    statusBar()->addPermanentWidget(m_chipGrid);
    statusBar()->addPermanentWidget(m_chipIds);
    statusBar()->addPermanentWidget(m_chipSel);
    statusBar()->addPermanentWidget(m_chipIssues);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void LayoutEditorWindow::bindGlyph(QAction* action, EditorGlyph glyph)
{
    m_glyphs.insert(action, glyph);
}

void LayoutEditorWindow::applyFluentTheme()
{
    setStyleSheet(editorStyleSheet(m_theme));
    for (auto it = m_glyphs.cbegin(); it != m_glyphs.cend(); ++it) {
        it.key()->setIcon(editorGlyphIcon(it.value(), m_theme, 18));
    }
    if (m_toolbox) {
        m_toolbox->setTheme(m_theme);
    }
    if (m_canvas) {
        m_canvas->setTheme(m_theme);
    }
    if (m_code) {
        m_code->setTheme(m_theme);
    }
}

void LayoutEditorWindow::refreshChrome()
{
    updateTitle();
    updateStatus();
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
}

void LayoutEditorWindow::updateStatus()
{
    const QString origin = m_session->filePath().isEmpty()
                               ? QString()
                               : (isShippedPath(m_session->filePath()) ? QStringLiteral("shipped")
                                                                      : QStringLiteral("user"));
    const PageGrid* g = m_session->selectedGrid();
    const EditorSelection sel = m_session->selection();
    QString selText;
    if (sel.target == EditorTarget::Item && !sel.itemIds.isEmpty()) {
        selText = sel.itemIds.size() == 1
                      ? sel.itemId
                      : QStringLiteral("%1 selected").arg(sel.itemIds.size());
    } else if (sel.target == EditorTarget::Grid && !sel.itemId.isEmpty()) {
        selText = QStringLiteral("grid %1").arg(sel.itemId);
    } else if (sel.target == EditorTarget::Style) {
        selText = QStringLiteral("style %1").arg(sel.itemId);
    } else if (sel.target == EditorTarget::Dwell) {
        selText = QStringLiteral("dwell %1").arg(sel.itemId);
    }
    const QVector<EditorIssue> issues = m_session->validate(m_catalogIds);
    const int cols = g ? g->columns : 0;
    const int rows = g ? g->rows : 0;
    const int ids = PageEdit::allIds(m_session->document()).size();
    if (m_chipGrid) {
        m_chipGrid->setText(QStringLiteral("Grid %1×%2").arg(cols).arg(rows));
    }
    if (m_chipIds) {
        m_chipIds->setText(QStringLiteral("%1 id%2").arg(ids).arg(ids == 1 ? QString()
                                                                          : QStringLiteral("s")));
    }
    if (m_chipSel) {
        m_chipSel->setVisible(!selText.isEmpty());
        m_chipSel->setText(selText);
    }
    if (m_chipIssues) {
        m_chipIssues->setVisible(!issues.isEmpty());
        if (!issues.isEmpty()) {
            m_chipIssues->setText(QStringLiteral("%1 issue%2")
                                      .arg(issues.size())
                                      .arg(issues.size() == 1 ? QString() : QStringLiteral("s")));
            QStringList tips;
            for (const EditorIssue& issue : issues) {
                tips.push_back(issue.message);
            }
            m_chipIssues->setToolTip(tips.join(QLatin1Char('\n')));
        }
    }
    statusBar()->showMessage(origin.isEmpty() ? QStringLiteral("Ready") : origin);
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

void LayoutEditorWindow::updateZoomLabel()
{
    if (!m_zoomLabel || !m_canvas) {
        return;
    }
    const int pct = qMax(1, qRound(m_canvas->zoom() * 100.0));
    m_zoomLabel->setText(QStringLiteral("%1%").arg(pct));
}

void LayoutEditorWindow::refreshCodeView()
{
    if (m_code && m_session) {
        m_code->loadDocument(m_session->document());
    }
}

void LayoutEditorWindow::setCodeView(bool on)
{
    if (!m_center || !m_codeView) {
        return;
    }
    if (!on) {
        if (!applyCodeView()) {
            const QSignalBlocker block(m_codeView);
            m_codeView->setChecked(true);
            return;
        }
        m_center->setCurrentWidget(m_canvas);
        if (m_canvas) {
            m_canvas->setFocus(Qt::OtherFocusReason);
        }
        return;
    }
    refreshCodeView();
    m_center->setCurrentWidget(m_code);
    m_code->setFocus(Qt::OtherFocusReason);
}

bool LayoutEditorWindow::applyCodeView()
{
    if (!m_code || !m_codeView || !m_codeView->isChecked()) {
        return true;
    }
    return m_code->applyTo(*m_session, this);
}

void LayoutEditorWindow::showIssues()
{
    const QVector<EditorIssue> issues = m_session->validate(m_catalogIds);
    if (issues.isEmpty()) {
        return;
    }
    auto* pop = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint);
    pop->setAttribute(Qt::WA_DeleteOnClose);
    pop->setObjectName(QStringLiteral("editorCard"));
    auto* lay = new QVBoxLayout(pop);
    lay->setContentsMargins(8, 8, 8, 8);
    auto* title = new QLabel(QStringLiteral("Issues"));
    title->setObjectName(QStringLiteral("panelTitle"));
    lay->addWidget(title);
    auto* list = new QListWidget;
    list->setFocusPolicy(Qt::NoFocus);
    for (const EditorIssue& issue : issues) {
        auto* item = new QListWidgetItem(issue.message);
        item->setData(Qt::UserRole, issue.itemId);
        item->setData(Qt::UserRole + 1, int(issue.target));
        list->addItem(item);
    }
    lay->addWidget(list);
    connect(list, &QListWidget::itemClicked, this, [this, pop](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        EditorIssue issue;
        issue.message = item->text();
        issue.itemId = item->data(Qt::UserRole).toString();
        issue.target = EditorTarget(item->data(Qt::UserRole + 1).toInt());
        selectIssue(issue);
        pop->close();
    });
    const int h = qBound(120, issues.size() * 28 + 52, 320);
    pop->resize(420, h);
    QPoint pos = m_chipIssues->mapToGlobal(QPoint(0, 0));
    pos.setY(pos.y() - h - 6);
    pos.setX(qMax(8, pos.x() + m_chipIssues->width() - 420));
    pop->move(pos);
    pop->show();
}

void LayoutEditorWindow::selectIssue(const EditorIssue& issue)
{
    if (issue.target == EditorTarget::Item && !issue.itemId.isEmpty()) {
        m_session->selectItem(issue.itemId);
        return;
    }
    if (issue.target == EditorTarget::Grid && !issue.itemId.isEmpty()) {
        m_session->selectGrid(issue.itemId);
        return;
    }
    m_session->selectTarget(EditorTarget::Document);
}

void LayoutEditorWindow::frameLoadedPage()
{
    if (m_canvas) {
        m_canvas->fitGrid();
        updateZoomLabel();
    }
    if (m_codeView && m_codeView->isChecked()) {
        refreshCodeView();
    }
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
