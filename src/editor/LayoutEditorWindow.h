#pragma once

#include "layout/LayoutTypes.h"
#include "ui/Theme.h"

#include <QMainWindow>
#include <QStringList>
#include <functional>

class QAction;
class QCloseEvent;
class QComboBox;
class QKeySequence;

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
    void setUserLayoutsDirectory(const QString& dir);
    void setCatalog(const QStringList& ids, const QStringList& labels);
    void setCommandNames(const QStringList& names);
    void setTheme(const ThemeColors& theme);
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
    void refreshLayers();
    void syncActionCatalog();
    [[nodiscard]] bool maybeSave();
    QAction* makeAction(const QString& text, const QKeySequence& shortcut,
                        const std::function<void()>& slot);

    [[nodiscard]] bool promptNewBoard();
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
    [[nodiscard]] QString jsonFilter() const;

    LayoutEditorSession* m_session = nullptr;
    LayoutEditorCanvas* m_canvas = nullptr;
    LayoutEditorToolbox* m_toolbox = nullptr;
    LayoutEditorProperties* m_props = nullptr;
    QComboBox* m_layerCombo = nullptr;
    QString m_layoutsDir;
    QString m_userDir;
    QStringList m_catalogIds;
    QStringList m_catalogLabels;
    QStringList m_commandNames;
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
    QAction* m_testMode = nullptr;
};

} // namespace gazer
