#include "assist/ComposeCommands.h"

#include "app/CommandRegistry.h"
#include "app/ComposeUi.h"

namespace gazer {

void registerComposeCommands(CommandRegistry& commands, ComposeUi& compose)
{
    commands.registerBuiltin(QStringLiteral("compose.open"),
                             [&compose](QString* error) { return compose.openCompose(error); });
    commands.registerBuiltin(QStringLiteral("compose.speak"),
                             [&compose](QString* error) { return compose.speakOrStop(error); });
    commands.registerBuiltin(QStringLiteral("compose.stop"), [&compose](QString*) {
        compose.stopSpeech();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.clear"), [&compose](QString*) {
        compose.clear();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.undo"), [&compose](QString*) {
        compose.undo();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.redo"), [&compose](QString*) {
        compose.redo();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.backspace"), [&compose](QString*) {
        compose.backspace();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.deleteWord"), [&compose](QString*) {
        compose.deleteWord();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.openVoices"),
                             [&compose](QString* error) { return compose.openVoices(error); });
    commands.registerBuiltin(QStringLiteral("compose.openHistory"),
                             [&compose](QString* error) { return compose.openHistory(error); });
    commands.registerBuiltin(QStringLiteral("compose.toggleFreestyle"),
                             [&compose](QString* error) { return compose.toggleFreestyle(error); });
    commands.registerBuiltin(QStringLiteral("compose.saveName"),
                             [&compose](QString* error) { return compose.saveNameEdit(error); });
    commands.registerBuiltin(QStringLiteral("compose.cancelName"), [&compose](QString*) {
        compose.cancelNameEdit();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.deleteName"),
                             [&compose](QString* error) { return compose.deleteNameEdit(error); });
    commands.registerBuiltin(QStringLiteral("compose.newVoicePreset"),
                             [&compose](QString* error) { return compose.newVoicePreset(error); });
    commands.registerBuiltin(QStringLiteral("compose.newTag"),
                             [&compose](QString* error) { return compose.newTag(error); });
    commands.registerBuiltin(QStringLiteral("compose.pin"),
                             [&compose](QString* error) { return compose.pin(error); });
    commands.registerBuiltin(QStringLiteral("compose.cancelAssign"), [&compose](QString*) {
        compose.cancelAssign();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.editPins"),
                             [&compose](QString*) { return compose.editPins(); });
    commands.registerBuiltin(QStringLiteral("compose.editShowColors"), [&compose](QString*) {
        compose.showEditColors();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.editShowIcons"), [&compose](QString*) {
        compose.showEditIcons();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.editClearColor"), [&compose](QString*) {
        compose.clearEditedColor();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.editClearIcon"), [&compose](QString*) {
        compose.clearEditedIcon();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("soundboard.newTopic"),
                             [&compose](QString* error) { return compose.newTopic(error); });
    commands.registerBuiltin(QStringLiteral("soundboard.loadStarters"),
                             [&compose](QString* error) { return compose.loadStarters(error); });

    auto bindId = [&](const QString& prefix, bool (ComposeUi::*fn)(const QString&, QString*)) {
        commands.registerPrefix(prefix, [&compose, prefix, fn](const CommandRegistry::Invocation& inv,
                                                               QString* error) {
            return (compose.*fn)(inv.name.mid(prefix.size()), error);
        });
    };
    auto bindVoidId = [&](const QString& prefix, void (ComposeUi::*fn)(const QString&)) {
        commands.registerPrefix(prefix, [&compose, prefix, fn](const CommandRegistry::Invocation& inv,
                                                               QString*) {
            (compose.*fn)(inv.name.mid(prefix.size()));
            return true;
        });
    };
    auto bindInt = [&](const QString& prefix, bool (ComposeUi::*fn)(int, QString*)) {
        commands.registerPrefix(prefix, [&compose, prefix, fn](const CommandRegistry::Invocation& inv,
                                                               QString* error) {
            bool ok = false;
            const int n = inv.name.mid(prefix.size()).toInt(&ok);
            return ok && (compose.*fn)(n, error);
        });
    };
    auto bindVoidInt = [&](const QString& prefix, void (ComposeUi::*fn)(int)) {
        commands.registerPrefix(prefix, [&compose, prefix, fn](const CommandRegistry::Invocation& inv,
                                                               QString*) {
            bool ok = false;
            const int n = inv.name.mid(prefix.size()).toInt(&ok);
            if (!ok) {
                return false;
            }
            (compose.*fn)(n);
            return true;
        });
    };

    bindId(QStringLiteral("compose.voicePreset."), &ComposeUi::applyVoicePreset);
    bindId(QStringLiteral("compose.editVoicePreset."), &ComposeUi::editVoicePreset);
    bindId(QStringLiteral("soundboard.edit."), &ComposeUi::openPinEdit);
    bindId(QStringLiteral("soundboard.editTopic."), &ComposeUi::openTopicEdit);
    bindId(QStringLiteral("soundboard.play."), &ComposeUi::playSoundboard);
    bindId(QStringLiteral("soundboard.assign."), &ComposeUi::assignSoundboard);
    bindId(QStringLiteral("soundboard.topic."), &ComposeUi::setTopic);
    bindId(QStringLiteral("history.play."), &ComposeUi::playHistory);
    bindId(QStringLiteral("history.restore."), &ComposeUi::restoreHistory);
    bindId(QStringLiteral("history.delete."), &ComposeUi::deleteHistory);
    bindVoidId(QStringLiteral("speech.voice."), &ComposeUi::selectVoice);
    bindVoidId(QStringLiteral("speech.voicePreview."), &ComposeUi::previewVoice);
    bindVoidId(QStringLiteral("speech.fav.toggle."), &ComposeUi::toggleFavorite);
    bindVoidId(QStringLiteral("speech.lang.set."), &ComposeUi::setLangFilter);
    bindInt(QStringLiteral("compose.editTag."), &ComposeUi::editTagAt);
    bindInt(QStringLiteral("compose.editColor."), &ComposeUi::setEditedColor);
    bindInt(QStringLiteral("compose.editIcon."), &ComposeUi::setEditedIcon);
    bindVoidInt(QStringLiteral("compose.removeWord."), &ComposeUi::removeVisibleWord);
    bindVoidInt(QStringLiteral("compose.insertTagAt."), &ComposeUi::insertTagAt);
    bindVoidInt(QStringLiteral("history.list.goto."), &ComposeUi::historyGoto);
    bindVoidInt(QStringLiteral("speech.voiceList.goto."), &ComposeUi::voicesGoto);

    auto setModel = [&compose](const QString& model) {
        return [&compose, model](QString*) {
            compose.setSpeechModel(model);
            return true;
        };
    };
    commands.registerBuiltin({"speech.model.sapi", "settings.speech.model.sapi"},
                             setModel(QStringLiteral("sapi")));
    commands.registerBuiltin({"speech.model.eleven_flash_v2_5",
                              "settings.speech.model.eleven_flash_v2_5"},
                             setModel(QStringLiteral("eleven_flash_v2_5")));
    commands.registerBuiltin({"speech.model.eleven_v3", "settings.speech.model.eleven_v3"},
                             setModel(QStringLiteral("eleven_v3")));
    commands.registerBuiltin(QStringLiteral("speech.preview"), [&compose](QString*) {
        compose.previewCurrent();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.fav.toggle"), [&compose](QString*) {
        compose.toggleFavorite({});
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.voiceList.next"), [&compose](QString*) {
        compose.voicesPage(1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.voiceList.prev"), [&compose](QString*) {
        compose.voicesPage(-1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.gender.all"), [&compose](QString*) {
        compose.setGenderFilter({});
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.gender.female"), [&compose](QString*) {
        compose.setGenderFilter(QStringLiteral("female"));
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.gender.male"), [&compose](QString*) {
        compose.setGenderFilter(QStringLiteral("male"));
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.lang.all"), [&compose](QString*) {
        compose.setLangFilter({});
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.speed.dec"), [&compose](QString*) {
        compose.nudgeSpeed(-1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.speed.inc"), [&compose](QString*) {
        compose.nudgeSpeed(1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.volume.dec"), [&compose](QString*) {
        compose.nudgeVolume(-1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("speech.volume.inc"), [&compose](QString*) {
        compose.nudgeVolume(1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("history.list.next"), [&compose](QString*) {
        compose.historyPage(1);
        return true;
    });
    commands.registerBuiltin(QStringLiteral("history.list.prev"), [&compose](QString*) {
        compose.historyPage(-1);
        return true;
    });
}

} // namespace gazer
