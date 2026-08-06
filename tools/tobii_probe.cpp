// Minimal Stream Engine device list probe (Windows).
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>

typedef struct tobii_api_t tobii_api_t;
typedef int tobii_error_t;
typedef void (*url_recv_t)(const char* url, void* user_data);
typedef tobii_error_t (*api_create_t)(tobii_api_t**, void*, void*);
typedef tobii_error_t (*api_destroy_t)(tobii_api_t*);
typedef tobii_error_t (*enum_urls_t)(tobii_api_t*, url_recv_t, void*);
typedef const char* (*err_msg_t)(tobii_error_t);

static void on_url(const char* url, void* user_data)
{
    auto* v = static_cast<std::vector<std::string>*>(user_data);
    if (url)
        v->emplace_back(url);
}

int main()
{
    const wchar_t* path = L"C:\\Program Files\\Tobii\\Tobii EyeX\\tobii_stream_engine.dll";
    HMODULE m = LoadLibraryW(path);
    if (!m) {
        printf("LoadLibrary failed %lu\n", GetLastError());
        return 1;
    }
    auto api_create = (api_create_t)GetProcAddress(m, "tobii_api_create");
    auto api_destroy = (api_destroy_t)GetProcAddress(m, "tobii_api_destroy");
    auto enum_urls = (enum_urls_t)GetProcAddress(m, "tobii_enumerate_local_device_urls");
    auto err_msg = (err_msg_t)GetProcAddress(m, "tobii_error_message");
    if (!api_create || !enum_urls) {
        printf("Missing exports\n");
        return 1;
    }

    tobii_api_t* api = nullptr;
    tobii_error_t e = api_create(&api, nullptr, nullptr);
    printf("api_create: %d %s\n", e, err_msg ? err_msg(e) : "");
    if (e != 0 || !api)
        return 2;

    std::vector<std::string> urls;
    e = enum_urls(api, on_url, &urls);
    printf("enumerate: %d %s\n", e, err_msg ? err_msg(e) : "");
    printf("devices: %zu\n", urls.size());
    for (const auto& u : urls)
        printf("  %s\n", u.c_str());

    api_destroy(api);
    FreeLibrary(m);
    return urls.empty() ? 3 : 0;
}
