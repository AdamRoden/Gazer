#include "editor/LayoutEditorProperties.h"

#include "editor/LayoutEditorFields.h"
#include "layout/LayoutSchema.h"

#include <QFormLayout>
#include <QLabel>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace gazer {

LayoutEditorProperties::LayoutEditorProperties(LayoutEditorSession& session, QWidget* parent)
    : QWidget(parent)
    , m_session(session)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Properties"));
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    m_tabs = new QTabWidget(this);
    auto makePage = [](QFormLayout** outForm) {
        auto* page = new QWidget;
        auto* form = new QFormLayout(page);
        form->setContentsMargins(8, 10, 8, 8);
        form->setSpacing(8);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        *outForm = form;
        return page;
    };
    m_tabs->addTab(makePage(&m_form0), QStringLiteral("Board"));
    m_tabs->addTab(makePage(&m_form1), QStringLiteral("Window"));
    m_tabs->addTab(makePage(&m_form2), QStringLiteral("Grid"));
    m_tabs->addTab(makePage(&m_form3), QStringLiteral("Gaze"));
    root->addWidget(m_tabs, 1);

    connect(&m_session, &LayoutEditorSession::selectionChanged, this, [this]() {
        m_actionStep = 0;
        rebuild();
    });
    connect(&m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        if (!m_applying) {
            rebuild();
        }
    });
    rebuild();
}

void LayoutEditorProperties::showBoardTab()
{
    m_session.selectTarget(EditorTarget::Document);
    m_tabs->setCurrentIndex(0);
}

void LayoutEditorProperties::showWindowTab()
{
    m_session.selectTarget(EditorTarget::Window);
    syncTabs(false);
    m_tabs->setCurrentIndex(1);
}

void LayoutEditorProperties::showGridTab()
{
    m_session.selectTarget(EditorTarget::Grid);
    syncTabs(false);
    m_tabs->setCurrentIndex(2);
}

void LayoutEditorProperties::showDwellTab()
{
    m_session.selectTarget(EditorTarget::Dwell);
    syncTabs(false);
    m_tabs->setCurrentIndex(3);
}

void LayoutEditorProperties::showStyleTab()
{
    showBoardTab();
}

void LayoutEditorProperties::showLayoutTab()
{
    showGridTab();
}

void LayoutEditorProperties::showInteractionTab()
{
    showDwellTab();
}

void LayoutEditorProperties::clearLayout(QFormLayout* form)
{
    while (form->count() > 0) {
        QLayoutItem* it = form->takeAt(0);
        if (it->widget()) {
            it->widget()->deleteLater();
        }
        delete it;
    }
}

void LayoutEditorProperties::syncTabs(bool itemSelected)
{
    if (itemSelected == m_itemMode) {
        return;
    }
    m_itemMode = itemSelected;
    if (itemSelected) {
        m_tabs->setTabText(0, QStringLiteral("Item"));
        m_tabs->setTabText(1, QStringLiteral("Layout"));
        m_tabs->setTabText(2, QStringLiteral("Action"));
        m_tabs->setTabVisible(3, false);
        m_tabs->setCurrentIndex(0);
    } else {
        m_tabs->setTabText(0, QStringLiteral("Board"));
        m_tabs->setTabText(1, QStringLiteral("Window"));
        m_tabs->setTabText(2, QStringLiteral("Grid"));
        m_tabs->setTabText(3, QStringLiteral("Gaze"));
        m_tabs->setTabVisible(3, true);
        m_tabs->setCurrentIndex(0);
    }
}

void LayoutEditorProperties::rebuild()
{
    if (m_loading || m_applying) {
        return;
    }
    m_loading = true;
    const bool item = m_session.selection().target == EditorTarget::Item;
    syncTabs(item);
    clearLayout(m_form0);
    clearLayout(m_form1);
    clearLayout(m_form2);
    clearLayout(m_form3);
    if (item) {
        fillItem(m_form0);
        fillItemLayout(m_form1);
        fillItemAction(m_form2);
    } else {
        fillBoard(m_form0);
        fillWindow(m_form1);
        fillGrid(m_form2);
        fillBoardDwell(m_form3);
    }
    m_loading = false;
}

