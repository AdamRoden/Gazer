#include "editor/LayoutEditorProperties.h"

#include "editor/LayoutEditorFields.h"
#include "layout/LayoutSchema.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
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
    auto makePage = [](QFormLayout** outForm, QScrollArea** outScroll) {
        auto* page = new QWidget;
        page->setAutoFillBackground(false);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(8, 10, 8, 8);
        form->setSpacing(8);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        *outForm = form;
        auto* scroll = new QScrollArea;
        scroll->setWidget(page);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        *outScroll = scroll;
        return scroll;
    };
    m_tabs->addTab(makePage(&m_pages[0].form, &m_pages[0].scroll), QStringLiteral("Board"));
    m_tabs->addTab(makePage(&m_pages[1].form, &m_pages[1].scroll), QStringLiteral("Window"));
    m_tabs->addTab(makePage(&m_pages[2].form, &m_pages[2].scroll), QStringLiteral("Grid"));
    m_tabs->addTab(makePage(&m_pages[3].form, &m_pages[3].scroll), QStringLiteral("Gaze"));
    root->addWidget(m_tabs, 1);

    connect(&m_session, &LayoutEditorSession::selectionChanged, this, [this]() {
        m_actionStep = 0;
        rebuild();
    });
    connect(&m_session, &LayoutEditorSession::documentChanged, this, [this]() {
        if (m_applying || m_loading) {
            return;
        }
        if (currentShape() != m_shape) {
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
        if (QWidget* w = it->widget()) {
            w->hide();
            w->setParent(nullptr);
            delete w;
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
    const int tab = m_tabs->currentIndex();
    const bool modeChange = item != m_itemMode;
    int scrollY[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (m_pages[i].scroll) {
            scrollY[i] = m_pages[i].scroll->verticalScrollBar()->value();
        }
    }
    syncTabs(item);
    for (auto& page : m_pages) {
        clearLayout(page.form);
    }
    if (item) {
        fillItem(m_pages[0].form);
        fillItemLayout(m_pages[1].form);
        fillItemAction(m_pages[2].form);
    } else {
        fillBoard(m_pages[0].form);
        fillWindow(m_pages[1].form);
        fillGrid(m_pages[2].form);
        fillBoardDwell(m_pages[3].form);
    }
    if (!modeChange) {
        m_tabs->setCurrentIndex(tab);
        for (int i = 0; i < 4; ++i) {
            if (m_pages[i].scroll) {
                m_pages[i].scroll->verticalScrollBar()->setValue(scrollY[i]);
            }
        }
    }
    m_shape = currentShape();
    m_loading = false;
}

LayoutEditorProperties::Shape LayoutEditorProperties::currentShape() const
{
    Shape s;
    const EditorSelection sel = m_session.selection();
    s.item = sel.target == EditorTarget::Item;
    s.itemKey = sel.itemIds.join(QLatin1Char(','));
    const LayoutDocument& d = m_session.document();
    s.windowShown = d.placement.specified && !d.placement.hidden;
    s.autoClose = d.autoClose;
    s.dwellTiming = d.dwell.hasTiming;
    s.dwellProgress = d.dwell.hasProgressStyle;
    s.children = d.children.size();
    s.hook = m_lifecycleHook;
    const QVector<LayoutAction>& hook =
        m_lifecycleHook == 1 ? d.onLoad : (m_lifecycleHook == 2 ? d.onClose : d.onOpen);
    s.hookSteps = hook.size();
    s.actionStep = m_actionStep;
    if (!hook.isEmpty()) {
        s.actionType = int(hook[qBound(0, m_actionStep, hook.size() - 1)].type);
    }
    if (const LayoutItem* it = m_session.selectedItem()) {
        s.unbounded = it->isUnbounded();
        s.role = it->role;
        s.loop = it->actionLoop;
        s.customDwell = it->dwell.sectionPresent;
        s.itemTiming = it->dwell.hasTiming;
        s.itemProgress = it->dwell.hasProgressStyle;
        s.embed = it->isEmbed();
        const QVector<LayoutAction> acts = it->effectiveActions();
        s.itemActions = acts.size();
        if (!acts.isEmpty()) {
            s.actionType = int(acts[qBound(0, m_actionStep, acts.size() - 1)].type);
        }
    }
    return s;
}

void LayoutEditorProperties::rebuildIfNeeded()
{
    if (currentShape() != m_shape) {
        QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
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
    rebuildIfNeeded();
}

void LayoutEditorProperties::applyDoc(const std::function<void(LayoutDocument&)>& fn,
                                      const QString& undoLabel)
{
    m_applying = true;
    m_session.edit(undoLabel, fn);
    m_applying = false;
    rebuildIfNeeded();
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

    b.combo(form, QStringLiteral("Bounds"),
            {QStringLiteral("desktop"), QStringLiteral("screen")},
            LayoutSchema::boundsModeName(d.hasBoundsMode ? d.boundsMode : BoundsMode::Desktop),
            [this](const QString& t) {
                applyDoc(
                    [&](LayoutDocument& doc) {
                        doc.hasBoundsMode = true;
                        doc.boundsMode = LayoutSchema::boundsModeFromName(t);
                    },
                    QStringLiteral("Board bounds"));
            });
    b.note(form, QStringLiteral("Default for the window and free items unless they override it."));
    addChromeFields(b, form, d.style, [this](const QString& undo, const auto& mut) {
        applyDoc([&](LayoutDocument& doc) { mut(doc.style); }, undo);
    });
    b.note(form, QStringLiteral("Default look for cells. Window look is on the Window tab."));

    b.heading(form, QStringLiteral("Owned children"));
    b.note(form, QStringLiteral("Declared instances of other layouts (drawer, quit, …)."));
    for (int i = 0; i < d.children.size(); ++i) {
        const LayoutChildRef& ch = d.children[i];
        b.text(form, QStringLiteral("Slot"), ch.id, [this, i](const QString& t) {
            applyDoc(
                [&](LayoutDocument& doc) {
                    if (i >= 0 && i < doc.children.size()) {
                        doc.children[i].id = t.trimmed();
                    }
                },
                QStringLiteral("Child slot"));
        });
        QStringList ids = m_catalog.layoutIds;
        QStringList labels = m_catalog.layoutLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        if (!ch.layoutId.isEmpty() && !ids.contains(ch.layoutId)) {
            ids.prepend(ch.layoutId);
            labels.prepend(ch.layoutId);
        }
        if (ids.isEmpty()) {
            b.text(form, QStringLiteral("Layout"), ch.layoutId, [this, i](const QString& t) {
                applyDoc(
                    [&](LayoutDocument& doc) {
                        if (i >= 0 && i < doc.children.size()) {
                            doc.children[i].layoutId = t.trimmed();
                        }
                    },
                    QStringLiteral("Child layout"));
            });
        } else {
            b.comboValues(form, QStringLiteral("Layout"), labels, ids, ch.layoutId,
                          [this, i](const QString& t) {
                              applyDoc(
                                  [&](LayoutDocument& doc) {
                                      if (i >= 0 && i < doc.children.size()) {
                                          doc.children[i].layoutId = t;
                                      }
                                  },
                                  QStringLiteral("Child layout"));
                          });
        }
        QStringList whenLabels = {QStringLiteral("(always)"), QStringLiteral("expanded"),
                                  QStringLiteral("!expanded"), QStringLiteral("quitConfirm"),
                                  QStringLiteral("!quitConfirm"), QStringLiteral("dwellSuspend"),
                                  QStringLiteral("!dwellSuspend")};
        QStringList whenVals = visibleWhenChoices();
        if (!ch.visibleWhen.isEmpty() && !whenVals.contains(ch.visibleWhen)) {
            whenVals.prepend(ch.visibleWhen);
            whenLabels.prepend(ch.visibleWhen);
        }
        b.comboValues(form, QStringLiteral("Visible when"), whenLabels, whenVals, ch.visibleWhen,
                      [this, i](const QString& t) {
                          applyDoc(
                              [&](LayoutDocument& doc) {
                                  if (i >= 0 && i < doc.children.size()) {
                                      doc.children[i].visibleWhen = t;
                                  }
                              },
                              QStringLiteral("Child visible when"));
                      });
        auto* rm = new QPushButton(QStringLiteral("Remove child"));
        QObject::connect(rm, &QPushButton::clicked, this, [this, i]() {
            applyDoc(
                [&](LayoutDocument& doc) {
                    if (i >= 0 && i < doc.children.size()) {
                        doc.children.removeAt(i);
                    }
                },
                QStringLiteral("Remove child"));
        });
        form->addRow(rm);
    }
    auto* addChild = new QPushButton(QStringLiteral("Add child"));
    QObject::connect(addChild, &QPushButton::clicked, this, [this]() {
        applyDoc(
            [&](LayoutDocument& doc) {
                LayoutChildRef ch;
                ch.id = QStringLiteral("child_%1").arg(doc.children.size() + 1);
                if (!m_catalog.layoutIds.isEmpty()) {
                    ch.layoutId = m_catalog.layoutIds.first();
                }
                doc.children.push_back(ch);
            },
            QStringLiteral("Add child"));
    });
    form->addRow(addChild);

    b.heading(form, QStringLiteral("Lifecycle"));
    b.combo(form, QStringLiteral("Hook"),
            {QStringLiteral("onOpen"), QStringLiteral("onLoad"), QStringLiteral("onClose")},
            m_lifecycleHook == 1   ? QStringLiteral("onLoad")
            : m_lifecycleHook == 2 ? QStringLiteral("onClose")
                                   : QStringLiteral("onOpen"),
            [this](const QString& t) {
                m_lifecycleHook = t == QLatin1String("onLoad")   ? 1
                                  : t == QLatin1String("onClose") ? 2
                                                                 : 0;
                QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
            });
    const QVector<LayoutAction>* hookActs = &d.onOpen;
    if (m_lifecycleHook == 1) {
        hookActs = &d.onLoad;
    } else if (m_lifecycleHook == 2) {
        hookActs = &d.onClose;
    }
    addActionSeriesFields(b, form, *hookActs, d.name, m_catalog, m_actionStep,
                          [this](int step) {
                              m_actionStep = step;
                              QTimer::singleShot(0, this, &LayoutEditorProperties::rebuild);
                          },
                          [this](QVector<LayoutAction> next) {
                              const int hook = m_lifecycleHook;
                              applyDoc(
                                  [&](LayoutDocument& doc) {
                                      if (hook == 1) {
                                          doc.onLoad = next;
                                      } else if (hook == 2) {
                                          doc.onClose = next;
                                      } else {
                                          doc.onOpen = next;
                                      }
                                  },
                                  QStringLiteral("Lifecycle"));
                          });
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
    b.dim(form, QStringLiteral("Inset X"), d.grid.marginX, [this](DimSpec v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.marginX = v; },
                 QStringLiteral("Grid margin X"));
    });
    b.dim(form, QStringLiteral("Inset Y"), d.grid.marginY, [this](DimSpec v) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.marginY = v; },
                 QStringLiteral("Grid margin Y"));
    });
    b.note(form, QStringLiteral("X/Y insets override Inset px when set. Bare % is of board size."));
    b.check(form, QStringLiteral("Unit rows (keyboard widths)"), d.grid.unitRows, [this](bool on) {
        applyDoc([&](LayoutDocument& doc) { doc.grid.unitRows = on; },
                 QStringLiteral("Unit rows"));
    });
    b.note(form, QStringLiteral("On: each row sizes keys by Key width (u). Auto-on if any key has u > 0."));
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
    b.text(form, QStringLiteral("Id"), item->id, [this, old = item->id](const QString& t) {
        const QString next = t.trimmed();
        if (next.isEmpty() || next == old) {
            return;
        }
        if (m_session.itemById(next)) {
            m_session.notify(QStringLiteral("Id already in use: %1").arg(next));
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
    {
        QStringList ids = iconChoices();
        QStringList labels = ids;
        labels[0] = QStringLiteral("(none)");
        if (!item->icon.isEmpty() && !ids.contains(item->icon)) {
            ids.insert(1, item->icon);
            labels.insert(1, item->icon);
        }
        b.comboValues(form, QStringLiteral("Icon"), labels, ids, item->icon,
                      [this](const QString& t) {
                          applyItem([&](LayoutItem& it) { it.icon = t; }, QStringLiteral("Icon"));
                      });
    }
    b.text(form, QStringLiteral("Cluster"), item->cluster, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.cluster = t.trimmed(); }, QStringLiteral("Cluster"));
    });
    b.combo(form, QStringLiteral("Cluster slot"),
            {QStringLiteral(""), QStringLiteral("dec"), QStringLiteral("value"),
             QStringLiteral("inc"), QStringLiteral("edit")},
            item->clusterSlot, [this](const QString& t) {
                applyItem([&](LayoutItem& it) { it.clusterSlot = t; },
                          QStringLiteral("Cluster slot"));
            });
    b.note(form, QStringLiteral("segment.* = pill group. stepper.* = NumberBox (dec/value/inc)."));
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
        rebuildIfNeeded();
    });
}

