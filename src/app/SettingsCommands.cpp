#include "app/SettingsUi.h"

#include "app/CommandRegistry.h"
#include "layout/LayoutInstanceManager.h"

#include <QString>

namespace gazer {

void SettingsUi::registerCommands()
{
    auto editCmd = [this](const QString& key) {
        return [this, key](QString* error) { return openNumericEditor(key, error); };
    };

    for (const QString& key : AppSettings::numericKeys()) {
        m_commands.registerBuiltin(QStringLiteral("settings.edit.%1").arg(key), editCmd(key));
    }

    m_commands.registerBuiltin(QStringLiteral("settings.numpad.noop"),
                               [](QString*) { return true; });
    for (int d = 0; d <= 9; ++d) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.numpad.digit.%1").arg(d), [this, d](QString*) {
                numpadAppend(QString::number(d));
                return true;
            });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.period"), [this](QString*) {
        numpadAppend(QStringLiteral("."));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.comma"), [this](QString*) {
        numpadAppend(QStringLiteral(","));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.backspace"), [this](QString*) {
        numpadBackspace();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.clear"), [this](QString*) {
        numpadClear();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.reset"), [this](QString*) {
        numpadReset();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.minus"), [this](QString*) {
        numpadMinus();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.save"),
                               [this](QString* e) { return numpadSave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.cancel"),
                               [this](QString* e) { return numpadCancel(e); });

    auto nudge = [this](const QString& key, int dir) {
        return [this, key, dir](QString*) {
            if (!m_settings.nudge(key, dir)) {
                return false;
            }
            apply(true);
            notifyStatus(QStringLiteral("%1 = %2")
                             .arg(AppSettings::settingTitle(key), m_settings.displayValue(key)));
            return true;
        };
    };
    for (const QString& key : AppSettings::numericKeys()) {
        m_commands.registerBuiltin(QStringLiteral("settings.nudge.%1.dec").arg(key), nudge(key, -1));
        m_commands.registerBuiltin(QStringLiteral("settings.nudge.%1.inc").arg(key), nudge(key, +1));
    }

    auto toggleBool = [this](bool AppSettings::*member, const QString& label) {
        return [this, member, label](QString*) {
            m_settings.*member = !(m_settings.*member);
            apply(true);
            notifyStatus(QStringLiteral("%1: %2")
                             .arg(label, (m_settings.*member) ? QStringLiteral("ON")
                                                              : QStringLiteral("OFF")));
            return true;
        };
    };
    const struct {
        const char* cmd;
        bool AppSettings::* member;
        const char* label;
    } boolToggles[] = {
        {"settings.progress.radial.toggle", &AppSettings::progressRadial, "Radial"},
        {"settings.progress.fill.toggle", &AppSettings::progressFill, "Fill"},
        {"settings.progress.border.toggle", &AppSettings::progressBorder, "Border"},
        {"settings.mouseProgress.radial.toggle", &AppSettings::mouseProgressRadial, "Mouse radial"},
        {"settings.mouseProgress.fill.toggle", &AppSettings::mouseProgressFill, "Mouse fill"},
        {"settings.mouseProgress.border.toggle", &AppSettings::mouseProgressBorder, "Mouse border"},
        {"settings.flash.toggle", &AppSettings::flashOnComplete, "Completion flash"},
        {"settings.lts.placeCursor.toggle", &AppSettings::ltsPlaceCursorFirst,
         "LTS place cursor first"},
        {"settings.session.autoCollapse.toggle", &AppSettings::autoCollapseMain,
         "Auto-collapse Main"},
        {"settings.session.startDocked.toggle", &AppSettings::startDocked, "Start docked"},
        {"settings.speech.alsoType.toggle", &AppSettings::speakAlsoType, "Speak also types"},
    };
    for (const auto& t : boolToggles) {
        m_commands.registerBuiltin(QLatin1String(t.cmd),
                                   toggleBool(t.member, QLatin1String(t.label)));
    }

    for (const char* ck : kColorKeys) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.edit.color.%1").arg(QLatin1String(ck)),
            [this, ck](QString* error) { return openColorPicker(QLatin1String(ck), error); });
        m_commands.registerBuiltin(
            QStringLiteral("settings.color.use.%1").arg(QLatin1String(ck)), [this, ck](QString*) {
                colorUseSaved(QLatin1String(ck));
                return true;
            });
    }
    for (const char* ch : {"h", "s", "v", "r", "g", "b", "a"}) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.color.nudge.%1.dec").arg(QLatin1String(ch)),
            [this, ch](QString*) {
                colorNudge(QLatin1String(ch), -1);
                return true;
            });
        m_commands.registerBuiltin(
            QStringLiteral("settings.color.nudge.%1.inc").arg(QLatin1String(ch)),
            [this, ch](QString*) {
                colorNudge(QLatin1String(ch), +1);
                return true;
            });
        m_commands.registerBuiltin(
            QStringLiteral("settings.color.edit.%1").arg(QLatin1String(ch)),
            [this, ch](QString* error) { return colorEditChannel(QLatin1String(ch), error); });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.color.editHex"),
                               [this](QString* e) { return openHexEditor(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.color.save"),
                               [this](QString* e) { return colorSave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.color.cancel"), [this](QString*) {
        closeColorPicker();
        notifyStatus(QStringLiteral("Color pick cancelled"));
        return true;
    });
    for (QChar d : QStringLiteral("0123456789ABCDEF")) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.hex.digit.%1").arg(d), [this, d](QString*) {
                hexAppend(d);
                return true;
            });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.hex.backspace"), [this](QString*) {
        hexBackspace();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.hex.clear"), [this](QString*) {
        if (m_hexActive) {
            m_hexBuffer.clear();
            refreshHexEditor();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.hex.save"),
                               [this](QString* e) { return hexSave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.hex.cancel"),
                               [this](QString* e) { return hexCancel(e); });

    for (int i = 0; i < 12; ++i) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.array.nudge.%1.dec").arg(i), [this, i](QString*) {
                arrayNudge(i, -1);
                return true;
            });
        m_commands.registerBuiltin(
            QStringLiteral("settings.array.nudge.%1.inc").arg(i), [this, i](QString*) {
                arrayNudge(i, +1);
                return true;
            });
        m_commands.registerBuiltin(QStringLiteral("settings.array.edit.%1").arg(i),
                                   [this, i](QString* error) { return arrayEditIndex(i, error); });
        m_commands.registerBuiltin(
            QStringLiteral("settings.array.del.%1").arg(i), [this, i](QString*) {
                arrayRemove(i);
                return true;
            });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.array.decAll"), [this](QString*) {
        arrayNudgeAll(-1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.array.incAll"), [this](QString*) {
        arrayNudgeAll(+1);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.array.reset"), [this](QString*) {
        arrayReset();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.array.add"), [this](QString*) {
        arrayAdd();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.array.save"),
                               [this](QString* e) { return arraySave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.array.cancel"),
                               [this](QString* e) { return arrayCancel(e); });
    m_commands.registerBuiltin(
        QStringLiteral("settings.edit.dwellSequence"),
        [this](QString* e) { return openArrayEditor(QStringLiteral("dwellSequence"), e); });

    const struct {
        const char* cmd;
        int preset;
        const char* status;
    } dwellPresets[] = {
        {"settings.dwell.slow", 0, "Dwell: Slow (~1000 ms)"},
        {"settings.dwell.normal", 1, "Dwell: Normal (800,600,400,200,100,50)"},
        {"settings.dwell.fast", 2, "Dwell: Fast (~450 ms)"},
    };
    for (const auto& p : dwellPresets) {
        m_commands.registerBuiltin(QLatin1String(p.cmd), [this, p](QString*) {
            if (m_mutate) {
                m_mutate([preset = p.preset](AppSettings& s) { s.setDwellPreset(preset); },
                         QLatin1String(p.status));
            }
            return true;
        });
    }

    const struct {
        const char* cmd;
        int profile;
        const char* status;
    } magFollow[] = {
        {"settings.mag.follow.sticky", 0, "Mag follow: Sticky"},
        {"settings.mag.follow.balanced", 1, "Mag follow: Balanced"},
        {"settings.mag.follow.snappy", 2, "Mag follow: Snappy"},
    };
    for (const auto& p : magFollow) {
        m_commands.registerBuiltin(QLatin1String(p.cmd), [this, p](QString*) {
            if (m_mutate) {
                m_mutate([profile = p.profile](AppSettings& s) { s.setMagFollowProfile(profile); },
                         QLatin1String(p.status));
            }
            return true;
        });
    }

    m_commands.registerBuiltin(QStringLiteral("settings.tracker.auto"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.trackerPref = 0; },
                     QStringLiteral("Tracker: Auto Tobii (restart to apply)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.tracker.mouse"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.trackerPref = 1; },
                     QStringLiteral("Tracker: Mouse only (restart to apply)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.reset"), [this](QString*) {
        if (m_reset) {
            m_reset();
        }
        return true;
    });
}

} // namespace gazer
