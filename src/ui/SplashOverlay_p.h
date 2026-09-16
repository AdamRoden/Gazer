#pragma once

#include <QtGlobal>

namespace gazer {
namespace splash {

inline constexpr int kTickMs = 16;
inline constexpr int kNavDwellMs = 650;
inline constexpr double kBlurRadius = 28.0;
inline constexpr double kGazeRadius = 32.0;
inline constexpr double kWantedScale = 0.85;
inline constexpr int kMiniTopPad = 16;
inline constexpr int kChipW = 200;
inline constexpr int kChipH = 100;
inline constexpr int kChipGap = 14;
inline constexpr int kChipRadius = 16;
inline constexpr int kStepCount = 6;

inline constexpr char kPage[] = "main";
inline constexpr char kShow[] = "show";
inline constexpr char kHide[] = "hide";
inline constexpr char kSleep[] = "sleep";

enum class Scale { One, Fitted };
enum class Mark { None, Center, MenuChip, MenuDwell, SleepChip, SleepDwell, Drawer, AmberFrame };

struct Callout {
    const char* targetId;
    const char* title;
    const char* body;
};

inline constexpr Callout kCallouts[] = {
    {"open_keyboard", "Keyboard", "Type with your eyes"},
    {"open_compose", "Speak", "Say a phrase out loud"},
    {"open_mouse", "Mouse", "Move and click the cursor"},
    {"open_assist", "Assist", "Scroll and extra tools"},
    {"open_settings", "Settings", "Timing, theme, and this tour"},
};

/// One row per SplashOverlay::Phase. introMs plays the lerp, then the row waits
/// (autoMs == 0) or auto-advances (Fade).
struct Spec {
    int introMs = 0;
    int autoMs = 0;
    int recaptureMs = -1;
    int step = 1;
    const char* title = "";
    bool mini = false;
    Scale scaleFrom = Scale::One;
    Scale scaleTo = Scale::One;
    bool openMenu = false;
    bool closeMenu = false;
    int suspend = -1;
    Mark gazeFrom = Mark::None;
    Mark gazeTo = Mark::None;
    bool lerpGaze = false;
    Mark captionAt = Mark::None;
    const char* capTitle = "";
    const char* capBody = "";
    Mark dwellZone = Mark::None;
    Mark chipRing = Mark::None;
    bool welcome = false;
    bool callouts = false;
    bool gazeCue = false;
    bool captionBelow = false;
};

inline constexpr Spec kPhases[] = {
    {
        .recaptureMs = 480,
        .step = 1,
        .title = "Welcome",
        .closeMenu = true,
        .suspend = 0,
        .gazeFrom = Mark::Center,
        .gazeTo = Mark::Center,
        .welcome = true,
    },
    {
        .introMs = 900,
        .recaptureMs = 480,
        .step = 2,
        .title = "Menu",
        .mini = true,
        .scaleFrom = Scale::One,
        .scaleTo = Scale::Fitted,
        .closeMenu = true,
        .suspend = 0,
        .gazeFrom = Mark::Center,
        .gazeTo = Mark::MenuDwell,
        .lerpGaze = true,
        .captionAt = Mark::MenuDwell,
        .capTitle = "Menu",
        .capBody = "Look just below the screen to trigger the chip to show.",
        .dwellZone = Mark::MenuDwell,
        .chipRing = Mark::MenuChip,
        .welcome = true,
        .gazeCue = true,
    },
    {
        .recaptureMs = 420,
        .step = 3,
        .title = "Drawer",
        .mini = true,
        .scaleFrom = Scale::Fitted,
        .scaleTo = Scale::Fitted,
        .openMenu = true,
        .suspend = 0,
        .gazeFrom = Mark::MenuChip,
        .gazeTo = Mark::MenuChip,
        .captionAt = Mark::MenuChip,
        .capTitle = "Menu",
        .capBody = "That look opened the menu drawer.",
        .chipRing = Mark::MenuChip,
        .gazeCue = true,
    },
    {
        .introMs = 850,
        .recaptureMs = 420,
        .step = 4,
        .title = "Sleep",
        .mini = true,
        .scaleFrom = Scale::Fitted,
        .scaleTo = Scale::Fitted,
        .suspend = 0,
        .gazeFrom = Mark::MenuChip,
        .gazeTo = Mark::SleepDwell,
        .lerpGaze = true,
        .captionAt = Mark::SleepDwell,
        .capTitle = "Sleep",
        .capBody = "Same idea: look below the screen to pause dwell.",
        .dwellZone = Mark::SleepDwell,
        .chipRing = Mark::SleepChip,
        .gazeCue = true,
    },
    {
        .step = 5,
        .title = "Pause",
        .mini = true,
        .scaleFrom = Scale::Fitted,
        .scaleTo = Scale::Fitted,
        .suspend = 1,
        .gazeFrom = Mark::SleepChip,
        .gazeTo = Mark::SleepChip,
        .captionAt = Mark::AmberFrame,
        .capTitle = "Dwell paused",
        .capBody = "The amber frame means Gazer is waiting.",
        .captionBelow = true,
    },
    {
        .step = 6,
        .title = "Tools",
        .mini = true,
        .scaleFrom = Scale::Fitted,
        .scaleTo = Scale::Fitted,
        .suspend = 0,
        .gazeFrom = Mark::Drawer,
        .gazeTo = Mark::Drawer,
        .chipRing = Mark::Drawer,
        .callouts = true,
    },
    {
        .autoMs = 480,
        .step = 6,
        .scaleFrom = Scale::Fitted,
        .scaleTo = Scale::One,
        .suspend = 0,
        .gazeFrom = Mark::Drawer,
        .gazeTo = Mark::Drawer,
    },
    {
        .step = 6,
        .suspend = 0,
    },
};

static_assert(sizeof(kPhases) / sizeof(kPhases[0]) == 8, "phase table matches Phase enumerators");

[[nodiscard]] inline const Spec& spec(int phase)
{
    const int i = qBound(0, phase, int(sizeof(kPhases) / sizeof(kPhases[0])) - 1);
    return kPhases[i];
}

} // namespace splash
} // namespace gazer
