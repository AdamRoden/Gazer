#include "editor/LayoutEditorProperties.h"

#include "editor/LayoutEditorFields.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"

#include <QFormLayout>
#include <QStringList>
#include <QTimer>

namespace gazer {

void LayoutEditorProperties::fillPage(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageDocument& d = m_session.document();
    b.text(form, QStringLiteral("Id"), d.id, [this](const QString& t) {
        applyDoc([&](PageDocument& doc) { doc.id = t.trimmed(); }, QStringLiteral("Page id"));
    });
    b.text(form, QStringLiteral("Name"), d.name, [this](const QString& t) {
        applyDoc([&](PageDocument& doc) { doc.name = t; }, QStringLiteral("Name"));
    });
    b.check(form, QStringLiteral("Process-lifetime root"), d.master, [this](bool on) {
        applyDoc([&](PageDocument& doc) { doc.master = on; }, QStringLiteral("Master"));
    });
    b.check(form, QStringLiteral("Auto-close when idle"), d.autoClose, [this](bool on) {
        applyDoc([&](PageDocument& doc) { doc.autoClose = on; }, QStringLiteral("Auto close"));
    });
    if (d.autoClose) {
        b.integer(form, QStringLiteral("Idle ms"), d.autoCloseIdleMs, -1, 120000, [this](int v) {
            applyDoc([&](PageDocument& doc) { doc.autoCloseIdleMs = v; }, QStringLiteral("Idle ms"));
        });
    }
}

void LayoutEditorProperties::fillGrid(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageGrid* g = m_session.selectedGrid();
    if (!g) {
        b.note(form, QStringLiteral("This page has no grid. Add a grid from the Layout menu."));
        return;
    }
    b.text(form, QStringLiteral("Id"), g->id, [this](const QString& t) {
        const QString next = t.trimmed();
        const QString cur = m_session.selectedGridId();
        if (next == cur) {
            return;
        }
        if (next.isEmpty() || PageEdit::allIds(m_session.document()).contains(next)) {
            rebuild();
            return;
        }
        applyGrid([&](PageGrid& grid) { grid.id = next; }, QStringLiteral("Grid id"));
        m_session.selectGrid(next);
    });
    QString chrome = QStringLiteral("none");
    if (g->rootSlot == PageRootSlot::Drawer) {
        chrome = QStringLiteral("drawer");
    } else if (g->rootSlot == PageRootSlot::Quit) {
        chrome = QStringLiteral("quit");
    }
    b.comboValues(form, QStringLiteral("Chrome slot"),
                  {QStringLiteral("none"), QStringLiteral("drawer"), QStringLiteral("quit")},
                  {QStringLiteral("none"), QStringLiteral("drawer"), QStringLiteral("quit")}, chrome,
                  [this](const QString& t) {
                      applyGrid(
                          [&](PageGrid& grid) {
                              if (t == QLatin1String("drawer")) {
                                  grid.rootSlot = PageRootSlot::Drawer;
                              } else if (t == QLatin1String("quit")) {
                                  grid.rootSlot = PageRootSlot::Quit;
                              } else {
                                  grid.rootSlot = PageRootSlot::None;
                              }
                          },
                          QStringLiteral("Chrome slot"));
                  });
    b.check(form, QStringLiteral("Shell (always on top)"), g->shell, [this](bool on) {
        applyGrid([&](PageGrid& grid) { grid.shell = on; }, QStringLiteral("Shell"));
    });
    if (g->rootSlot == PageRootSlot::None) {
        b.check(form, QStringLiteral("Show"), g->show, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.show = on; }, QStringLiteral("Grid show"));
        });
    }
    b.check(form, QStringLiteral("Auto-close when idle"), g->autoClose, [this](bool on) {
        applyGrid([&](PageGrid& grid) { grid.autoClose = on; }, QStringLiteral("Grid auto close"));
    });
    if (g->autoClose) {
        b.integer(form, QStringLiteral("Idle ms"), g->autoCloseIdleMs, -1, 120000, [this](int v) {
            applyGrid([&](PageGrid& grid) { grid.autoCloseIdleMs = v; },
                      QStringLiteral("Grid idle ms"));
        });
    }
    b.heading(form, QStringLiteral("Cells"));
    b.integer(form, QStringLiteral("Columns"), g->columns, 1, 48, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.columns = v; }, QStringLiteral("Columns"));
    });
    b.integer(form, QStringLiteral("Rows"), g->rows, 1, 48, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.rows = v; }, QStringLiteral("Rows"));
    });
    QString weights;
    for (double w : g->rowWeights) {
        if (!weights.isEmpty()) {
            weights += QLatin1Char(',');
        }
        weights += QString::number(w, 'g', 8);
    }
    b.text(form, QStringLiteral("Row weights"), weights, [this](const QString& t) {
        applyGrid([&](PageGrid& grid) { grid.rowWeights = parseRowWeights(t); },
                  QStringLiteral("Row weights"));
    });
    b.integer(form, QStringLiteral("Gap px"), g->gapPx, 0, 64, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.gapPx = v; }, QStringLiteral("Gap"));
    });
    b.integer(form, QStringLiteral("Inset px"), g->marginPx, 0, 200, [this](int v) {
        applyGrid([&](PageGrid& grid) { grid.marginPx = v; }, QStringLiteral("Grid margin"));
    });
}

