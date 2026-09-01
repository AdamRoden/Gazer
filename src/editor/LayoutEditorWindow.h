#pragma once

#include "editor/LayoutEditorIcons.h"
#include "editor/LayoutEditorSession.h"
#include "layout/PageTypes.h"
#include "ui/Theme.h"

#include <QHash>
#include <QMainWindow>
#include <QStringList>
#include <functional>

class QAction;
class QCloseEvent;
class QKeySequence;
class QLabel;
class QPushButton;
class QStackedWidget;

namespace gazer {

class LayoutEditorCanvas;
class LayoutEditorToolbox;
class LayoutEditorProperties;
class LayoutEditorCodeView;

/// Fluent three-pane layout designer (elements tree · canvas · properties).
class LayoutEditorWindow final : public QMainWindow {
    Q_OBJECT

public:
    using TestHandler = std::function<bool(const PageDocument& doc, QString* error)>;

    explicit LayoutEditorWindow(QWidget* parent = nullptr);
    ~LayoutEditorWindow() override;

    void setLayoutsDirectory(const QString& dir);
    void setUserLayoutsDirectory(const QString& dir);
    void setCatalog(const QStringList& ids, const QStringList& labels);
    void setCommandNames(const QStringList& names);
    void setTheme(const ThemeColors& theme);
    void setTestHandler(TestHandler handler);
    [[nodiscard]] bool openFile(const QString& path, QString* error = nullptr);
    /// Open shipped or user `id.xml` (user copy wins).
    [[nodiscard]] bool openLayoutId(const QString& layoutId, QString* error = nullptr);
    void showAndRaise();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void applyFluentTheme();
    void refreshChrome();
    void updateTitle();
    void updateStatus();
    void updateActions();
    void syncActionCatalog();
    void updateZoomLabel();
    void refreshCodeView();
    void setCodeView(bool on);
    [[nodiscard]] bool applyCodeView();
    void frameLoadedPage();
    void showIssues();
    void selectIssue(const EditorIssue& issue);
    [[nodiscard]] bool maybeSave();
    QAction* makeAction(const QString& text, const QKeySequence& shortcut,
                        const std::function<void()>& slot);
    void bindGlyph(QAction* action, EditorGlyph glyph);

    [[nodiscard]] bool promptNewPage();
    [[nodiscard]] bool promptOpenCatalog();
    void newFile();
    void open();
    void save();
    void saveAs();
    void closeDoc();
    void importFile();
    void exportFile();
    void testLive();

    [[nodiscard]] QString defaultDir() const;
    [[nodiscard]] QString xmlFilter() const;
    [[nodiscard]] bool isShippedPath(const QString& path) const;
    [[nodiscard]] QString userCopyPath(const QString& path) const;
    [[nodiscard]] bool confirmIssues(const QString& action);

    LayoutEditorSession* m_session = nullptr;
    LayoutEditorCanvas* m_canvas = nullptr;
    LayoutEditorToolbox* m_toolbox = nullptr;
    LayoutEditorProperties* m_props = nullptr;
    QStackedWidget* m_center = nullptr;
    LayoutEditorCodeView* m_code = nullptr;
    QLabel* m_zoomLabel = nullptr;
    QString m_layoutsDir;
    QString m_userDir;
    QStringList m_catalogIds;
    QStringList m_catalogLabels;
    QStringList m_commandNames;
    TestHandler m_test;
    ThemeColors m_theme = ThemeColors::darkPreset();

    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_cut = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_paste = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_save = nullptr;
    QAction* m_fit = nullptr;
    QAction* m_fitScreen = nullptr;
    QAction* m_zoomIn = nullptr;
    QAction* m_zoomOut = nullptr;
    QAction* m_codeView = nullptr;
    QAction* m_grid = nullptr;
    QAction* m_testMode = nullptr;
    QHash<QAction*, EditorGlyph> m_glyphs;

    QLabel* m_chipGrid = nullptr;
    QLabel* m_chipIds = nullptr;
    QLabel* m_chipSel = nullptr;
    QPushButton* m_chipIssues = nullptr;
};

} // namespace gazer
