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
    commands.registerBuiltin(QStringLiteral("compose.selectAll"), [&compose](QString*) {
        compose.selectAll();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.cut"), [&compose](QString*) {
        compose.cut();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.copy"), [&compose](QString*) {
        compose.copy();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.paste"), [&compose](QString*) {
        compose.paste();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.clearStyle"), [&compose](QString*) {
        compose.clearStyle();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.exportBoard"),
                             [&compose](QString* error) { return compose.exportBoard(error); });
    commands.registerBuiltin(QStringLiteral("compose.openFiles"),
                             [&compose](QString* error) { return compose.openFiles(error); });
    commands.registerBuiltin(QStringLiteral("compose.browserUp"),
                             [&compose](QString* error) { return compose.browserUp(error); });
    commands.registerBuiltin(QStringLiteral("compose.browserPrev"),
                             [&compose](QString* error) { return compose.browserPrev(error); });
    commands.registerBuiltin(QStringLiteral("compose.browserForward"),
                             [&compose](QString* error) { return compose.browserForward(error); });
    commands.registerBuiltin(QStringLiteral("compose.browserSave"),
                             [&compose](QString* error) { return compose.browserSave(error); });
    commands.registerBuiltin(QStringLiteral("compose.fileYes"),
                             [&compose](QString* error) { return compose.confirmFileYes(error); });
    commands.registerBuiltin(QStringLiteral("compose.fileNo"), [&compose](QString*) {
        compose.confirmFileNo();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.browserBack"), [&compose](QString*) {
        compose.browserBack();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.importChoose"),
                             [&compose](QString* error) { return compose.importChoose(error); });
    commands.registerBuiltin(QStringLiteral("compose.importMerge"),
                             [&compose](QString* error) { return compose.importMerge(error); });
    commands.registerBuiltin(QStringLiteral("compose.importReplace"),
                             [&compose](QString* error) { return compose.importReplace(error); });
    commands.registerBuiltin(QStringLiteral("compose.importFile"),
                             [&compose](QString* error) { return compose.importFromDialog(error); });
    commands.registerBuiltin(QStringLiteral("compose.exportFile"),
                             [&compose](QString* error) { return compose.exportToDialog(error); });
    commands.registerBuiltin(QStringLiteral("compose.saveName"),
                             [&compose](QString* error) { return compose.saveNameEdit(error); });
    commands.registerBuiltin(QStringLiteral("compose.cancelName"), [&compose](QString*) {
        compose.cancelNameEdit();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.deleteName"),
                             [&compose](QString* error) { return compose.deleteNameEdit(error); });
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

    bindVoidId(QStringLiteral("compose.applyStyle."), &ComposeUi::applyStyle);
    bindVoidId(QStringLiteral("compose.favStyle."), &ComposeUi::toggleStyleFavorite);
    bindVoidId(QStringLiteral("compose.trashStyle."), &ComposeUi::trashStyle);
    bindVoidInt(QStringLiteral("compose.stylePage."), &ComposeUi::pageStyles);
    commands.registerBuiltin(QStringLiteral("compose.openStyleEditor"), [&compose](QString*) {
        compose.openStyleEditor();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.closeStyleEditor"), [&compose](QString*) {
        compose.closeStyleEditor();
        return true;
    });
    commands.registerBuiltin(QStringLiteral("compose.beginStyleName"), [&compose](QString*) {
        compose.beginStyleName();
        return true;
    });
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
    bindInt(QStringLiteral("compose.editColor."), &ComposeUi::setEditedColor);
    bindInt(QStringLiteral("compose.editIcon."), &ComposeUi::setEditedIcon);
    bindVoidInt(QStringLiteral("compose.pageChips."), &ComposeUi::pageChips);
    bindInt(QStringLiteral("compose.importPick."), &ComposeUi::pickImportFile);
    bindVoidInt(QStringLiteral("compose.importPage."), &ComposeUi::pageImportFiles);
    bindVoidInt(QStringLiteral("compose.fileRow."), &ComposeUi::scrollFileRows);
    bindInt(QStringLiteral("compose.browserGoto."), &ComposeUi::browserGoto);
    bindVoidInt(QStringLiteral("compose.removeWord."), &ComposeUi::removeVisibleWord);
    bindVoidInt(QStringLiteral("compose.acceptPrediction."), &ComposeUi::acceptPrediction);
    bindVoidInt(QStringLiteral("compose.moveEndOfWord."), &ComposeUi::moveEndOfWord);
    bindVoidInt(QStringLiteral("compose.moveStartOfWord."), &ComposeUi::moveStartOfWord);
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
    commands.registerBuiltin({"speech.model.eleven_v4_turbo",
                              "settings.speech.model.eleven_v4_turbo"},
                             setModel(QStringLiteral("eleven_v4_turbo")));
    // Older boards still name Flash and v3. Both select v4 Turbo.
    commands.registerBuiltin({"speech.model.eleven_flash_v2_5",
                              "settings.speech.model.eleven_flash_v2_5"},
                             setModel(QStringLiteral("eleven_v4_turbo")));
    commands.registerBuiltin({"speech.model.eleven_v3", "settings.speech.model.eleven_v3"},
                             setModel(QStringLiteral("eleven_v4_turbo")));
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