void LayoutEditorProperties::fillLeafIdentity(QFormLayout* form, const PageLeaf& item)
{
    PropertyBinder b{this, &m_loading};
    b.text(form, QStringLiteral("Id"), item.id, [this](const QString& t) {
        const QString next = t.trimmed();
        const QString cur = m_session.selection().itemId;
        if (next == cur) {
            return;
        }
        if (next.isEmpty() || PageEdit::allIds(m_session.document()).contains(next)) {
            rebuild();
            return;
        }
        applyDoc(
            [&](PageDocument& doc) {
                if (PageLeaf* leaf = PageEdit::findLeaf(doc, cur)) {
                    leaf->id = next;
                }
            },
            QStringLiteral("Id"));
        m_session.selectItem(next);
    });
    b.text(form, QStringLiteral("Label"), item.label, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.label = t; }, QStringLiteral("Label"));
    });
    b.text(form, QStringLiteral("Caption"), item.caption, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.caption = t; }, QStringLiteral("Caption"));
    });
    b.combo(form, QStringLiteral("Icon"), iconChoices(), item.icon, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.icon = t; }, QStringLiteral("Icon"));
    });
    b.combo(form, QStringLiteral("Role"),
            {QString(), QStringLiteral("label"), QStringLiteral("value"), QStringLiteral("tab"),
             QStringLiteral("toggle"), QStringLiteral("slider"), QStringLiteral("preview")},
            item.role, [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.role = t; }, QStringLiteral("Role"));
            });
    b.combo(form, QStringLiteral("Text style"),
            {QString(), QStringLiteral("caption"), QStringLiteral("body"), QStringLiteral("title"),
             QStringLiteral("section")},
            item.textStyle, [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.textStyle = t; }, QStringLiteral("Text style"));
            });
    b.text(form, QStringLiteral("Setting key"), item.settingKey, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.settingKey = t; }, QStringLiteral("Setting key"));
    });
    b.check(form, QStringLiteral("Show"), item.show, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.show = on; }, QStringLiteral("Show"));
    });
    fillVisibleWhen(form, item);
    b.check(form, QStringLiteral("Shell (always on top)"), item.shell, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.shell = on; }, QStringLiteral("Shell"));
    });
}

void LayoutEditorProperties::fillVisibleWhen(QFormLayout* form, const PageLeaf& item)
{
    PropertyBinder b{this, &m_loading};
    QStringList whenVals = visibleWhenChoices();
    QStringList whenLabels = {QStringLiteral("(always)"), QStringLiteral("expanded"),
                              QStringLiteral("!expanded"), QStringLiteral("quitConfirm"),
                              QStringLiteral("!quitConfirm"), QStringLiteral("dwellSuspend"),
                              QStringLiteral("!dwellSuspend")};
    if (!item.visibleWhen.isEmpty() && !whenVals.contains(item.visibleWhen)) {
        whenVals.prepend(item.visibleWhen);
        whenLabels.prepend(item.visibleWhen);
    }
    b.comboValues(form, QStringLiteral("Visible when"), whenLabels, whenVals, item.visibleWhen,
                  [this](const QString& t) {
                      applyItem([&](PageLeaf& it) { it.visibleWhen = t; },
                                QStringLiteral("Visible when"));
                  });
}

