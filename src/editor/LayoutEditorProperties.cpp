#include "editor/LayoutEditorProperties.h"

#include "editor/LayoutEditorFields.h"
#include "layout/LayoutSchema.h"

#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
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
    m_tabs->addTab(makePage(&m_styleForm), QStringLiteral("Style"));
    m_tabs->addTab(makePage(&m_layoutForm), QStringLiteral("Layout"));
    m_tabs->addTab(makePage(&m_interactForm), QStringLiteral("Interaction"));
    root->addWidget(m_tabs, 3);

    auto* hierTitle = new QLabel(QStringLiteral("Elements hierarchy"));
    hierTitle->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(hierTitle);

    m_hierarchy = new QTreeWidget(this);
    m_hierarchy->setHeaderHidden(true);
    m_hierarchy->setRootIsDecorated(true);
    m_hierarchy->setMaximumHeight(220);
    root->addWidget(m_hierarchy, 1);

    connect(m_hierarchy, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) {
            return;
        }
        const QString kind = item->data(0, Qt::UserRole).toString();
        const QString id = item->data(0, Qt::UserRole + 1).toString();
        if (kind == QLatin1String("item")) {
            m_session.selectItem(id);
        } else if (kind == QLatin1String("window")) {
            m_session.selectTarget(EditorTarget::Window);
        } else if (kind == QLatin1String("grid")) {
            m_session.selectTarget(EditorTarget::Grid);
        } else if (kind == QLatin1String("dwell")) {
            m_session.selectTarget(EditorTarget::Dwell);
        } else if (kind == QLatin1String("style")) {
            m_session.selectTarget(EditorTarget::Style);
        } else {
            m_session.selectTarget(EditorTarget::Document);
        }
    });

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

void LayoutEditorProperties::showStyleTab()
{
    m_tabs->setCurrentIndex(0);
}

void LayoutEditorProperties::showLayoutTab()
{
    m_tabs->setCurrentIndex(1);
}

