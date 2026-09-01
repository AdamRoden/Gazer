#pragma once

#include "editor/LayoutEditorSession.h"

#include <QString>

namespace gazer {

[[nodiscard]] PageDocument makeBlankDocument();
[[nodiscard]] PageDocument makeTemplateDocument(EditorTemplate tmpl, const QString& id,
                                                const QString& name);

[[nodiscard]] PageDocument makeKeyboardPage(const QString& id, const QString& name);

} // namespace gazer