void LayoutEditorProperties::applyItem(const std::function<void(LayoutItem&)>& fn,
                                       const QString& undoLabel)
{
    const QStringList ids = m_session.selection().itemIds;
    m_applying = true;
    m_session.edit(undoLabel, [&](LayoutDocument& d) {
        for (LayoutItem& it : d.items) {
            if (!ids.contains(it.id)) {
                continue;
            }
            fn(it);
            it.applyKind();
        }
    });
    m_applying = false;
    QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
}

void LayoutEditorProperties::applyDoc(const std::function<void(LayoutDocument&)>& fn,
                                      const QString& undoLabel)
{
    m_applying = true;
    m_session.edit(undoLabel, fn);
    m_applying = false;
    QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
}

void LayoutEditorProperties::fillBoard(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutDocument& d = m_session.document();
    b.heading(form, QStringLiteral("Board"));
    b.text(form, QStringLiteral("Id"), d.id, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.id = t.trimmed(); }, QStringLiteral("Layout id"));
    });
    b.text(form, QStringLiteral("Name"), d.name, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.name = t; }, QStringLiteral("Name"));
    });
    b.text(form, QStringLiteral("Description"), d.description, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.description = t; }, QStringLiteral("Description"));
    });
    b.check(form, QStringLiteral("Process-lifetime root"), d.master, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.master = on; }, QStringLiteral("Master"));
    });
    b.check(form, QStringLiteral("Hide until gaze reveal"), d.hideUntilGazeReveal, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.hideUntilGazeReveal = on; },
                 QStringLiteral("Gaze reveal"));
    });
    b.check(form, QStringLiteral("Auto-close when idle"), d.autoClose, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.autoClose = on; }, QStringLiteral("Auto close"));
    });
    if (d.autoClose) {
        b.integer(form, QStringLiteral("Idle ms"), d.autoCloseIdleMs, -1, 120000, [this](int v) {
            applyDoc([&](LayoutDocument& doc) { doc.autoCloseIdleMs = v; },
                     QStringLiteral("Idle ms"));
        });
        b.integer(form, QStringLiteral("Fade ms"), d.autoCloseFadeMs, -1, 30000, [this](int v) {
            applyDoc([&](LayoutDocument& doc) { doc.autoCloseFadeMs = v; },
                     QStringLiteral("Fade ms"));
        });
        b.note(form, QStringLiteral("−1 uses the app setting."));
    }
}