void LayoutEditorProperties::showInteractionTab()
{
    m_tabs->setCurrentIndex(2);
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

void LayoutEditorProperties::rebuild()
{
    if (m_loading || m_applying) {
        return;
    }
    m_loading = true;
    clearLayout(m_styleForm);
    clearLayout(m_layoutForm);
    clearLayout(m_interactForm);
    fillStyle(m_styleForm);
    fillLayout(m_layoutForm);
    fillInteraction(m_interactForm);
    rebuildHierarchy();
    m_loading = false;
}

void LayoutEditorProperties::rebuildHierarchy()
{
    const QSignalBlocker block(m_hierarchy);
    m_hierarchy->clear();
    const LayoutDocument& d = m_session.document();
    const EditorSelection sel = m_session.selection();

    auto add = [&](QTreeWidgetItem* parent, const QString& label, const QString& kind,
                   const QString& id, bool selected) {
        auto* item = parent ? new QTreeWidgetItem(parent, {label})
                            : new QTreeWidgetItem(m_hierarchy, {label});
        item->setData(0, Qt::UserRole, kind);
        item->setData(0, Qt::UserRole + 1, id);
        if (selected) {
            m_hierarchy->setCurrentItem(item);
        }
        return item;
    };

    auto* root = add(nullptr, d.name.isEmpty() ? d.id : d.name, QStringLiteral("document"), {},
                     sel.target == EditorTarget::Document);
    root->setExpanded(true);
    add(root, QStringLiteral("Window"), QStringLiteral("window"), {},
        sel.target == EditorTarget::Window);
    add(root, QStringLiteral("Grid"), QStringLiteral("grid"), {}, sel.target == EditorTarget::Grid);
    add(root, QStringLiteral("Dwell"), QStringLiteral("dwell"), {},
        sel.target == EditorTarget::Dwell);
    add(root, QStringLiteral("Style"), QStringLiteral("style"), {},
        sel.target == EditorTarget::Style);
    auto* items = add(root, QStringLiteral("Items"), QStringLiteral("document"), {}, false);
    items->setExpanded(true);
    for (const LayoutItem& it : d.items) {
        const QString label =
            it.label.isEmpty() ? it.id : QStringLiteral("%1  (%2)").arg(it.label, it.id);
        add(items, label, QStringLiteral("item"), it.id,
            sel.target == EditorTarget::Item && sel.itemId == it.id);
    }
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

void LayoutEditorProperties::fillStyle(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const EditorSelection sel = m_session.selection();
    const LayoutDocument& d = m_session.document();

    if (sel.target == EditorTarget::Item) {
        const LayoutItem* item = m_session.selectedItem();
        if (!item) {
            b.note(form, QStringLiteral("No item selected."));
            return;
        }
        if (sel.itemIds.size() > 1) {
            b.note(form, QStringLiteral("Editing %1 selected items (style applies to all).")
                             .arg(sel.itemIds.size()));
        }
        b.heading(form, QStringLiteral("Content"));
        b.text(form, QStringLiteral("Text"), item->label, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.label = t; }, QStringLiteral("Label"));
        });
        b.text(form, QStringLiteral("Caption"), item->caption, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.caption = t; }, QStringLiteral("Caption"));
        });
        b.text(form, QStringLiteral("Icon"), item->icon, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.icon = t; }, QStringLiteral("Icon"));
        });
        b.combo(form, QStringLiteral("Role"),
                {QStringLiteral(""), QStringLiteral("label"), QStringLiteral("tab"),
                 QStringLiteral("toggle"), QStringLiteral("slider"), QStringLiteral("preview")},
                item->role, [this](const QString& t) {
                    applyItem([&](LayoutItem& it) { it.role = t; }, QStringLiteral("Role"));
                });
        b.combo(form, QStringLiteral("Text style"),
                {QStringLiteral(""), QStringLiteral("caption"), QStringLiteral("body"),
                 QStringLiteral("bodyStrong"), QStringLiteral("subtitle"),
                 QStringLiteral("title"), QStringLiteral("section")},
                item->textStyle, [this](const QString& t) {
                    applyItem([&](LayoutItem& it) { it.textStyle = t; }, QStringLiteral("Text style"));
                });
        addChromeFields(b, form, item->style, [this](const QString& undo, const auto& mut) {
            m_session.applyChromeToSelected(mut, undo);
            QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
        });
        return;
    }

    if (sel.target == EditorTarget::Window) {
        b.heading(form, QStringLiteral("Window chrome"));
        addChromeFields(b, form, d.placement.style, [this](const QString& undo, const auto& mut) {
            applyDoc([&](LayoutDocument& doc) { mut(doc.placement.style); }, undo);
        });
        b.check(form, QStringLiteral("Above taskbar"), d.placement.aboveTaskbar, [this](bool on) {
            applyDoc([&](LayoutDocument& doc) { doc.placement.aboveTaskbar = on; },
                     QStringLiteral("Above taskbar"));
        });
        b.check(form, QStringLiteral("Drawer motion"), d.placement.drawerMotion, [this](bool on) {
            applyDoc([&](LayoutDocument& doc) { doc.placement.drawerMotion = on; },
                     QStringLiteral("Drawer motion"));
        });
        return;
    }

    if (sel.target == EditorTarget::Style || sel.target == EditorTarget::Document) {
        b.heading(form, QStringLiteral("Default item style"));
        addChromeFields(b, form, d.style, [this](const QString& undo, const auto& mut) {
            applyDoc([&](LayoutDocument& doc) { mut(doc.style); }, undo);
        });
        return;
    }

    b.note(form, QStringLiteral("Select an item, the window, or Style to edit chrome."));
}

