#pragma once

#include "input/InputService.h"
#include "mapping/MappingTypes.h"

#include <QObject>
#include <QString>

namespace gazer {

/// Loads a mapping profile and resolves commands → InputService.
class MappingEngine final : public QObject {
    Q_OBJECT

public:
    explicit MappingEngine(InputService& input, QObject* parent = nullptr);

    [[nodiscard]] bool loadProfileFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool hasProfile() const { return m_profile.isValid(); }
    [[nodiscard]] const MappingProfile& profile() const { return m_profile; }

    /// Run mapped outputs for a layout command name. Returns false if unknown or inject fail.
    [[nodiscard]] bool runCommand(const QString& commandName, QString* error = nullptr);

    /// Whether speak actions should also type the phrase (from profile).
    [[nodiscard]] bool speakAlsoType() const { return m_profile.speakAlsoType; }
    void setSpeakAlsoType(bool enabled) { m_profile.speakAlsoType = enabled; }

signals:
    void statusMessage(const QString& message);

private:
    InputService& m_input;
    MappingProfile m_profile;
};

} // namespace gazer
