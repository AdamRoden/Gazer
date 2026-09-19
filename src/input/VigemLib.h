#pragma once

#include <QString>
#include <cstdint>
#include <optional>

namespace gazer {

#pragma pack(push, 8)
struct XusbReport {
    std::uint16_t wButtons = 0;
    std::uint8_t bLeftTrigger = 0;
    std::uint8_t bRightTrigger = 0;
    std::int16_t sThumbLX = 0;
    std::int16_t sThumbLY = 0;
    std::int16_t sThumbRX = 0;
    std::int16_t sThumbRY = 0;
};
#pragma pack(pop)

static_assert(sizeof(XusbReport) == 12, "XUSB_REPORT is 12 bytes");

/// Runtime loader for ViGEmClient.dll (no static .lib).
class VigemLib {
public:
    VigemLib();
    ~VigemLib();

    VigemLib(const VigemLib&) = delete;
    VigemLib& operator=(const VigemLib&) = delete;

    [[nodiscard]] bool load(QString* error = nullptr);
    void unload();
    [[nodiscard]] bool isLoaded() const { return m_loaded; }
    [[nodiscard]] QString dllPath() const { return m_path; }
    /// `nullopt` = normal search. Empty string = pretend missing. Otherwise only that path.
    void setOverridePath(const std::optional<QString>& path) { m_overridePath = path; }

    using Client = void*;
    using Target = void*;
    using Error = std::uint32_t;

    Client (*alloc)() = nullptr;
    void (*free)(Client) = nullptr;
    Error (*connect)(Client) = nullptr;
    void (*disconnect)(Client) = nullptr;
    Target (*target_x360_alloc)() = nullptr;
    void (*target_free)(Target) = nullptr;
    Error (*target_add)(Client, Target) = nullptr;
    Error (*target_remove)(Client, Target) = nullptr;
    Error (*target_x360_update)(Client, Target, XusbReport) = nullptr;

    static constexpr Error kOk = 0x20000000u;
    static constexpr Error kBusNotFound = 0xE0000001u;

    [[nodiscard]] static QString errorMessage(Error err);

private:
    bool m_loaded = false;
    QString m_path;
    void* m_handle = nullptr;
    std::optional<QString> m_overridePath;
};

} // namespace gazer