void LayoutEditorProperties::fillWindow(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutDocument& d = m_session.document();
    const bool shown = d.placement.specified && !d.placement.hidden;
    b.heading(form, QStringLiteral("Window"));
    b.check(form, QStringLiteral("Show on-screen board"), shown, [this](bool on) {
        applyDoc(
            [&](LayoutDocument& doc) {
                doc.placement.specified = on;
                doc.placement.hidden = !on;
            },
            QStringLiteral("Show window"));
    });
    if (!shown) {
        b.note(form, QStringLiteral("Headless: only free items (if any) appear on screen."));
        return;
    }
    b.combo(form, QStringLiteral("Anchor"), LayoutSchema::windowAnchorNames(),
            LayoutSchema::windowAnchorName(d.placement.anchor), [this](const QString& t) {
                applyDoc(
                    [&](LayoutDocument& doc) {
                        doc.placement.specified = true;
                        doc.placement.anchor = LayoutSchema::windowAnchorFromName(t);
                    },
                    QStringLiteral("Anchor"));
            });
    b.dim(form, QStringLiteral("Width"), d.placement.width, [this](DimSpec v) {
        applyDoc(
            [&](LayoutDocument& doc) {
                doc.placement.specified = true;
                doc.placement.width = v;
            },
            QStringLiteral("Width"));
    });
    b.dim(form, QStringLiteral("Height"), d.placement.height, [this](DimSpec v) {
        applyDoc(
            [&](LayoutDocument& doc) {
                doc.placement.specified = true;
                doc.placement.height = v;
            },
            QStringLiteral("Height"));
    });
    b.dim(form, QStringLiteral("X"), d.placement.x, [this](DimSpec v) {
        applyDoc([&](LayoutDocument& doc) { doc.placement.x = v; }, QStringLiteral("X"));
    });
    b.dim(form, QStringLiteral("Y"), d.placement.y, [this](DimSpec v) {
        applyDoc([&](LayoutDocument& doc) { doc.placement.y = v; }, QStringLiteral("Y"));
    });
    b.integer(form, QStringLiteral("Edge inset px"), d.placement.marginPx, 0, 400, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.placement.marginPx = v; },
                 QStringLiteral("Margin"));
    });
    b.combo(form, QStringLiteral("Bounds"),
            {QStringLiteral("desktop"), QStringLiteral("screen")},
            LayoutSchema::boundsModeName(d.placement.hasBoundsMode ? d.placement.boundsMode
                                                                   : BoundsMode::Desktop),
            [this](const QString& t) {
                applyDoc(
                    [&](LayoutDocument& doc) {
                        doc.placement.hasBoundsMode = true;
                        doc.placement.boundsMode = LayoutSchema::boundsModeFromName(t);
                    },
                    QStringLiteral("Window bounds"));
            });
    b.note(form, QStringLiteral("desktop = work area (excludes the taskbar). screen = full display."));
    b.check(form, QStringLiteral("Stack in front of the taskbar"), d.placement.aboveTaskbar,
            [this](bool on) {
                applyDoc([&](LayoutDocument& doc) { doc.placement.aboveTaskbar = on; },
                         QStringLiteral("Above taskbar"));
            });
    b.note(form, QStringLiteral("Z-order only (HWND_TOPMOST). Does not change Bounds."));
    b.check(form, QStringLiteral("Drawer motion"), d.placement.drawerMotion, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.placement.drawerMotion = on; },
                 QStringLiteral("Drawer motion"));
    });
    addChromeFields(b, form, d.placement.style, [this](const QString& undo, const auto& mut) {
        applyDoc([&](LayoutDocument& doc) { mut(doc.placement.style); }, undo);
    });
}

void LayoutEditorProperties::fillGrid(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutDocument& d = m_session.document();
    b.heading(form, QStringLiteral("Grid"));
    b.integer(form, QStringLiteral("Columns"), d.grid.columns, 1, 48, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.columns = v; }, QStringLiteral("Columns"));
    });
    b.integer(form, QStringLiteral("Rows"), d.grid.rows, 1, 48, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.rows = v; }, QStringLiteral("Rows"));
    });
    b.integer(form, QStringLiteral("Gap px"), d.grid.gapPx, 0, 64, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.gapPx = v; }, QStringLiteral("Gap"));
    });
    b.integer(form, QStringLiteral("Inset px"), d.grid.marginPx, 0, 200, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.marginPx = v; },
                 QStringLiteral("Grid margin"));
    });
}

void LayoutEditorProperties::fillBoardDwell(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    addDwellFields(b, form, m_session.document().dwell,
                   [this](const QString& undo, const auto& mut) {
                       applyDoc([&](LayoutDocument& doc) { mut(doc.dwell); }, undo);
                   },
                   true);
}

