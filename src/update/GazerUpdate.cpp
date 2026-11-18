// Tiny update helper. No Qt. The user-mode copy waits until Gazer.exe has
// exited, runs the SYSTEM task, then relaunches Gazer through explorer.exe.
// The SYSTEM copy checks the MSI signature and runs msiexec /qn.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <wintrust.h>

#include "update/SignatureIntact.h"

#include <stdio.h>
#include <string>
#include <vector>

namespace {

bool hasArg(int argc, wchar_t** argv, const wchar_t* name)
{
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

const wchar_t* argValue(int argc, wchar_t** argv, const wchar_t* name)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return nullptr;
}

std::wstring programData()
{
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"ProgramData", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return L"C:\\ProgramData";
    }
    return buf;
}

std::wstring quote(const std::wstring& text)
{
    return L"\"" + text + L"\"";
}

bool relaunchFromTemp(const wchar_t* destName, int argc, wchar_t** argv)
{
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH)) {
        return false;
    }
    wchar_t temp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp)) {
        return false;
    }
    const std::wstring dest = std::wstring(temp) + destName;
    if (!CopyFileW(self, dest.c_str(), FALSE)) {
        return false;
    }
    std::wstring cmd = quote(dest);
    for (int i = 1; i < argc; ++i) {
        cmd += L" ";
        cmd += quote(argv[i]);
    }
    cmd += L" \"--from-copy\"";
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(dest.c_str(), buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        return false;
    }
    // A Temp copy that cannot load its DLLs dies at once with STATUS_DLL_NOT_FOUND
    // and never writes a result. Run the work here instead of reporting success.
    if (WaitForSingleObject(pi.hProcess, 500) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (code == 0xC0000135u || code == 0xC0000142u) {
            return false;
        }
        return true;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool processExists(const wchar_t* name)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return false;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, name) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return found;
}

void waitUntilGazerExits()
{
    const ULONGLONG start = GetTickCount64();
    while (processExists(L"Gazer.exe")) {
        if (GetTickCount64() - start > 15000) {
            return;
        }
        Sleep(200);
    }
}

std::wstring signerThumbprint(const wchar_t* path)
{
    HCERTSTORE store = nullptr;
    HCRYPTMSG msg = nullptr;
    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, path, CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &store, &msg,
                          nullptr)) {
        return {};
    }
    std::wstring thumb;
    DWORD bytes = 0;
    if (CryptMsgGetParam(msg, CMSG_SIGNER_CERT_INFO_PARAM, 0, nullptr, &bytes) && bytes > 0) {
        std::vector<BYTE> buf(bytes);
        if (CryptMsgGetParam(msg, CMSG_SIGNER_CERT_INFO_PARAM, 0, buf.data(), &bytes)) {
            auto* info = reinterpret_cast<CERT_INFO*>(buf.data());
            PCCERT_CONTEXT cert = CertFindCertificateInStore(
                store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, info,
                nullptr);
            if (cert) {
                BYTE hash[20];
                DWORD hashLen = sizeof(hash);
                if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash,
                                                      &hashLen)) {
                    static const wchar_t* kHex = L"0123456789abcdef";
                    thumb.resize(hashLen * 2);
                    for (DWORD i = 0; i < hashLen; ++i) {
                        thumb[i * 2] = kHex[(hash[i] >> 4) & 0xF];
                        thumb[i * 2 + 1] = kHex[hash[i] & 0xF];
                    }
                }
                CertFreeCertificateContext(cert);
            }
        }
    }
    if (store) {
        CertCloseStore(store, 0);
    }
    if (msg) {
        CryptMsgClose(msg);
    }
    return thumb;
}

bool signatureCoversFile(const wchar_t* path)
{
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = path;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    data.pFile = &file;
    const LONG status = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    return gazer::signatureIntact(status);
}

bool sameSigner(const wchar_t* helper, const wchar_t* msi)
{
    if (!signatureCoversFile(helper) || !signatureCoversFile(msi)) {
        return false;
    }
    const std::wstring left = signerThumbprint(helper);
    const std::wstring right = signerThumbprint(msi);
    return !left.empty() && _wcsicmp(left.c_str(), right.c_str()) == 0;
}

void writeResult(const std::wstring& path, unsigned long code)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    char buf[32];
    const int n = sprintf(buf, "%lu\r\n", code);
    DWORD wrote = 0;
    if (n > 0) {
        WriteFile(file, buf, DWORD(n), &wrote, nullptr);
    }
    CloseHandle(file);
}

DWORD runAndWait(const std::wstring& exe, const std::wstring& command)
{
    std::vector<wchar_t> buf(command.begin(), command.end());
    buf.push_back(0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

void launchGazer(const wchar_t* gazer)
{
    if (!gazer || !gazer[0]) {
        return;
    }
    const std::wstring params = quote(gazer);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

int systemApply()
{
    const std::wstring root = programData() + L"\\Gazer\\update";
    const std::wstring msi = root + L"\\pending.msi";
    const std::wstring result = root + L"\\result.txt";
    if (GetFileAttributesW(msi.c_str()) == INVALID_FILE_ATTRIBUTES) {
        writeResult(result, 0);
        return 0;
    }
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, self, MAX_PATH) || !sameSigner(self, msi.c_str())) {
        writeResult(result, 1603);
        return 1603;
    }
    wchar_t sys[MAX_PATH];
    if (!GetSystemDirectoryW(sys, MAX_PATH)) {
        writeResult(result, 1603);
        return 1603;
    }
    const std::wstring msiexec = std::wstring(sys) + L"\\msiexec.exe";
    const std::wstring command =
        quote(msiexec) + L" /i " + quote(msi) + L" /qn /norestart REBOOT=ReallySuppress";
    const DWORD code = runAndWait(msiexec, command);
    writeResult(result, code);
    return int(code);
}

int finishUpdate(const wchar_t* gazer)
{
    waitUntilGazerExits();
    const std::wstring result = programData() + L"\\Gazer\\update\\result.txt";
    DeleteFileW(result.c_str());
    wchar_t sys[MAX_PATH];
    if (GetSystemDirectoryW(sys, MAX_PATH)) {
        const std::wstring schtasks = std::wstring(sys) + L"\\schtasks.exe";
        const std::wstring command = quote(schtasks) + L" /Run /TN \"Gazer\\ApplyUpdate\"";
        runAndWait(schtasks, command);
    }
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (GetFileAttributesExW(result.c_str(), GetFileExInfoStandard, &info)
            && info.nFileSizeLow > 0) {
            break;
        }
        if (GetTickCount64() - start > 15ull * 60ull * 1000ull) {
            break;
        }
        Sleep(500);
    }
    launchGazer(gazer);
    return 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return 1;
    }
    const bool fromCopy = hasArg(argc, argv, L"--from-copy");
    const bool system = hasArg(argc, argv, L"--system-apply");
    int code = 0;
    if (system) {
        if (!fromCopy && relaunchFromTemp(L"GazerUpdate-system.exe", argc, argv)) {
            LocalFree(argv);
            return 0;
        }
        code = systemApply();
    } else if (hasArg(argc, argv, L"--finish")) {
        const wchar_t* gazer = argValue(argc, argv, L"--finish");
        if (!fromCopy && relaunchFromTemp(L"GazerUpdate-run.exe", argc, argv)) {
            LocalFree(argv);
            return 0;
        }
        code = finishUpdate(gazer);
    }
    LocalFree(argv);
    return code;
}
