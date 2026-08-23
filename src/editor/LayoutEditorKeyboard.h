#pragma once

#include "editor/LayoutEditorSession.h"

#include <QString>
#include <QVector>

namespace gazer {

[[nodiscard]] PageDocument makeBlankDocument();
[[nodiscard]] QVector<EditorLayer> makeBlankLayers();
[[nodiscard]] QVector<EditorLayer> makeTemplateLayers(EditorTemplate tmpl, const QString& id,
                                                      const QString& name);

[[nodiscard]] QVector<PageDocument> makeKeyboardFamily(const QString& id, const QString& name);

} // namespace gazer
