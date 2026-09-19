#pragma once

#include <QString>
#include <cstdint>
#include <memory>
#include <optional>

namespace gazer {

/// Virtual XInput-style Xbox 360 pad via ViGEmBus / ViGEmClient.dll.
class VirtualGamepad {
public:
    struct Report {
        std::uint16_t buttons = 0;
        std::uint8_t leftTrigger = 0;
        std::uint8_t rightTrigger = 0;
        std::int16_t leftX = 0;
        std::int16_t leftY = 0;
        std::int16_t rightX = 0;
        std::int16_t rightY = 0;
    };

    VirtualGamepad();
    ~VirtualGamepad();

    VirtualGamepad(const VirtualGamepad&) = delete;
    VirtualGamepad& operator=(const VirtualGamepad&) = delete;

    /// Connect / plug in the virtual pad. No-op when already connected.
    [[nodiscard]] bool ensureConnected(QString* error = nullptr);

    [[nodiscard]] bool pressButton(const QString& button, QString* error = nullptr);
    [[nodiscard]] bool releaseButton(const QString& button, QString* error = nullptr);
    /// axis: lx ly rx ry lt rt — sticks [-1, 1], triggers 0..1 (clamped).
    [[nodiscard]] bool setAxis(const QString& axis, double value, QString* error = nullptr);

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] QString backendName() const;
    [[nodiscard]] Report report() const;

    /// Skip the DLL; keep a local report. Test hook.
    void setDryRun(bool on);
    /// When set, skip system discovery (`empty` pretends the DLL is missing). Test hook.
    void setOverrideDll(const std::optional<QString>& path);

    [[nodiscard]] static bool lookupButton(const QString& name, std::uint16_t* bit,
                                           QString* error = nullptr);

private:
    [[nodiscard]] bool pushReport(QString* error);
    void teardown();

    struct Impl;
    std::unique_ptr<Impl> m;
};

} // namespace gazer
