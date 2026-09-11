#include "app/SettingsUi.h"

#include "app/CommandRegistry.h"
#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "ui/MaterialPalette.h"
#include "ui/PickStyle.h"
#include "ui/ThemeScheme.h"

#include <QString>
#include <initializer_list>

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

    auto afterToggle = [this](const QString& label, bool on) {
        if (m_color.active) {
            refreshColorPicker();
        }
        notifyStatus(QStringLiteral("%1: %2")
                         .arg(label, on ? QStringLiteral("ON") : QStringLiteral("OFF")));
        return true;
    };
    for (const AppSettings::StyleToggle& spec : AppSettings::kStyleToggles) {
        m_commands.registerBuiltin(QLatin1String(spec.command), [this, spec, afterToggle](QString*) {
            m_settings.styleFlag(spec) = !m_settings.styleFlag(spec);
            apply(true);
            return afterToggle(QLatin1String(spec.label), m_settings.styleFlag(spec));
        });
    }
    auto toggleBool = [this, afterToggle](bool AppSettings::*member, const QString& label) {
        return [this, member, label, afterToggle](QString*) {
            m_settings.*member = !(m_settings.*member);
            apply(true);
            return afterToggle(label, m_settings.*member);
        };
    };
    const struct {
        const char* cmd;
        bool AppSettings::* member;
        const char* label;
    } boolToggles[] = {
        {"settings.session.autoCollapse.toggle", &AppSettings::autoCollapseMain,
         "Auto-collapse Main"},
        {"settings.session.startDocked.toggle", &AppSettings::startDocked, "Start docked"},
        {"settings.session.layoutAutoClose.toggle", &AppSettings::layoutAutoClose,
         "Auto-close boards"},
    };
    for (const auto& t : boolToggles) {
        m_commands.registerBuiltin(QLatin1String(t.cmd),
                                   toggleBool(t.member, QLatin1String(t.label)));
    }

    m_commands.registerBuiltin(QStringLiteral("settings.speech.editKey"),
                               [this](QString* e) { return openSpeechKeyBoard(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.clearKey"),
                               [this](QString* e) { return clearSpeechKey(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.key.save"),
                               [this](QString* e) { return speechKeySave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.key.cancel"), [this](QString*) {
        speechKeyCancel();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.key.clear"), [this](QString*) {
        speechKeyClear();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.key.paste"), [this](QString*) {
        speechKeyPaste();
        return true;
    });

    m_commands.registerBuiltin(QStringLiteral("settings.flash.foreground"),
                               [this](QString* e) { return openFlashForeground(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.flash.custom"),
                               [this](QString* e) { return openFlashCustom(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.opacity.scrub"),
                               [this](QString*) { return beginSliderScrub(QStringLiteral("opacity")); });
    m_commands.registerBuiltin(QStringLiteral("settings.opacity.nudge.dec"), [this](QString*) {
        opacityNudge(-5);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.opacity.nudge.inc"), [this](QString*) {
        opacityNudge(+5);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.opacity.save"),
                               [this](QString* e) { return opacitySave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.opacity.cancel"), [this](QString*) {
        closeOpacityEditor();
        notifyStatus(QStringLiteral("Flash opacity cancelled"));
        return true;
    });

    for (const char* ck : kColorKeys) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.edit.color.%1").arg(QLatin1String(ck)),
            [this, ck](QString* error) { return openColorPicker(QLatin1String(ck), error); });
    }
    for (const char* ch : {"h", "s", "l", "r", "g", "b", "a"}) {
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
        m_commands.registerBuiltin(
            QStringLiteral("settings.color.scrub.%1").arg(QLatin1String(ch)),
            [this, ch](QString*) { return beginSliderScrub(QLatin1String(ch)); });
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
    m_commands.registerBuiltin(
        QStringLiteral("settings.edit.rapidDwellSequence"),
        [this](QString* e) { return openArrayEditor(QStringLiteral("rapidDwellSequence"), e); });

    struct IntChoice {
        const char* cmd;
        int value;
        const char* status;
    };
    auto registerIntChoices = [this](std::initializer_list<IntChoice> choices,
                                     void (AppSettings::*setter)(int)) {
        for (const auto& p : choices) {
            m_commands.registerBuiltin(QLatin1String(p.cmd), [this, p, setter](QString*) {
                if (m_mutate) {
                    m_mutate([setter, v = p.value](AppSettings& s) { (s.*setter)(v); },
                             QLatin1String(p.status));
                }
                return true;
            });
        }
    };
    registerIntChoices(
        {
            {"settings.dwell.slow", 0, "Dwell: Slow"},
            {"settings.dwell.normal", 1, "Dwell: Normal"},
            {"settings.dwell.fast", 2, "Dwell: Fast"},
            {"settings.dwell.custom", 3, "Dwell: Custom"},
        },
        &AppSettings::setDwellPreset);
    m_commands.registerBuiltin(QStringLiteral("settings.dwell.custom.save"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.saveDwellCustom(); },
                     QStringLiteral("Custom timing saved"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.dwell.custom.restore"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.applyDwellCustom(); },
                     QStringLiteral("Custom timing restored"));
        }
        return true;
    });
    registerIntChoices(
        {
            {"settings.mag.follow.slow", int(GazeFollowProfile::Slow), "Gaze follow: Slow"},
            {"settings.mag.follow.sticky", int(GazeFollowProfile::Sticky), "Gaze follow: Sticky"},
            {"settings.mag.follow.smooth", int(GazeFollowProfile::Smooth), "Gaze follow: Smooth"},
            {"settings.mag.follow.snappy", int(GazeFollowProfile::Snappy), "Gaze follow: Snappy"},
        },
        &AppSettings::setMagFollowProfile);
    for (const auto& c : kLtsIndicatorCommands) {
        m_commands.registerBuiltin(QLatin1String(c.cmd), [this, c](QString*) {
            if (m_mutate) {
                m_mutate([v = int(c.style)](AppSettings& s) { s.setLtsIndicatorStyle(v); },
                         QLatin1String(c.status));
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
    auto togglePick = [this](int AppSettings::*member, int flag, int mask, int fallback,
                             const char* status) {
        return [this, member, flag, mask, fallback, status](QString*) {
            int bits = (m_settings.*member ^ flag) & mask;
            if (bits == 0) {
                bits = fallback;
            }
            m_settings.*member = bits;
            apply(true);
            notifyStatus(QLatin1String(status));
            return true;
        };
    };
    m_commands.registerBuiltin(
        QStringLiteral("settings.magPickStyle.cursor.toggle"),
        togglePick(&AppSettings::magPickStyle, PickStyle::Cursor, PickStyle::kMagPickMask,
                   PickStyle::kDefaultMagPick, "Magnify pick: cursor"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.magPickStyle.dot.toggle"),
        togglePick(&AppSettings::magPickStyle, PickStyle::Dot, PickStyle::kMagPickMask,
                   PickStyle::kDefaultMagPick, "Magnify pick: dot"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.magPickStyle.crosshair.toggle"),
        togglePick(&AppSettings::magPickStyle, PickStyle::Crosshair, PickStyle::kMagPickMask,
                   PickStyle::kDefaultMagPick, "Magnify pick: crosshair"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.magPickStyle.gaze.toggle"),
        togglePick(&AppSettings::magPickStyle, PickStyle::GazeIndicator, PickStyle::kMagPickMask,
                   PickStyle::kDefaultMagPick, "Magnify pick: gaze indicator"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mousePickStyle.cursor.toggle"),
        togglePick(&AppSettings::mousePickStyle, PickStyle::Cursor, PickStyle::kMousePickMask,
                   PickStyle::kDefaultMousePick, "Mouse pick: cursor"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mousePickStyle.dot.toggle"),
        togglePick(&AppSettings::mousePickStyle, PickStyle::Dot, PickStyle::kMousePickMask,
                   PickStyle::kDefaultMousePick, "Mouse pick: dot"));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mousePickStyle.crosshair.toggle"),
        togglePick(&AppSettings::mousePickStyle, PickStyle::Crosshair, PickStyle::kMousePickMask,
                   PickStyle::kDefaultMousePick, "Mouse pick: crosshair"));
    m_commands.registerBuiltin(QStringLiteral("settings.pickWindow.round"), [this](QString*) {
        m_settings.pickWindowRound = true;
        apply(true);
        notifyStatus(QStringLiteral("Zoom shape: Round"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.pickWindow.square"), [this](QString*) {
        m_settings.pickWindowRound = false;
        apply(true);
        notifyStatus(QStringLiteral("Zoom shape: Square"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.pickCenter.gaze"), [this](QString*) {
        m_settings.mouseMoveMagPickCenterOnDwell = true;
        apply(true);
        notifyStatus(QStringLiteral("Zoom position: Gaze point"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.pickCenter.screen"), [this](QString*) {
        m_settings.mouseMoveMagPickCenterOnDwell = false;
        apply(true);
        notifyStatus(QStringLiteral("Zoom position: Screen center"));
        return true;
    });

    m_commands.registerBuiltin(QStringLiteral("settings.reset"), [this](QString*) {
        if (m_reset) {
            m_reset();
        }
        return true;
    });

    m_commands.registerBuiltin(QStringLiteral("settings.theme.source"), [this](QString* error) {
        return openColorPicker(QStringLiteral("customPrimaryColor"), error);
    });
    m_commands.registerBuiltin(QStringLiteral("settings.theme.assign.primary"), [this](QString*) {
        themeSetAssignPrimary(true);
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.theme.assign.secondary"), [this](QString*) {
        themeSetAssignPrimary(false);
        return true;
    });
    {
        struct ShadeCmd {
            MaterialPalette::Family family;
            const char* id;
        };
        const ShadeCmd cmds[] = {
            {MaterialPalette::Family::Primary, "primary"},
            {MaterialPalette::Family::Complementary, "complementary"},
            {MaterialPalette::Family::Analogous1, "analogous1"},
            {MaterialPalette::Family::Analogous2, "analogous2"},
            {MaterialPalette::Family::Triadic1, "tertiary1"},
            {MaterialPalette::Family::Triadic2, "tertiary2"},
        };
        for (const ShadeCmd& cmd : cmds) {
            const int f = int(cmd.family);
            for (int i = 0; i < MaterialPalette::kShadeCount; ++i) {
                m_commands.registerBuiltin(
                    QStringLiteral("settings.theme.shade.%1.%2").arg(QLatin1String(cmd.id)).arg(i),
                    [this, f, i](QString*) {
                        themePickShade(f, i);
                        return true;
                    });
            }
        }
    }
    for (int i = 0; i < MaterialPalette::kShadeCount; ++i) {
        m_commands.registerBuiltin(QStringLiteral("settings.color.draftShade.%1").arg(i),
                                   [this, i](QString*) {
                                       colorApplyDraftShade(i);
                                       return true;
                                   });
    }

    auto applyAppearance = [this](ThemeAppearance appearance, const char* status) {
        return [this, appearance, status](QString*) {
            if (m_mutate) {
                m_mutate([appearance](AppSettings& s) { s.setThemeAppearance(appearance); },
                         QLatin1String(status));
            }
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("theme.light"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setThemeDark(false); }, QStringLiteral("Theme: Light"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("theme.dark"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setThemeDark(true); }, QStringLiteral("Theme: Dark"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("theme.lightTinted"),
                               applyAppearance(ThemeAppearance::LightTinted, "Theme: Light tint"));
    m_commands.registerBuiltin(QStringLiteral("theme.darkTinted"),
                               applyAppearance(ThemeAppearance::DarkTinted, "Theme: Dark tint"));
    {
        struct TintCmd {
            ThemeTintFamily family;
            const char* id;
            const char* status;
        };
        const TintCmd tints[] = {
            {ThemeTintFamily::None, "none", "Tint: None"},
            {ThemeTintFamily::Primary, "primary", "Tint: Primary"},
            {ThemeTintFamily::Complementary, "complementary", "Tint: Complementary"},
            {ThemeTintFamily::Analogous1, "analogous1", "Tint: Analogous"},
            {ThemeTintFamily::Analogous2, "analogous2", "Tint: Analogous"},
            {ThemeTintFamily::Tertiary1, "tertiary1", "Tint: Tertiary"},
            {ThemeTintFamily::Tertiary2, "tertiary2", "Tint: Tertiary"},
        };
        for (const TintCmd& t : tints) {
            m_commands.registerBuiltin(QStringLiteral("theme.tint.%1").arg(QLatin1String(t.id)),
                                       [this, fam = t.family, status = t.status](QString*) {
                                           if (m_mutate) {
                                               m_mutate([fam](AppSettings& s) {
                                                   s.setThemeTintFamily(fam);
                                               }, QLatin1String(status));
                                           }
                                           return true;
                                       });
        }
    }
    for (int i = 0; i < kThemeBrightnessLevels; ++i) {
        m_commands.registerBuiltin(QStringLiteral("theme.brightness.%1").arg(i), [this, i](QString*) {
            if (m_mutate) {
                m_mutate([i](AppSettings& s) { s.setThemeBrightness(i); },
                         QStringLiteral("Background shade %1").arg(i + 1));
            }
            return true;
        });
    }
    m_commands.registerBuiltin(QStringLiteral("theme.custom"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setThemeCustom(true); },
                     QStringLiteral("Theme: Custom"));
        }
        return true;
    });
    for (int i = 0; i < kThemeBrandCount; ++i) {
        m_commands.registerBuiltin(QStringLiteral("theme.primary.%1").arg(i), [this, i](QString*) {
            if (m_mutate) {
                m_mutate([i](AppSettings& s) { s.setThemePrimaryIndex(i); },
                         QStringLiteral("Accent: %1")
                             .arg(QLatin1String(ThemeScheme::brands()[i].name)));
            }
            return true;
        });
    }
    for (int i = 0; i < kThemeBrandCount; ++i) {
        m_commands.registerBuiltin(QStringLiteral("theme.secondary.%1").arg(i), [this, i](QString*) {
            if (m_mutate) {
                m_mutate([i](AppSettings& s) { s.setThemeSecondaryIndex(i); },
                         QStringLiteral("Progress: %1")
                             .arg(QLatin1String(ThemeScheme::brands()[i].name)));
            }
            return true;
        });
    }
}

} // namespace gazer