void LayoutEditorProperties::fillItemLayout(QFormLayout* form)
{
    PropertyBinder b{this, &m_loading};
    const LayoutItem* item = m_session.selectedItem();
    if (!item) {
        return;
    }
    b.heading(form, QStringLiteral("Placement"));
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
        b.combo(form, QStringLiteral("Bounds"),
                {QStringLiteral("desktop"), QStringLiteral("screen")},
                LayoutSchema::boundsModeName(item->dwellRegion.hasBoundsMode
                                                 ? item->dwellRegion.boundsMode
                                                 : BoundsMode::Desktop),
                [this](const QString& t) {
                    applyItem(
                        [&](LayoutItem& it) {
                            it.dwellRegion.hasBoundsMode = true;
                            it.dwellRegion.boundsMode = LayoutSchema::boundsModeFromName(t);
                        },
                        QStringLiteral("Item bounds"));
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

    b.check(form, QStringLiteral("Visible"), item->visible, [this](bool on) {
        applyItem([&](LayoutItem& it) { it.visible = on; }, QStringLiteral("Visible"));
    });
    {
        QStringList whenVals = visibleWhenChoices();
        QStringList whenLabels = {QStringLiteral("(always)"), QStringLiteral("expanded"),
                                  QStringLiteral("!expanded"), QStringLiteral("quitConfirm"),
                                  QStringLiteral("!quitConfirm"), QStringLiteral("dwellSuspend"),
                                  QStringLiteral("!dwellSuspend")};
        if (!item->visibleWhen.isEmpty() && !whenVals.contains(item->visibleWhen)) {
            whenVals.prepend(item->visibleWhen);
            whenLabels.prepend(item->visibleWhen);
        }
        b.comboValues(form, QStringLiteral("Visible when"), whenLabels, whenVals, item->visibleWhen,
                      [this](const QString& t) {
                          applyItem([&](LayoutItem& it) { it.visibleWhen = t; },
                                    QStringLiteral("Visible when"));
                      });
    }
    b.combo(form, QStringLiteral("Embed"),
            {QStringLiteral("cell"), QStringLiteral("layout")},
            item->isEmbed() ? QStringLiteral("layout") : QStringLiteral("cell"),
            [this](const QString& t) {
                applyItem(
                    [&](LayoutItem& it) {
                        if (t == QLatin1String("layout")) {
                            it.type = QStringLiteral("layout");
                        } else {
                            it.type.clear();
                            it.embedLayoutId.clear();
                        }
                    },
                    QStringLiteral("Embed"));
            });
    if (item->isEmbed()) {
        QStringList ids = m_catalog.layoutIds;
        QStringList labels = m_catalog.layoutLabels;
        if (labels.size() != ids.size()) {
            labels = ids;
        }
        if (!item->embedLayoutId.isEmpty() && !ids.contains(item->embedLayoutId)) {
            ids.prepend(item->embedLayoutId);
            labels.prepend(item->embedLayoutId);
        }
        if (ids.isEmpty()) {
            b.text(form, QStringLiteral("Layout id"), item->embedLayoutId, [this](const QString& t) {
                applyItem([&](LayoutItem& it) { it.embedLayoutId = t.trimmed(); },
                          QStringLiteral("Embed layout"));
            });
        } else {
            b.comboValues(form, QStringLiteral("Layout id"), labels, ids, item->embedLayoutId,
                          [this](const QString& t) {
                              applyItem([&](LayoutItem& it) { it.embedLayoutId = t; },
                                        QStringLiteral("Embed layout"));
                          });
        }
    }
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
    b.text(form, QStringLiteral("Active-state key"), item->activeState, [this](const QString& t) {
        applyItem([&](LayoutItem& it) { it.activeState = t; }, QStringLiteral("Active state"));
    });
    b.note(form, QStringLiteral("Accent on when this key is true (dwellSuspend, setting.*, loop.id, !…)."));

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
                              rebuildIfNeeded();
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