void LayoutEditorProperties::fillCell(QFormLayout* form)
{
    const PageLeaf* item = m_session.selectedItem();
    if (!item || !PageEdit::findCell(m_session.document(), item->id)) {
        return;
    }
    fillLeafIdentity(form, *item);
}

void LayoutEditorProperties::fillZone(QFormLayout* form)
{
    const PageLeaf* item = m_session.selectedItem();
    if (!item || !PageEdit::findZone(m_session.document(), item->id)) {
        return;
    }
    fillLeafIdentity(form, *item);
}

void LayoutEditorProperties::fillPlacement(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Grid) {
        const PageGrid* g = m_session.selectedGrid();
        if (!g) {
            return;
        }
        if (g->nested) {
            b.integer(form, QStringLiteral("Row"), g->row, 0, 64, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.row = v; }, QStringLiteral("Row"));
            });
            b.integer(form, QStringLiteral("Column"), g->col, 0, 64, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.col = v; }, QStringLiteral("Column"));
            });
            b.integer(form, QStringLiteral("Row span"), g->rowSpan, 1, 16, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.rowSpan = v; }, QStringLiteral("Row span"));
            });
            b.integer(form, QStringLiteral("Column span"), g->colSpan, 1, 16, [this](int v) {
                applyGrid([&](PageGrid& grid) { grid.colSpan = v; }, QStringLiteral("Column span"));
            });
            return;
        }
        b.check(form, QStringLiteral("Desktop bounds"), g->desktopMode, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.desktopMode = on; }, QStringLiteral("Desktop mode"));
        });
        b.combo(form, QStringLiteral("Anchor"), pageAnchorNames(),
                PageDimParse::anchorName(g->anchor), [this](const QString& t) {
                    applyGrid(
                        [&](PageGrid& grid) {
                            bool ok = true;
                            grid.anchor = PageDimParse::parseAnchor(t, &ok);
                        },
                        QStringLiteral("Anchor"));
                });
        b.dim(form, QStringLiteral("Offset X"), g->offset.x, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.offset.x = v; }, QStringLiteral("Offset X"));
        });
        b.dim(form, QStringLiteral("Offset Y"), g->offset.y, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.offset.y = v; }, QStringLiteral("Offset Y"));
        });
        b.dim(form, QStringLiteral("Width"), g->size.x, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.size.x = v; }, QStringLiteral("Width"));
        });
        b.dim(form, QStringLiteral("Height"), g->size.y, [this](PageDim v) {
            applyGrid([&](PageGrid& grid) { grid.size.y = v; }, QStringLiteral("Height"));
        });
        b.check(form, QStringLiteral("Drawer motion"), g->drawerMotion, [this](bool on) {
            applyGrid([&](PageGrid& grid) { grid.drawerMotion = on; },
                      QStringLiteral("Drawer motion"));
        });
        return;
    }
    if (kind == Kind::Cell) {
        const PageLeaf* item = m_session.selectedItem();
        const PageCell* c = item ? PageEdit::findCell(m_session.document(), item->id) : nullptr;
        if (!c) {
            return;
        }
        b.integer(form, QStringLiteral("Row"), c->row, 0, 64, [this](int v) {
            applyCell([&](PageCell& cell) { cell.row = v; }, QStringLiteral("Row"));
        });
        b.integer(form, QStringLiteral("Column"), c->col, 0, 64, [this](int v) {
            applyCell([&](PageCell& cell) { cell.col = v; }, QStringLiteral("Column"));
        });
        b.integer(form, QStringLiteral("Row span"), c->rowSpan, 1, 16, [this](int v) {
            applyCell([&](PageCell& cell) { cell.rowSpan = v; }, QStringLiteral("Row span"));
        });
        b.integer(form, QStringLiteral("Column span"), c->colSpan, 1, 16, [this](int v) {
            applyCell([&](PageCell& cell) { cell.colSpan = v; }, QStringLiteral("Column span"));
        });
        return;
    }
    const PageLeaf* item = m_session.selectedItem();
    const PageZone* z = item ? PageEdit::findZone(m_session.document(), item->id) : nullptr;
    if (!z) {
        return;
    }
    b.heading(form, QStringLiteral("Progress zone (offset from page anchor)"));
    b.check(form, QStringLiteral("Desktop bounds"), z->desktopMode, [this](bool on) {
        applyZone([&](PageZone& zone) { zone.desktopMode = on; }, QStringLiteral("Desktop mode"));
    });
    b.combo(form, QStringLiteral("Anchor"), pageAnchorNames(), PageDimParse::anchorName(z->anchor),
            [this](const QString& t) {
                applyZone(
                    [&](PageZone& zone) {
                        bool ok = true;
                        zone.anchor = PageDimParse::parseAnchor(t, &ok);
                    },
                    QStringLiteral("Anchor"));
            });
    b.dim(form, QStringLiteral("Offset X"), z->offset.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.offset.x = v; }, QStringLiteral("Offset X"));
    });
    b.dim(form, QStringLiteral("Offset Y"), z->offset.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.offset.y = v; }, QStringLiteral("Offset Y"));
    });
    b.dim(form, QStringLiteral("Width"), z->size.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.size.x = v; }, QStringLiteral("Width"));
    });
    b.dim(form, QStringLiteral("Height"), z->size.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.size.y = v; }, QStringLiteral("Height"));
    });
    b.heading(form, QStringLiteral("Dwell zone (offset from progress anchor)"));
    b.dim(form, QStringLiteral("Offset X"), z->dwellOffset.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellOffset.x = v; }, QStringLiteral("Dwell offset X"));
    });
    b.dim(form, QStringLiteral("Offset Y"), z->dwellOffset.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellOffset.y = v; }, QStringLiteral("Dwell offset Y"));
    });
    b.dim(form, QStringLiteral("Width"), z->dwellSize.x, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellSize.x = v; }, QStringLiteral("Dwell width"));
    });
    b.dim(form, QStringLiteral("Height"), z->dwellSize.y, [this](PageDim v) {
        applyZone([&](PageZone& zone) { zone.dwellSize.y = v; }, QStringLiteral("Dwell height"));
    });
}

