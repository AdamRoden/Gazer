#include "app/SettingsUi.h"

#include "app/AppUpdater.h"
#include "app/CommandRegistry.h"
#include "app/ComposeUi.h"
#include "app/SettingsCatalog.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtGlobal>

namespace gazer {

namespace {

constexpr int kDefaultsQueryMax = 40;

} // namespace

void SettingsUi::setCompose(ComposeUi* compose)
{
    m_compose = compose;
    if (!m_compose) {
        return;
    }
    m_compose->setGuestText(
        [this](const QString& text) { onGuestInsert(text); },
        [this](const QString& command) { onGuestCommand(command); });
    m_compose->setSettingsFileHandlers(
        [this](const QString& path, QString* error) { return importSettingsFile(path, error); },
        [this](const QString& path, QString* error) { return exportSettingsFile(path, error); });
}

QString SettingsUi::defaultsFilter() const
{
    return m_defaultsFilter;
}

bool SettingsUi::defaultsFilterOpen() const
{
    return m_filterOpen;
}

bool SettingsUi::defaultsPicked(const QString& id) const
{
    return m_defaultsPicked.contains(id);
}

void SettingsUi::fillDefaultsList(PageGrid& host)
{
    const bool editing = m_defaultsDraftKind == DefaultsDraftKind::Bool;
    fillSettingsDefaultsGrid(host, m_settings, m_defaultsFilter, m_defaultsQuery,
                             editing ? m_defaultsChoiceId : QString(), m_defaultsDraftOn);
}

void SettingsUi::decorateDefaults(PageDocument& doc)
{
    QVector<int> layers{1};
    if (m_filterOpen) {
        layers.push_back(2);
    }
    doc.showLayers = layers;
    if (PageCell* search = PageEdit::findCell(doc, QStringLiteral("def_search"))) {
        search->caption = m_defaultsQuery.isEmpty() ? QStringLiteral("Name, description, or page")
                                                     : m_defaultsQuery;
    }
    if (PageCell* filter = PageEdit::findCell(doc, QStringLiteral("def_filter"))) {
        filter->label = m_defaultsFilter;
    }
    if (PageGrid* list = PageEdit::findGrid(doc, QStringLiteral("scroller"))) {
        fillDefaultsList(*list);
    }
}

void SettingsUi::decorateUpdate(PageDocument& doc)
{
    const AppUpdater::Phase phase = m_updater ? m_updater->phase() : AppUpdater::Phase::Idle;
    const QString status = m_updater ? m_updater->statusLine() : QStringLiteral("Updates are unavailable.");
    const QString note = m_updater ? m_updater->promptCaption() : QString();
    PageEdit::forEachCell(doc, [&](PageGrid&, PageCell& cell) {
        if (cell.id == QLatin1String("update_body")) {
            cell.label = status;
            cell.caption = note;
        } else if (cell.id == QLatin1String("update_title")) {
            if (phase == AppUpdater::Phase::Ready) {
                cell.label = QStringLiteral("Restart Gazer to update");
            } else if (phase == AppUpdater::Phase::Failed) {
                cell.label = QStringLiteral("Update failed");
            } else if (phase == AppUpdater::Phase::Downloading) {
                cell.label = QStringLiteral("Downloading");
            } else {
                cell.label = QStringLiteral("Update available");
            }
        } else if (cell.id == QLatin1String("update_now")) {
            if (phase == AppUpdater::Phase::Ready) {
                cell.label = QStringLiteral("Restart Gazer");
            } else if (phase == AppUpdater::Phase::Available) {
                cell.label = QStringLiteral("Update now");
            } else {
                cell.label = QStringLiteral("Close");
            }
        }
    });
}

void SettingsUi::stampSetupUpdate(PageDocument& doc)
{
    const QString status = m_updater ? m_updater->statusLine()
                                     : QStringLiteral("Gazer");
    const QString note = m_updater ? m_updater->setupNote() : QString();
    PageEdit::forEachCell(doc, [&](PageGrid&, PageCell& cell) {
        if (cell.id == QLatin1String("update_status")) {
            cell.caption = status;
        } else if (cell.id == QLatin1String("update_note") && !note.isEmpty()) {
            cell.caption = note;
        }
    });
}

void SettingsUi::refreshDefaultsList()
{
    m_settingsScroll.insert(QStringLiteral("main_settings_defaults"), 0);
    m_pages.refreshDecorated();
}

void SettingsUi::openDefaultsSearch()
{
    if (!m_compose) {
        notifyStatus(QStringLiteral("Search needs the Speak keyboard"));
        return;
    }
    QString err;
    if (!m_compose->beginGuestKeyboard(&err)) {
        if (!err.isEmpty()) {
            notifyStatus(err);
        }
        return;
    }
    m_defaultsSearchOpen = true;
}

void SettingsUi::onGuestInsert(const QString& text)
{
    if (text.isEmpty() || m_defaultsQuery.size() >= kDefaultsQueryMax) {
        return;
    }
    const QString next = (m_defaultsQuery + text).left(kDefaultsQueryMax);
    if (next == m_defaultsQuery) {
        return;
    }
    m_defaultsQuery = next;
    refreshDefaultsList();
}

void SettingsUi::onGuestCommand(const QString& command)
{
    if (command == QLatin1String("backspace")) {
        if (m_defaultsQuery.isEmpty()) {
            return;
        }
        m_defaultsQuery.chop(1);
        refreshDefaultsList();
        return;
    }
    if (command == QLatin1String("space")) {
        onGuestInsert(QStringLiteral(" "));
        return;
    }
    if (command == QLatin1String("enter") || command == QLatin1String("escape")) {
        m_defaultsSearchOpen = false;
        if (m_compose) {
            m_compose->endGuestKeyboard();
        }
    }
}

void SettingsUi::toggleDefaultsFilterMenu()
{
    closeDefaultsEditor();
    m_filterOpen = !m_filterOpen;
    m_pages.refreshDecorated();
    m_pages.refreshActive();
}

void SettingsUi::setDefaultsFilter(const QString& page)
{
    closeDefaultsEditor();
    m_filterOpen = false;
    if (m_defaultsFilter == page) {
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return;
    }
    m_defaultsFilter = page;
    refreshDefaultsList();
}

void SettingsUi::defaultsSelectVisible()
{
    const SettingsCatalogEntry* entries = settingsCatalogEntries();
    const int count = settingsCatalogCount();
    for (int i = 0; i < count; ++i) {
        if (settingsEntryVisible(entries[i], m_defaultsFilter, m_defaultsQuery)) {
            m_defaultsPicked.insert(QLatin1String(entries[i].id));
        }
    }
    m_pages.refreshDecorated();
    m_pages.refreshActive();
}

void SettingsUi::defaultsDeselectAll()
{
    if (m_defaultsPicked.isEmpty()) {
        return;
    }
    m_defaultsPicked.clear();
    m_pages.refreshDecorated();
    m_pages.refreshActive();
}

void SettingsUi::defaultsResetSelected()
{
    QStringList keys;
    int picked = 0;
    for (const QString& id : m_defaultsPicked) {
        const SettingsCatalogEntry* entry = findSettingsCatalogEntry(id);
        if (!entry) {
            continue;
        }
        ++picked;
        keys.append(settingsCatalogJsonKeys(*entry));
    }
    if (keys.isEmpty()) {
        notifyStatus(QStringLiteral("Select settings to reset"));
        return;
    }
    if (!m_mutate) {
        return;
    }
    m_mutate(
        [keys](AppSettings& settings) { (void)settings.resetKeysToDefaults(keys); },
        QStringLiteral("Reset %1 settings").arg(picked));
    m_defaultsPicked.clear();
}

void SettingsUi::importSettings()
{
    if (!m_compose) {
        notifyStatus(QStringLiteral("Import needs the Speak file browser"));
        return;
    }
    QString err;
    if (!m_compose->openSettingsFiles(false, &err) && !err.isEmpty()) {
        notifyStatus(err);
    }
}

void SettingsUi::exportSettings()
{
    if (!m_compose) {
        notifyStatus(QStringLiteral("Export needs the Speak file browser"));
        return;
    }
    QString err;
    if (!m_compose->openSettingsFiles(true, &err) && !err.isEmpty()) {
        notifyStatus(err);
    }
}

bool SettingsUi::importSettingsFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        notifyStatus(msg);
        return false;
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Cannot open settings: %1").arg(file.errorString()));
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject()) {
        return fail(QStringLiteral("Settings JSON root must be an object"));
    }
    const SettingsImportKind kind = classifySettingsImport(doc.object());
    if (kind == SettingsImportKind::Empty) {
        return fail(QStringLiteral("This file has no settings"));
    }
    if (kind == SettingsImportKind::Newer) {
        return fail(QStringLiteral("This settings file is from a newer version of Gazer"));
    }
    AppSettings next = AppSettings::defaults();
    if (!next.applyJsonObject(doc.object())) {
        return fail(QStringLiteral("Could not read settings"));
    }
    const QString live = AppSettings::defaultFilePath();
    if (QFile::exists(live)) {
        QString backupError;
        if (!AppSettings::backupExistingFile(live, &backupError)) {
            return fail(QStringLiteral("Could not back up settings before import"));
        }
    }
    if (!m_mutate) {
        return fail(QStringLiteral("Could not read settings"));
    }
    m_mutate([next](AppSettings& settings) { settings = next; }, QStringLiteral("Imported settings"));
    return true;
}

