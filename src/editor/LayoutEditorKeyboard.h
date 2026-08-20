#pragma once

#include "editor/LayoutEditorSession.h"

#include <QString>
#include <QVector>

namespace gazer {

[[nodiscard]] LayoutDocument makeBlankDocument();
[[nodiscard]] QVector<EditorLayer> makeBlankLayers();
[[nodiscard]] QVector<EditorLayer> makeTemplateLayers(EditorTemplate tmpl, const QString& id,
                                                      const QString& name);

/// Three related boards: base, shift, and symbols. Shift/sym `loadLayout` back to @p id.
[[nodiscard]] QVector<LayoutDocument> makeKeyboardFamily(const QString& id, const QString& name);

} // namespace gazer
