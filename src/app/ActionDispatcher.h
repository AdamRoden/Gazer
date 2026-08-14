#pragma once

#include "app/GazerServices.h"
#include "layout/LayoutTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

/// Routes layout item actions using GazerServices.
class ActionDispatcher final : public QObject {
    Q_OBJECT

public:
    explicit ActionDispatcher(GazerServices& services, QObject* parent = nullptr);

    /// Dispatch item (series, optional actionLoop toggle).
    void dispatchItem(const LayoutItem& item, const QString& sourceInstanceId);

    /// Run an ordered list of actions once (no loop). Used by lifecycle hooks and series.
    void dispatchAll(const QVector<LayoutAction>& actions, const QString& sourceInstanceId);

    /// Run one action immediately.
    void dispatchOne(const LayoutAction& action, const QString& sourceInstanceId,
                     const QString& itemId = {});

signals:
    void statusMessage(const QString& message);

private:
    GazerServices& m_svc;
};

} // namespace gazer
