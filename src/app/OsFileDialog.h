#pragma once

#include <QString>
#include <QVector>

#include <functional>

namespace gazer {

class PageSession;

/// Catalog id of the keyboard shown with a native open or save dialog.
inline constexpr char kOsFilePageId[] = "qwerty_file";

struct OsFileType {
    QString label;
    /// Pattern such as `*.json`.
    QString spec;
};

struct OsFileRequest {
    enum class Kind { Open, Save };
    Kind kind = Kind::Open;
    QString title;
    QString directory;
    QString fileName;
    /// Extension without a dot. The save dialog appends it when the name has none.
    QString defaultExtension;
    QVector<OsFileType> types;
};

/// Called on the gaze thread when the dialog closes. Empty path and empty error
/// is Cancel.
using OsFileDialogDone = std::function<void(const QString& path, const QString& error)>;

/// Show @c qwerty_file with a native Windows file dialog filling the top-left
/// work-area rectangle beside the painted boards. Other boards stay in the session
/// but are not painted. The root keeps its current layers. Returns immediately.
/// The dialog's modal loop runs on its own thread and does not move Gazer
/// windows. @p done runs on the gaze thread after the saved pages are restored.
void runOsFileDialog(PageSession& pages, const OsFileRequest& request, OsFileDialogDone done);

} // namespace gazer
