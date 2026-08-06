#pragma once

#include "app/GazerServices.h"
#include "layout/LayoutTypes.h"

#include <QObject>
#include <QString>

namespace gazer {

/// Routes layout item actions using GazerServices.
class ActionDispatcher final : public QObject {
    Q_OBJECT

public:
    explicit ActionDispatcher(GazerServices& services, QObject* parent = nullptr);

    void dispatch(LayoutItem item, const QString& sourceInstanceId);

signals:
    void statusMessage(const QString& message);

private:
    GazerServices& m_svc;
};

} // namespace gazer
