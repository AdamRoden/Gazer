#pragma once

#include "layout/PageTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

class GazerServices;

/// Routes page actions using GazerServices.
class ActionDispatcher final : public QObject {
    Q_OBJECT

public:
    explicit ActionDispatcher(GazerServices& services, QObject* parent = nullptr);

    void dispatchPage(const QVector<PageAction>& actions, const QString& sourcePageId,
                      const QString& targetId = {});
    /// Same handlers as `dispatchPage`. After OpenPage / HostPage, later ShowLayers use
    /// the new top page (cell ShowLayers still retargets the source page).
    void dispatchInbound(const QVector<PageAction>& actions);

signals:
    void statusMessage(const QString& message);

private:
    [[nodiscard]] bool dispatchClick(const PageAction& a, QString* error);

    GazerServices& m_svc;
};

} // namespace gazer