void LayoutEditorProperties::fillItem(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const EditorSelection sel = m_session.selection();
    const LayoutItem* item = m_session.selectedItem();
    if (!item) {
        b.note(form, QStringLiteral("No item selected."));
        return;
    }
    if (sel.itemIds.size() > 1) {
        b.note(form, QStringLiteral("Editing %1 selected items.").arg(sel.itemIds.size()));
    }

    b.heading(form, QStringLiteral("Item"));
    b.combo(form, QStringLiteral("Anchor"), LayoutSchema::itemAnchorNames(),
            LayoutSchema::itemAnchorName(*item), [this](const QString& t) {
                applyItem(
                    [&](LayoutItem& it) {
                        it.setAnchor(LayoutSchema::itemAnchorFromName(t));
                    },
                    QStringLiteral("Anchor"));
            });
    b.note(form, QStringLiteral("cell = row/column on the board. Other values are free "
                                "(anchor point on the screen)."));
    const QString roleUi = item->role.isEmpty() ? QStringLiteral("button") : item->role;
    b.combo(form, QStringLiteral("Role"),
            {QStringLiteral("button"), QStringLiteral("label"), QStringLiteral("tab"),
             QStringLiteral("toggle"), QStringLiteral("slider"), QStringLiteral("preview")},
            roleUi, [this](const QString& t) {
                applyItem(
                    [&](LayoutItem& it) {
                        it.role = t == QLatin1String("button") ? QString() : t;
                        it.applyKind();
                    },
                    QStringLiteral("Role"));
            });
    b.text(form, QStringLiteral("Text"), item->label, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.label = t; }, QStringLiteral("Label"));
    });
    b.text(form, QStringLiteral("Caption"), item->caption, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.caption = t; }, QStringLiteral("Caption"));
    });
    b.text(form, QStringLiteral("Icon"), item->icon, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.icon = t; }, QStringLiteral("Icon"));
    });
    if (item->kind == LayoutItemKind::Label || item->kind == LayoutItemKind::Tab) {
        b.combo(form, QStringLiteral("Text style"),
                {QStringLiteral(""), QStringLiteral("caption"), QStringLiteral("body"),
                 QStringLiteral("bodyStrong"), QStringLiteral("subtitle"),
                 QStringLiteral("title"), QStringLiteral("section")},
                item->textStyle, [this](const QString& t) {
                    applyItem([&](LayoutItem& it) { it.textStyle = t; },
                             QStringLiteral("Text style"));
                });
    }
    if (item->kind == LayoutItemKind::Label || item->kind == LayoutItemKind::Preview) {
        b.text(form, QStringLiteral("Setting key"), item->settingKey, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.settingKey = t; }, QStringLiteral("Setting key"));
        });
    }

    addChromeFields(b, form, item->style, [this](const QString& undo, const auto& mut) {
        m_session.applyChromeToSelected(mut, undo);
        QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
    });
}