void LayoutEditorProperties::fillStyle(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Style) {
        const QString styleId = m_session.selection().itemId;
        addNamedChromeEditor(
            b, form, m_session.document().styles, styleId,
            [this](const QString& id) { selectNamedStyle(id); },
            [this]() { m_session.addNamedStyle(); },
            [this](const QString& from, const QString& to) { renameNamedStyle(from, to); },
            [this](const QString& id) {
                applyDoc([&](PageDocument& doc) { doc.styles.remove(id); },
                         QStringLiteral("Delete style"));
                m_session.selectTarget(EditorTarget::Document);
            },
            [this, styleId](const QString& undo, const auto& mut) {
                applyDoc(
                    [&](PageDocument& doc) {
                        const auto it = doc.styles.find(styleId);
                        if (it != doc.styles.end()) {
                            mut(*it);
                        }
                    },
                    undo);
            });
        return;
    }
    if (kind == Kind::Page) {
        const PageDocument& d = m_session.document();
        addChromeFields(b, form, d.style, [this](const QString& undo, const auto& mut) {
            applyDoc([&](PageDocument& doc) { mut(doc.style); }, undo);
        });
        return;
    }
    if (kind == Kind::Grid) {
        const PageGrid* g = m_session.selectedGrid();
        if (!g) {
            return;
        }
        addOptionalIdCombo(b, form, QStringLiteral("Inherit"), m_session.document().styles.keys(),
                           g->styleId, [this](const QString& t) {
                               applyGrid([&](PageGrid& grid) { grid.styleId = t; },
                                         QStringLiteral("Grid style"));
                           });
        addChromeFields(b, form, g->style, [this](const QString& undo, const auto& mut) {
            applyGrid([&](PageGrid& grid) { mut(grid.style); }, undo);
        }, false);
        return;
    }
    const PageLeaf* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    addOptionalIdCombo(b, form, QStringLiteral("Inherit"), m_session.document().styles.keys(),
                       item->styleId, [this](const QString& t) {
                           applyItem([&](PageLeaf& it) { it.styleId = t; },
                                     QStringLiteral("Style"));
                       });
    addChromeFields(b, form, item->style, [this](const QString& undo, const auto& mut) {
        m_session.applyChromeToSelected(mut, undo);
        rebuildIfNeeded();
    }, false);
}