void LayoutEditorProperties::fillLayout(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const EditorSelection sel = m_session.selection();
    const LayoutDocument& d = m_session.document();

    if (sel.target == EditorTarget::Item) {
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
        b.heading(form, QStringLiteral("Grid"));
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
        b.real(form, QStringLiteral("Width units (u)"), item->widthUnits, 0, 24, 2, [this](double v) {
            applyItem([&](LayoutItem& it) { it.widthUnits = v; }, QStringLiteral("Width units"));
        });
        b.check(form, QStringLiteral("Unbounded"), item->unbounded, [this](bool on) {
            applyItem([&](LayoutItem& it) { it.unbounded = on; }, QStringLiteral("Unbounded"));
        });
        b.check(form, QStringLiteral("Visible"), item->visible, [this](bool on) {
            applyItem([&](LayoutItem& it) { it.visible = on; }, QStringLiteral("Visible"));
        });
        b.text(form, QStringLiteral("Visible when"), item->visibleWhen, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.visibleWhen = t; }, QStringLiteral("Visible when"));
        });
        b.heading(form, QStringLiteral("Dwell region"));
        b.check(form, QStringLiteral("Has dwell region"), item->hasDwellRegion, [this](bool on) {
            applyItem(
                [&](LayoutItem& it) {
                    it.hasDwellRegion = on;
                    if (on && !it.dwellRegion.width.isSet()) {
                        it.dwellRegion.width = DimSpec::pixels(80);
                        it.dwellRegion.height = DimSpec::pixels(80);
                    }
                },
                QStringLiteral("Dwell region"));
        });
        QStringList anchors = {QStringLiteral("(none)")};
        anchors.append(LayoutSchema::screenAnchorNames());
        const QString sa = LayoutSchema::screenAnchorName(item->dwellRegion.screenAnchor);
        b.combo(form, QStringLiteral("Screen anchor"), anchors,
                sa.isEmpty() ? QStringLiteral("(none)") : sa, [this](const QString& t) {
                    applyItem(
                        [&](LayoutItem& it) {
                            it.dwellRegion.screenAnchor = LayoutSchema::screenAnchorFromName(t);
                            if (it.dwellRegion.usesScreenAnchor()) {
                                it.unbounded = true;
                                it.hasDwellRegion = true;
                            }
                        },
                        QStringLiteral("Screen anchor"));
                });
        b.dim(form, QStringLiteral("Region X"), item->dwellRegion.x, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.x = v; }, QStringLiteral("Region X"));
        });
        b.dim(form, QStringLiteral("Region Y"), item->dwellRegion.y, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.y = v; }, QStringLiteral("Region Y"));
        });
        b.dim(form, QStringLiteral("Region W"), item->dwellRegion.width, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.width = v; }, QStringLiteral("Region W"));
        });
        b.dim(form, QStringLiteral("Region H"), item->dwellRegion.height, [this](DimSpec v) {
            applyItem([&](LayoutItem& it) { it.dwellRegion.height = v; }, QStringLiteral("Region H"));
        });
        return;
    }

    if (sel.target == EditorTarget::Window) {
        b.heading(form, QStringLiteral("Placement"));
        b.check(form, QStringLiteral("Specified"), d.placement.specified, [this](bool on) {
            applyDoc([&](LayoutDocument& doc) { doc.placement.specified = on; },
                     QStringLiteral("Window specified"));
        });
        b.check(form, QStringLiteral("Hidden"), d.placement.hidden, [this](bool on) {
            applyDoc([&](LayoutDocument& doc) { doc.placement.hidden = on; },
                     QStringLiteral("Window hidden"));
        });
        b.combo(form, QStringLiteral("Anchor"), LayoutSchema::windowAnchorNames(),
                LayoutSchema::windowAnchorName(d.placement.anchor), [this](const QString& t) {
                    applyDoc(
                        [&](LayoutDocument& doc) {
                            doc.placement.specified = true;
                            doc.placement.anchor = LayoutSchema::windowAnchorFromName(t);
                        },
                        QStringLiteral("Anchor"));
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
        b.integer(form, QStringLiteral("Margin px"), d.placement.marginPx, 0, 400, [this](int v) {
            applyDoc([&](LayoutDocument& doc) { doc.placement.marginPx = v; },
                     QStringLiteral("Margin"));
        });
        return;
    }

    if (sel.target == EditorTarget::Grid) {
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
        b.integer(form, QStringLiteral("Margin px"), d.grid.marginPx, 0, 200, [this](int v) {
            applyDoc([&](LayoutDocument& doc) { doc.grid.marginPx = v; },
                     QStringLiteral("Grid margin"));
        });
        b.dim(form, QStringLiteral("Margin X"), d.grid.marginX, [this](DimSpec v) {
            applyDoc([&](LayoutDocument& doc) { doc.grid.marginX = v; }, QStringLiteral("Margin X"));
        });
        b.dim(form, QStringLiteral("Margin Y"), d.grid.marginY, [this](DimSpec v) {
            applyDoc([&](LayoutDocument& doc) { doc.grid.marginY = v; }, QStringLiteral("Margin Y"));
        });
        b.check(form, QStringLiteral("Unit rows"), d.grid.unitRows, [this](bool on) {
            applyDoc([&](LayoutDocument& doc) { doc.grid.unitRows = on; },
                     QStringLiteral("Unit rows"));
        });
        return;
    }

    b.heading(form, QStringLiteral("Document"));
    b.text(form, QStringLiteral("Id"), d.id, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.id = t.trimmed(); }, QStringLiteral("Layout id"));
    });
    b.text(form, QStringLiteral("Name"), d.name, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.name = t; }, QStringLiteral("Name"));
    });
    b.text(form, QStringLiteral("Description"), d.description, [this](const QString& t) {
        applyDoc([&](LayoutDocument& doc) { doc.description = t; }, QStringLiteral("Description"));
    });
    b.combo(form, QStringLiteral("Bounds"), {QStringLiteral("desktop"), QStringLiteral("screen")},
            LayoutSchema::boundsModeName(d.hasBoundsMode ? d.boundsMode : BoundsMode::Desktop),
            [this](const QString& t) {
                applyDoc(
                    [&](LayoutDocument& doc) {
                        doc.hasBoundsMode = true;
                        doc.boundsMode = LayoutSchema::boundsModeFromName(t);
                    },
                    QStringLiteral("Bounds"));
            });
    b.check(form, QStringLiteral("Master"), d.master, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.master = on; }, QStringLiteral("Master"));
    });
    b.check(form, QStringLiteral("Hide until gaze reveal"), d.hideUntilGazeReveal, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.hideUntilGazeReveal = on; },
                 QStringLiteral("Gaze reveal"));
    });
    b.check(form, QStringLiteral("Auto close"), d.autoClose, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.autoClose = on; }, QStringLiteral("Auto close"));
    });
    b.integer(form, QStringLiteral("Idle ms"), d.autoCloseIdleMs, -1, 120000, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.autoCloseIdleMs = v; }, QStringLiteral("Idle ms"));
    });
    b.integer(form, QStringLiteral("Fade ms"), d.autoCloseFadeMs, -1, 30000, [this](int v) {
        applyDoc([&](LayoutDocument& doc) { doc.autoCloseFadeMs = v; }, QStringLiteral("Fade ms"));
    });
}