void LayoutEditorProperties::fillItemLayout(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutItem* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    b.heading(form, QStringLiteral("Identity"));
    b.text(form, QStringLiteral("Id"), item->id, [this, old = item->id](const QString& t) {
        const QString next = t.trimmed();
        if (next.isEmpty() || next == old) {
            return;
        }
        if (m_session.itemById(next)) {
            m_session.notify(QStringLiteral("Id already in use: %1").arg(next));
            QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
            return;
        }
        applyDoc(
            [old, next](LayoutDocument& doc) {
                for (LayoutItem& it : doc.items) {
                    if (it.id == old) {
                        it.id = next;
                        break;
                    }
                }
            },
            QStringLiteral("Rename item"));
        m_session.selectItem(next);
    });

    if (item->isUnbounded()) {
        b.heading(form, QStringLiteral("Free placement"));
        b.note(form, QStringLiteral("Offset and size are relative to the Anchor point."));
        b.dim(form, QStringLiteral("X"), item->dwellRegion.x, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.x = v; }, QStringLiteral("X"));
        });
        b.dim(form, QStringLiteral("Y"), item->dwellRegion.y, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.y = v; }, QStringLiteral("Y"));
        });
        b.dim(form, QStringLiteral("Width"), item->dwellRegion.width, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.width = v; }, QStringLiteral("Width"));
        });
        b.dim(form, QStringLiteral("Height"), item->dwellRegion.height, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.height = v; }, QStringLiteral("Height"));
        });
    } else {
        b.heading(form, QStringLiteral("Cell"));
        b.integer(form, QStringLiteral("Row"), item->row, 0, 64, [this](int v) {
            applyItem([&](LayoutItem& it) { it.row = v; }, QStringLiteral("Row"));
        });
        b.integer(form, QStringLiteral("Column"), item->col, 0, 64, [this](int v) {
            applyItem([&](LayoutItem& it) { it.col = v; }, QStringLiteral("Column"));
        });
        b.integer(form, QStringLiteral("Row span"), item->rowSpan, 1, 16, [this](int v) {
            applyItem([&](LayoutItem& it) { it.rowSpan = v; }, QStringLiteral("Row span"));
        });
        b.integer(form, QStringLiteral("Column span"), item->colSpan, 1, 16, [this](int v) {
            applyItem([&](LayoutItem& it) { it.colSpan = v; }, QStringLiteral("Column span"));
        });
        b.real(form, QStringLiteral("Key width (u)"), item->widthUnits, 0, 24, 2, [this](double v) {
            applyItem([&](LayoutItem& it) { it.widthUnits = v; }, QStringLiteral("Width units"));
        });
        b.note(form, QStringLiteral("0 = equal columns. Keyboard keys use letter-widths."));
    }

    b.text(form, QStringLiteral("Visible when"), item->visibleWhen, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.visibleWhen = t; }, QStringLiteral("Visible when"));
    });
}

void LayoutEditorProperties::fillItemAction(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutItem* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    b.heading(form, QStringLiteral("Activation"));
    b.check(form, QStringLiteral("Gaze / click can activate"), item->interactive, [this](bool on) {
        applyItem([&](LayoutItem& it) { it.interactive = on; }, QStringLiteral("Interactive"));
    });
    b.check(form, QStringLiteral("Still works while Sleep is on"), item->dwellExempt,
            [this](bool on) {
                applyItem([&](LayoutItem& it) { it.dwellExempt = on; },
                         QStringLiteral("Dwell exempt"));
            });
    b.check(form, QStringLiteral("Loop until activated again"), item->actionLoop, [this](bool on) {
        applyItem([&](LayoutItem& it) { it.actionLoop = on; }, QStringLiteral("Action loop"));
    });
    if (item->kind == LayoutItemKind::Toggle || item->actionLoop) {
        b.text(form, QStringLiteral("Active-state key"), item->activeState,
               [this](const QString& t) {
                   applyItem([&](LayoutItem& it) { it.activeState = t; },
                            QStringLiteral("Active state"));
               });
    }

    const QVector<LayoutAction> acts = item->effectiveActions();
    addActionSeriesFields(b, form, acts, item->label, m_catalog, m_actionStep,
                          [this](int step) {
                              if (m_actionStep == step) {
                                  return;
                              }
                              m_actionStep = step;
                              QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
                          },
                          [this, id = item->id](QVector<LayoutAction> next) {
                              m_session.setActions(id, std::move(next));
                              QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
                          });

    b.check(form, QStringLiteral("Custom gaze timing"), item->dwell.sectionPresent, [this](bool on) {
        applyItem(
            [&](LayoutItem& it) {
                it.dwell.sectionPresent = on;
                if (!on) {
                    it.dwell = LayoutDwellConfig{};
                }
            },
            QStringLiteral("Custom dwell"));
    });
    if (item->dwell.sectionPresent) {
        addDwellFields(b, form, item->dwell,
                       [this](const QString& undo, const auto& mut) {
                           applyItem([&](LayoutItem& it) { mut(it.dwell); }, undo);
                       },
                       false);
    }
}

} // namespace gazer