void LayoutEditorProperties::fillDwell(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const Kind kind = currentKind();
    if (kind == Kind::Dwell) {
        const QString dwellId = m_session.selection().itemId;
        addNamedDwellEditor(
            b, form, m_session.document().dwells, dwellId,
            [this](const QString& id) { selectNamedDwell(id); },
            [this]() { m_session.addNamedDwell(); },
            [this](const QString& from, const QString& to) { renameNamedDwell(from, to); },
            [this](const QString& id) {
                applyDoc([&](PageDocument& doc) { doc.dwells.remove(id); },
                         QStringLiteral("Delete dwell"));
                m_session.selectTarget(EditorTarget::Document);
            },
            [this, dwellId](const QString& undo, const auto& mut) {
                applyDoc(
                    [&](PageDocument& doc) {
                        const auto it = doc.dwells.find(dwellId);
                        if (it != doc.dwells.end()) {
                            mut(*it);
                        }
                    },
                    undo);
            });
        return;
    }
    if (kind == Kind::Page) {
        const PageDocument& d = m_session.document();
        addDwellFields(b, form, d.dwell, [this](const QString& undo, const auto& mut) {
            applyDoc([&](PageDocument& doc) { mut(doc.dwell); }, undo);
        });
        return;
    }
    if (kind == Kind::Cell || kind == Kind::Zone) {
        const PageLeaf* item = m_session.selectedItem();
        if (!item) {
            return;
        }
        addDwellFields(
            b, form, item->dwell,
            [this](const QString& undo, const auto& mut) {
                applyItem([&](PageLeaf& it) { mut(it.dwell); }, undo);
            },
            true, m_session.document().dwells.keys(), item->dwellId, [this](const QString& t) {
                applyItem([&](PageLeaf& it) { it.dwellId = t; }, QStringLiteral("Dwell"));
            });
        return;
    }
}

void LayoutEditorProperties::fillAction(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const PageLeaf* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    b.check(form, QStringLiteral("Interactive"), item->interactive, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.interactive = on; }, QStringLiteral("Interactive"));
    });
    b.check(form, QStringLiteral("Still works while Sleep is on"), item->suspendExempt,
            [this](bool on) {
                applyItem([&](PageLeaf& it) { it.suspendExempt = on; },
                          QStringLiteral("Suspend exempt"));
            });
    b.check(form, QStringLiteral("Loop until activated again"), item->actionLoop, [this](bool on) {
        applyItem([&](PageLeaf& it) { it.actionLoop = on; }, QStringLiteral("Action loop"));
    });
    b.text(form, QStringLiteral("Active-state key"), item->activeState, [this](const QString& t) {
        applyItem([&](PageLeaf& it) { it.activeState = t; }, QStringLiteral("Active state"));
    });
    addActionSeriesFields(b, form, item->actions, item->label, m_catalog, m_actionStep,
                          [this](int step) {
                              if (m_actionStep == step) {
                                  return;
                              }
                              m_actionStep = step;
                              QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
                          },
                          [this, id = item->id](QVector<PageAction> next) {
                              m_session.setActions(id, std::move(next));
                              rebuildIfNeeded();
                          });
}

} // namespace gazer
