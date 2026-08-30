#pragma once

class QPoint;
class QWidget;

namespace gazer {

class LayoutEditorSession;

struct EditorItemMenuOpts {
    bool subgrid = false;
    bool zOrder = false;
};

void execEditorItemMenu(QWidget* parent, const QPoint& globalPos, LayoutEditorSession& session,
                        EditorItemMenuOpts opts = {});

} // namespace gazer
