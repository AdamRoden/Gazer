#pragma once

#include "layout/LayoutTypes.h"

#include <QMainWindow>
#include <functional>

class QAction;
class QKeySequence;

class QCloseEvent;

namespace gazer {

class LayoutEditorSession;
class LayoutEditorCanvas;
class LayoutEditorToolbox;
class LayoutEditorProperties;

/// Fluent three-pane layout designer (toolbox · monitor canvas · properties).
class LayoutEditorWindow final : public QMainWindow {
    Q_OBJECT

public:
    using TestHandler = std::function<bool(const LayoutDocument& doc, QString* error)>;

    explicit LayoutEditorWindow(QWidget* parent = nullptr);
    ~LayoutEditorWindow() override;

    void setLayoutsDirectory(const QString& dir);
    void setTestHandler(TestHandler handler);
    [[nodiscard]] bool openFile(const QString& path, QString* error = nullptr);
    void showAndRaise();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void applyFluentTheme();
    void updateTitle();
    void updateActions();
    [[nodiscard]] bool maybeSave();
    QAction* makeAction(const QString& text, const QKeySequence& shortcut,
                        const std::function<void()>& slot);

    void newFile();
    void open();
    void save();
    void saveAs();
    void closeDoc();
    void importFile();
    void exportFile();
    void testLive();

    [[nodiscard]] QString defaultDir() const;
    [[nodiscard]] QString jsonFilter() const;

    LayoutEditorSession* m_session = nullptr;
    LayoutEditorCanvas* m_canvas = nullptr;
    LayoutEditorToolbox* m_toolbox = nullptr;
    LayoutEditorProperties* m_props = nullptr;
    QString m_layoutsDir;
    TestHandler m_test;

    QAction* m_undo = nullptr;
    QAction* m_redo = nullptr;
    QAction* m_cut = nullptr;
    QAction* m_copy = nullptr;
    QAction* m_paste = nullptr;
    QAction* m_delete = nullptr;
    QAction* m_save = nullptr;
    QAction* m_fit = nullptr;
    QAction* m_grid = nullptr;
};

} // namespace gazer
