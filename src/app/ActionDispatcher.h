#pragma once

#include "app/GazerServices.h"
#include "layout/PageTypes.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

/// Routes page actions using GazerServices.
class ActionDispatcher final : public QObject {
    Q_OBJECT

public:
    explicit ActionDispatcher(GazerServices& services, QObject* parent = nullptr);

    void dispatchPage(const QVector<PageAction>& actions, const QString& sourcePageId,
                      const QString& targetId = {});

signals:
    void statusMessage(const QString& message);

private:
    [[nodiscard]] bool dispatchClick(const PageAction& a, QString* error);

    GazerServices& m_svc;
};

} // namespace gazer
