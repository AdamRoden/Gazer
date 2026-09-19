#include "input/VigemLib.h"

#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#endif

namespace gazer {

VigemLib::VigemLib() = default;

VigemLib::~VigemLib()
{
    unload();
}

QString VigemLib::errorMessage(Error err)
{
    switch (err) {
    case kOk:
        return QStringLiteral("ok");
    case kBusNotFound:
        return QStringLiteral("ViGEmBus is not installed");
    case 0xE0000002u:
        return QStringLiteral("ViGEm has no free pad slot");
    case 0xE0000009u:
        return QStringLiteral("ViGEmBus is installed but could not be opened");
    default:
        return QStringLiteral("ViGEm error 0x%1").arg(err, 8, 16, QLatin1Char('0'));
    }
}

bool VigemLib::load(QString* error)
{
    if (m_loaded) {
        return true;
    }

#ifdef Q_OS_WIN
    QStringList candidates;
    if (m_overridePath.has_value()) {
        const QString forced = m_overridePath->trimmed();
        if (forced.isEmpty() || !QFileInfo::exists(forced) || !QFileInfo(forced).isFile()) {
            if (error) {
                *error = QStringLiteral(
                    "ViGEmClient.dll not found. Place it next to Gazer.exe or set GAZER_VIGEM_DLL, "
                    "and install ViGEmBus.");
            }
            return false;
        }
        candidates << forced;
    } else {
        const QString envDll = qEnvironmentVariable("GAZER_VIGEM_DLL").trimmed();
        if (!envDll.isEmpty()) {
            candidates << envDll;
        }
        if (!QCoreApplication::applicationDirPath().isEmpty()) {
            candidates << QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("ViGEmClient.dll"));
        }
        const QString pf = qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files"));
        candidates << QDir(pf).filePath(
            QStringLiteral("Nefarius Software Solutions/ViGEm Client/ViGEmClient.dll"));
    }

    HMODULE mod = nullptr;
    for (const QString& path : candidates) {
        if (!QFileInfo::exists(path)) {
            continue;
        }
        mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (mod) {
            m_path = path;
            GAZER_INFO << "Loaded ViGEmClient:" << path;
            break;
        }
    }
    if (!mod) {
        mod = LoadLibraryW(L"ViGEmClient.dll");
        if (mod) {
            m_path = QStringLiteral("ViGEmClient.dll (PATH)");
            GAZER_INFO << "Loaded ViGEmClient from PATH";
        }
    }
    if (!mod) {
        if (error) {
            *error = QStringLiteral(
                "ViGEmClient.dll not found. Place it next to Gazer.exe or set GAZER_VIGEM_DLL, "
                "and install ViGEmBus.");
        }
        return false;
    }
    m_handle = mod;

    auto require = [&](const char* name) -> FARPROC {
        FARPROC p = GetProcAddress(mod, name);
        if (!p && error) {
            *error = QStringLiteral("Missing ViGEm export: %1").arg(QString::fromLatin1(name));
        }
        return p;
    };

#    define GAZER_BIND(field, symbol)                                                              \
        do {                                                                                       \
            FARPROC p = require(symbol);                                                           \
            if (!p) {                                                                              \
                unload();                                                                          \
                return false;                                                                      \
            }                                                                                      \
            field = reinterpret_cast<decltype(field)>(p);                                          \
        } while (0)

    GAZER_BIND(alloc, "vigem_alloc");
    GAZER_BIND(free, "vigem_free");
    GAZER_BIND(connect, "vigem_connect");
    GAZER_BIND(disconnect, "vigem_disconnect");
    GAZER_BIND(target_x360_alloc, "vigem_target_x360_alloc");
    GAZER_BIND(target_free, "vigem_target_free");
    GAZER_BIND(target_add, "vigem_target_add");
    GAZER_BIND(target_remove, "vigem_target_remove");
    GAZER_BIND(target_x360_update, "vigem_target_x360_update");
#    undef GAZER_BIND

    m_loaded = true;
    return true;
#else
    if (error) {
        *error = QStringLiteral("ViGEm is only available on Windows");
    }
    return false;
#endif
}

void VigemLib::unload()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        FreeLibrary(static_cast<HMODULE>(m_handle));
        m_handle = nullptr;
    }
#endif
    alloc = nullptr;
    free = nullptr;
    connect = nullptr;
    disconnect = nullptr;
    target_x360_alloc = nullptr;
    target_free = nullptr;
    target_add = nullptr;
    target_remove = nullptr;
    target_x360_update = nullptr;
    m_path.clear();
    m_loaded = false;
}

} // namespace gazer
