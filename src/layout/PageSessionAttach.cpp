#include "layout/PageSession.h"

#include "layout/PageDim.h"
#include "ui/PageHostWindow.h"

#include <QPointF>
#include <QTransform>

namespace gazer {

bool PageSession::placeAttachedCenter(const QString& id, const QPoint& screenCenter,
                                      bool leaveGate)
{
    AttachedPage* att = nullptr;
    for (AttachedPage& a : m_attached) {
        if (a.doc.id == id) {
            att = &a;
            break;
        }
    }
    if (!att) {
        return false;
    }
    const QPointF c = frame().desktop.center();
    const PageDim ox = PageDim::pixels(double(screenCenter.x()) - c.x());
    const PageDim oy = PageDim::pixels(double(screenCenter.y()) - c.y());
    auto pin = [&](bool& desktopMode, PageAnchor& anchor, PageDimPair& offset) {
        desktopMode = true;
        anchor = PageAnchor::Center;
        offset.x = ox;
        offset.y = oy;
    };
    for (PageGrid& g : att->doc.grids) {
        pin(g.desktopMode, g.anchor, g.offset);
    }
    for (PageZone& z : att->doc.zones) {
        pin(z.desktopMode, z.anchor, z.offset);
    }
    leaveGaze();
    rebuild();
    raise();
    if (leaveGate) {
        armLeaveGate(id);
    } else {
        clearLeaveGate();
    }
    emit sessionChanged();
    return true;
}

void PageSession::gateHover(const QString& pageId)
{
    if (pageId.isEmpty()) {
        leaveGaze();
        return;
    }
    armLeaveGate(pageId);
}

void PageSession::armLeaveGate(const QString& pageId)
{
    m_leaveGatePage = pageId;
    m_leaveGateKey.clear();
    if (m_lastGaze.valid && !pageId.isEmpty()) {
        const QTransform xf = hitXf();
        const PageTarget* hit =
            PageHit::at(m_targets, m_lastGaze.toPointF(), m_drawerScale, {}, m_gridPaints, &xf);
        if (hit && hit->interactive && hit->pageId == pageId) {
            m_leaveGateKey = sessionKey(*hit);
        }
    }
    leaveGaze();
}

void PageSession::clearLeaveGate()
{
    m_leaveGatePage.clear();
    m_leaveGateKey.clear();
}

bool PageSession::blockedByLeaveGate(const PageTarget* hit)
{
    if (m_leaveGatePage.isEmpty()) {
        return false;
    }
    const bool onPage = hit && hit->interactive && hit->pageId == m_leaveGatePage;
    if (m_leaveGateKey.isEmpty()) {
        if (!onPage) {
            clearLeaveGate();
            return false;
        }
        m_leaveGateKey = sessionKey(*hit);
        return true;
    }
    if (onPage && sessionKey(*hit) == m_leaveGateKey) {
        return true;
    }
    clearLeaveGate();
    return false;
}

} // namespace gazer