void LayoutEditorProperties::fillInteraction(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const EditorSelection sel = m_session.selection();
    const LayoutDocument& d = m_session.document();

    if (sel.target == EditorTarget::Item) {
        const LayoutItem* item = m_session.selectedItem();
        if (!item) {
            return;
        }
        b.heading(form, QStringLiteral("Behavior"));
        b.check(form, QStringLiteral("Interactive"), item->interactive, [this](bool on) {
            applyItem([&](LayoutItem& it) { it.interactive = on; }, QStringLiteral("Interactive"));
        });
        b.check(form, QStringLiteral("Dwell exempt"), item->dwellExempt, [this](bool on) {
            applyItem([&](LayoutItem& it) { it.dwellExempt = on; }, QStringLiteral("Dwell exempt"));
        });
        b.check(form, QStringLiteral("Action loop"), item->actionLoop, [this](bool on) {
            applyItem([&](LayoutItem& it) { it.actionLoop = on; }, QStringLiteral("Action loop"));
        });
        b.text(form, QStringLiteral("Active state"), item->activeState, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.activeState = t; }, QStringLiteral("Active state"));
        });
        b.text(form, QStringLiteral("Setting key"), item->settingKey, [this](const QString& t) {
            applyItem([&](LayoutItem& it) { it.settingKey = t; }, QStringLiteral("Setting key"));
        });

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
        addDwellFields(b, form, item->dwell, [this](const QString& undo, const auto& mut) {
            applyItem([&](LayoutItem& it) { mut(it.dwell); }, undo);
        });
        return;
    }

    if (sel.target == EditorTarget::Dwell || sel.target == EditorTarget::Document) {
        addDwellFields(b, form, d.dwell, [this](const QString& undo, const auto& mut) {
            applyDoc([&](LayoutDocument& doc) { mut(doc.dwell); }, undo);
        });
        return;
    }

    b.note(form, QStringLiteral("Select an item or Dwell to edit activation."));
}

} // namespace gazer