bool SettingsUi::exportSettingsFile(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString msg = QStringLiteral("Cannot write settings: %1").arg(file.errorString());
        if (error) {
            *error = msg;
        }
        notifyStatus(msg);
        return false;
    }
    file.write(QJsonDocument(m_settings.toJsonObject()).toJson(QJsonDocument::Indented));
    notifyStatus(QStringLiteral("Exported settings"));
    return true;
}

void SettingsUi::registerDefaultsCommands()
{
    m_commands.registerBuiltin(QStringLiteral("settings.defaults.search"), [this](QString*) {
        openDefaultsSearch();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.defaults.filterMenu"), [this](QString*) {
        toggleDefaultsFilterMenu();
        return true;
    });
    for (const char* page : kSettingsDefaultsPages) {
        const QString name = QString::fromLatin1(page);
        m_commands.registerBuiltin(QStringLiteral("settings.defaults.filter.%1").arg(name),
                                   [this, name](QString*) {
                                       setDefaultsFilter(name);
                                       return true;
                                   });
    }
    const SettingsCatalogEntry* entries = settingsCatalogEntries();
    const int count = settingsCatalogCount();
    for (int i = 0; i < count; ++i) {
        const QString id = QLatin1String(entries[i].id);
        m_commands.registerBuiltin(QStringLiteral("settings.defaults.pick.%1").arg(id),
                                   [this, id](QString*) {
                                       if (m_defaultsPicked.contains(id)) {
                                           m_defaultsPicked.remove(id);
                                       } else {
                                           m_defaultsPicked.insert(id);
                                       }
                                       m_pages.refreshActive();
                                       return true;
                                   });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.defaults.selectAll"), [this](QString*) {
        defaultsSelectVisible();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.defaults.deselectAll"), [this](QString*) {
        defaultsDeselectAll();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.defaults.resetSelected"), [this](QString*) {
        defaultsResetSelected();
        return true;
    });
    m_commands.registerPrefix(
        QStringLiteral("settings.defaults.edit."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            const auto prefix = QLatin1String("settings.defaults.edit.");
            const QString id = inv.name.mid(prefix.size());
            if (id.isEmpty() || id.contains(QLatin1Char('.'))) {
                return false;
            }
            return editDefaultSetting(id);
        });
    m_commands.registerPrefix(
        QStringLiteral("settings.defaults.value."),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            const auto prefix = QLatin1String("settings.defaults.value.");
            const QString rest = inv.name.mid(prefix.size());
            const int dot = rest.indexOf(QLatin1Char('.'));
            if (dot <= 0) {
                return false;
            }
            const QString token = rest.mid(dot + 1);
            if (token.isEmpty()) {
                return false;
            }
            return applyDefaultsChoice(rest.left(dot), token);
        });
    m_commands.registerBuiltin(QStringLiteral("settings.import"), [this](QString*) {
        importSettings();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.export"), [this](QString*) {
        exportSettings();
        return true;
    });
}

void SettingsUi::registerUpdateCommands()
{
    m_commands.registerBuiltin(QStringLiteral("settings.update.check"), [this](QString*) {
        if (m_updater) {
            m_updater->checkNow();
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.update.now"), [this](QString*) {
        if (!m_updater) {
            m_pages.closePage(QStringLiteral("main_update"));
            return true;
        }
        const AppUpdater::Phase phase = m_updater->phase();
        if (phase == AppUpdater::Phase::Ready) {
            if (!m_updater->beginRestart()) {
                notifyStatus(m_updater->blockReason());
                return false;
            }
            return true;
        }
        if (phase == AppUpdater::Phase::Available) {
            m_pages.closePage(QStringLiteral("main_update"));
            m_updater->beginDownload();
            return true;
        }
        m_pages.closePage(QStringLiteral("main_update"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.update.later"), [this](QString*) {
        if (m_updater) {
            m_updater->dismiss();
        }
        m_pages.closePage(QStringLiteral("main_update"));
        return true;
    });
}

} // namespace gazer
