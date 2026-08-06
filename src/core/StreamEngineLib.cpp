#include "core/StreamEngineLib.h"

#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

StreamEngineLib::StreamEngineLib() = default;

StreamEngineLib::~StreamEngineLib()
{
    unload();
}

QString StreamEngineLib::errorString(tobii_error_t err) const
{
    if (error_message) {
        if (const char* msg = error_message(err)) {
            return QString::fromUtf8(msg);
        }
    }
    return QStringLiteral("tobii_error %1").arg(static_cast<int>(err));
}

bool StreamEngineLib::load(QString* error)
{
    if (m_loaded) {
        return true;
    }

#ifdef Q_OS_WIN
    QStringList candidates;

    if (!QCoreApplication::applicationDirPath().isEmpty()) {
        candidates << QDir(QCoreApplication::applicationDirPath())
                          .filePath(QStringLiteral("tobii_stream_engine.dll"));
    }

    const QString pf = qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files"));
    candidates << QDir(pf).filePath(QStringLiteral("Tobii/Tobii EyeX/tobii_stream_engine.dll"));
    candidates << QDir(pf).filePath(QStringLiteral("Tobii/web_host/tobii_stream_engine.dll"));

    const QString envDll = qEnvironmentVariable("TOBII_STREAM_ENGINE_DLL");
    if (!envDll.isEmpty()) {
        candidates.prepend(envDll);
    }
    const QString envDir = qEnvironmentVariable("TOBII_STREAM_ENGINE_DIR");
    if (!envDir.isEmpty()) {
        candidates.prepend(QDir(envDir).filePath(QStringLiteral("tobii_stream_engine.dll")));
        candidates.prepend(
            QDir(envDir).filePath(QStringLiteral("lib/tobii/tobii_stream_engine.dll")));
    }

    HMODULE mod = nullptr;
    for (const QString& path : candidates) {
        if (!QFileInfo::exists(path)) {
            continue;
        }
        mod = LoadLibraryW(reinterpret_cast<LPCWSTR>(path.utf16()));
        if (mod) {
            m_path = path;
            GAZER_INFO << "Loaded Stream Engine:" << path;
            break;
        }
    }

    if (!mod) {
        mod = LoadLibraryW(L"tobii_stream_engine.dll");
        if (mod) {
            m_path = QStringLiteral("tobii_stream_engine.dll (PATH)");
            GAZER_INFO << "Loaded Stream Engine from PATH";
        }
    }

    if (!mod) {
        if (error) {
            *error = QStringLiteral(
                "tobii_stream_engine.dll not found. Install Tobii Experience / Eye Tracking "
                "Core, or set TOBII_STREAM_ENGINE_DLL.");
        }
        return false;
    }

    m_handle = mod;

    auto require = [&](const char* name) -> FARPROC {
        FARPROC p = GetProcAddress(mod, name);
        if (!p && error) {
            *error = QStringLiteral("Missing export: %1").arg(QString::fromLatin1(name));
        }
        return p;
    };

#define GAZER_BIND(field, symbol)                                                                  \
    do {                                                                                           \
        FARPROC p = require(symbol);                                                               \
        if (!p) {                                                                                  \
            unload();                                                                              \
            return false;                                                                          \
        }                                                                                          \
        field = reinterpret_cast<decltype(field)>(p);                                              \
    } while (0)

    GAZER_BIND(api_create, "tobii_api_create");
    GAZER_BIND(api_destroy, "tobii_api_destroy");
    GAZER_BIND(error_message, "tobii_error_message");
    GAZER_BIND(get_api_version, "tobii_get_api_version");
    GAZER_BIND(enumerate_local_device_urls, "tobii_enumerate_local_device_urls");
    GAZER_BIND(device_create, "tobii_device_create");
    GAZER_BIND(device_destroy, "tobii_device_destroy");
    GAZER_BIND(wait_for_callbacks, "tobii_wait_for_callbacks");
    GAZER_BIND(device_process_callbacks, "tobii_device_process_callbacks");
    GAZER_BIND(device_clear_callback_buffers, "tobii_device_clear_callback_buffers");
    GAZER_BIND(device_reconnect, "tobii_device_reconnect");
    GAZER_BIND(system_clock, "tobii_system_clock");
    GAZER_BIND(gaze_point_subscribe, "tobii_gaze_point_subscribe");
    GAZER_BIND(gaze_point_unsubscribe, "tobii_gaze_point_unsubscribe");
#undef GAZER_BIND

    m_loaded = true;
    return true;
#else
    if (error) {
        *error = QStringLiteral("Stream Engine is only supported on Windows in this build");
    }
    return false;
#endif
}

void StreamEngineLib::unload()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        FreeLibrary(static_cast<HMODULE>(m_handle));
        m_handle = nullptr;
    }
#endif
    api_create = nullptr;
    api_destroy = nullptr;
    error_message = nullptr;
    get_api_version = nullptr;
    enumerate_local_device_urls = nullptr;
    device_create = nullptr;
    device_destroy = nullptr;
    wait_for_callbacks = nullptr;
    device_process_callbacks = nullptr;
    device_clear_callback_buffers = nullptr;
    device_reconnect = nullptr;
    system_clock = nullptr;
    gaze_point_subscribe = nullptr;
    gaze_point_unsubscribe = nullptr;
    m_loaded = false;
    m_path.clear();
}

} // namespace gazer
