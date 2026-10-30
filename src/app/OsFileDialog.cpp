#include "app/OsFileDialog.h"

#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWindow>

#include <cmath>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <oleidl.h>
#  include <shobjidl.h>
#  include <shlobj.h>
#endif

namespace gazer {

#ifdef Q_OS_WIN

namespace {

struct PxBox {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

bool boardHits(const RECT& hole, const PxBox& box)
{
    return box.left < hole.right && box.right > hole.left && box.top < hole.bottom
           && box.bottom > hole.top;
}

// Largest rectangle anchored at the top-left of `work` that misses every board.
RECT holeBesideBoards(const RECT& work, const std::vector<PxBox>& boards)
{
    RECT hole = work;
    const int limit = static_cast<int>(boards.size()) + 1;
    for (int n = 0; n < limit; ++n) {
        const PxBox* hit = nullptr;
        for (const PxBox& box : boards) {
            if (boardHits(hole, box)) {
                hit = &box;
                break;
            }
        }
        if (!hit) {
            return hole;
        }
        const long long keepRight = hit->left > hole.left
                                        ? static_cast<long long>(hit->left - hole.left)
                                              * (hole.bottom - hole.top)
                                        : 0;
        const long long keepBottom = hit->top > hole.top
                                         ? static_cast<long long>(hole.right - hole.left)
                                               * (hit->top - hole.top)
                                         : 0;
        if (keepRight <= 0 && keepBottom <= 0) {
            return {};
        }
        if (keepRight >= keepBottom) {
            hole.right = hit->left;
        } else {
            hole.bottom = hit->top;
        }
    }
    return {};
}

// Physical pixels. The dialog fills the top-left work-area rectangle that no
// painted board covers. Qt geometry is logical; only a window whose size
// already matches devicePixelRatio is mapped. A failed measurement does not
// invent a second rectangle.
RECT boardHole(PageSession& pages)
{
    QWindow* host = pages.window();
    const HWND hwnd = host ? reinterpret_cast<HWND>(host->winId()) : nullptr;
    if (!host || !hwnd) {
        return {};
    }
    RECT phys{};
    if (!GetWindowRect(hwnd, &phys) || phys.right <= phys.left || phys.bottom <= phys.top) {
        return {};
    }
    const QRect logical = host->geometry();
    if (logical.width() < 1 || logical.height() < 1) {
        return {};
    }
    const double dpr = host->devicePixelRatio();
    if (!(dpr > 0.0)) {
        return {};
    }
    const bool synced = qAbs((phys.right - phys.left) - logical.width() * dpr) <= 2.0
                        && qAbs((phys.bottom - phys.top) - logical.height() * dpr) <= 2.0;
    if (!synced) {
        return {};
    }
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(mon, &info)) {
        return {};
    }
    const RECT& work = info.rcWork;
    if (work.right <= work.left || work.bottom <= work.top) {
        return {};
    }
    auto mapX = [&](double x) { return phys.left + (x - logical.left()) * dpr; };
    auto mapY = [&](double y) { return phys.top + (y - logical.top()) * dpr; };
    std::vector<PxBox> boards;
    for (const PageGridPaint& grid : pages.gridPaints()) {
        const QRectF box = grid.visual;
        if (box.isEmpty()) {
            continue;
        }
        PxBox o;
        o.left = static_cast<int>(std::floor(mapX(box.left())));
        o.top = static_cast<int>(std::floor(mapY(box.top())));
        o.right = static_cast<int>(std::ceil(mapX(box.right())));
        o.bottom = static_cast<int>(std::ceil(mapY(box.bottom())));
        if (o.right <= work.left || o.left >= work.right || o.bottom <= work.top
            || o.top >= work.bottom) {
            continue;
        }
        o.left = qMax(o.left, static_cast<int>(work.left));
        o.top = qMax(o.top, static_cast<int>(work.top));
        o.right = qMin(o.right, static_cast<int>(work.right));
        o.bottom = qMin(o.bottom, static_cast<int>(work.bottom));
        if (o.right > o.left && o.bottom > o.top) {
            boards.push_back(o);
        }
    }
    if (boards.empty()) {
        return {};
    }
    const RECT hole = holeBesideBoards(work, boards);
    if (hole.right - hole.left < 80 || hole.bottom - hole.top < 80) {
        return {};
    }
    return hole;
}

// The shell restores its saved size after the first move. This proc forces
// the board hole on WM_WINDOWPOSCHANGING and does not change z-order.
class DialogEvents final : public IFileDialogEvents {
public:
    explicit DialogEvents(RECT free)
        : m_free(free)
    {
    }

    ~DialogEvents() { unpin(); }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (!object) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == IID_IFileDialogEvents) {
            *object = static_cast<IFileDialogEvents*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG left = --m_refs;
        if (left == 0) {
            delete this;
        }
        return left;
    }

    HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog*) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog* dialog, IShellItem*) override
    {
        place(dialog);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog* dialog) override
    {
        place(dialog);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog* dialog) override
    {
        place(dialog);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog*, IShellItem*,
                                               FDE_SHAREVIOLATION_RESPONSE* response) override
    {
        if (response) {
            *response = FDESVR_DEFAULT;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog* dialog) override
    {
        place(dialog);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog*, IShellItem*,
                                          FDE_OVERWRITE_RESPONSE* response) override
    {
        if (response) {
            *response = FDEOR_DEFAULT;
        }
        return S_OK;
    }

private:
    void place(IFileDialog* dialog)
    {
        if (m_placed || !dialog) {
            return;
        }
        IOleWindow* ole = nullptr;
        if (FAILED(dialog->QueryInterface(IID_IOleWindow, reinterpret_cast<void**>(&ole))) || !ole) {
            return;
        }
        HWND hwnd = nullptr;
        const HRESULT got = ole->GetWindow(&hwnd);
        ole->Release();
        if (FAILED(got) || !hwnd) {
            return;
        }
        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (!root) {
            root = hwnd;
        }
        RECT wr{};
        if (!GetWindowRect(root, &wr)) {
            return;
        }
        if ((wr.right - wr.left) < 80 || (wr.bottom - wr.top) < 80) {
            return;
        }
        const int w = static_cast<int>(m_free.right - m_free.left);
        const int h = static_cast<int>(m_free.bottom - m_free.top);
        if (w < 80 || h < 80) {
            return;
        }
        // Keep this size. The gaze host is clipped to its painted pieces, so
        // this window is not under that HWND. Do not change z-order.
        pin(root);
        SetWindowPos(root, nullptr, m_free.left, m_free.top, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        m_placed = true;
    }

    void unpin()
    {
        if (s_pin != this) {
            return;
        }
        if (m_previous && m_hwnd && IsWindow(m_hwnd)) {
            SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_previous));
        }
        m_previous = nullptr;
        m_hwnd = nullptr;
        s_pin = nullptr;
    }

    void pin(HWND root)
    {
        if (s_pin && s_pin != this) {
            s_pin->unpin();
        }
        if (s_pin == this && m_hwnd == root && m_previous) {
            return;
        }
        if (s_pin == this) {
            unpin();
        }
        m_hwnd = root;
        m_previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            root, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&DialogEvents::pinProc)));
        s_pin = this;
    }

    static LRESULT CALLBACK pinProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        DialogEvents* self = s_pin;
        const WNDPROC previous = self ? self->m_previous : nullptr;
        if (!self || self->m_reentry) {
            return previous ? CallWindowProcW(previous, hwnd, msg, wp, lp)
                            : DefWindowProcW(hwnd, msg, wp, lp);
        }
        if (msg == WM_WINDOWPOSCHANGING) {
            const int width = self->m_free.right - self->m_free.left;
            const int height = self->m_free.bottom - self->m_free.top;
            self->m_reentry = true;
            const LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wp, lp)
                                            : DefWindowProcW(hwnd, msg, wp, lp);
            self->m_reentry = false;
            auto* pos = reinterpret_cast<WINDOWPOS*>(lp);
            if (pos && width >= 80 && height >= 80) {
                pos->x = self->m_free.left;
                pos->y = self->m_free.top;
                pos->cx = width;
                pos->cy = height;
                pos->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
            }
            return result;
        }
        if (msg == WM_NCDESTROY) {
            self->unpin();
            return previous ? CallWindowProcW(previous, hwnd, msg, wp, lp)
                            : DefWindowProcW(hwnd, msg, wp, lp);
        }
        return previous ? CallWindowProcW(previous, hwnd, msg, wp, lp)
                        : DefWindowProcW(hwnd, msg, wp, lp);
    }

    static DialogEvents* s_pin;
    WNDPROC m_previous = nullptr;
    HWND m_hwnd = nullptr;
    RECT m_free{};
    bool m_placed = false;
    bool m_reentry = false;
    ULONG m_refs = 1;
};

DialogEvents* DialogEvents::s_pin = nullptr;

struct DialogRequest {
    bool save = false;
    /// Physical pixels. Computed on the gaze thread. Not an owner HWND:
    /// Show(owner) would disable that window and SendMessage it from this loop.
    RECT free{};
    std::wstring title;
    std::wstring fileName;
    std::wstring extension;
    std::wstring directory;
    std::vector<std::wstring> labels;
    std::vector<std::wstring> specs;
};

struct DialogResult {
    HRESULT shown = E_FAIL;
    std::wstring path;
};

void applyRequest(IFileDialog* dialog, const DialogRequest& request)
{
    if (!request.title.empty()) {
        dialog->SetTitle(request.title.c_str());
    }
    if (!request.fileName.empty()) {
        dialog->SetFileName(request.fileName.c_str());
    }
    if (!request.extension.empty()) {
        dialog->SetDefaultExtension(request.extension.c_str());
    }
    if (!request.directory.empty()) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(request.directory.c_str(), nullptr, IID_IShellItem,
                                                  reinterpret_cast<void**>(&folder)))
            && folder) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    std::vector<COMDLG_FILTERSPEC> native;
    native.reserve(request.labels.size());
    const size_t count = request.labels.size() < request.specs.size() ? request.labels.size()
                                                                     : request.specs.size();
    for (size_t i = 0; i < count; ++i) {
        if (request.labels[i].empty() || request.specs[i].empty()) {
            continue;
        }
        COMDLG_FILTERSPEC spec;
        spec.pszName = request.labels[i].c_str();
        spec.pszSpec = request.specs[i].c_str();
        native.push_back(spec);
    }
    if (!native.empty()) {
        dialog->SetFileTypes(static_cast<UINT>(native.size()), native.data());
        dialog->SetFileTypeIndex(1);
    }
    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        options |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST;
        if (request.save) {
            options |= FOS_OVERWRITEPROMPT;
        } else {
            options |= FOS_FILEMUSTEXIST;
        }
        dialog->SetOptions(options);
    }
}

std::wstring resultPath(IFileDialog* dialog)
{
    IShellItem* item = nullptr;
    if (FAILED(dialog->GetResult(&item)) || !item) {
        return {};
    }
    PWSTR raw = nullptr;
    const HRESULT hr = item->GetDisplayName(SIGDN_FILESYSPATH, &raw);
    std::wstring path;
    if (SUCCEEDED(hr) && raw) {
        path = raw;
        CoTaskMemFree(raw);
    }
    item->Release();
    return path;
}

// Show() is modal on this STA thread. The gaze thread does not wait.
void showOnStaThread(const DialogRequest& request, DialogResult& result)
{
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init)) {
        result.shown = init;
        return;
    }
    struct ComGuard {
        ~ComGuard() { CoUninitialize(); }
    } com;

    IFileOpenDialog* open = nullptr;
    IFileSaveDialog* saveDialog = nullptr;
    const HRESULT created =
        request.save ? CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_IFileSaveDialog, reinterpret_cast<void**>(&saveDialog))
                     : CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_IFileOpenDialog, reinterpret_cast<void**>(&open));
    IFileDialog* dialog = request.save ? static_cast<IFileDialog*>(saveDialog)
                                       : static_cast<IFileDialog*>(open);
    if (FAILED(created) || !dialog) {
        result.shown = FAILED(created) ? created : E_FAIL;
        return;
    }

    applyRequest(dialog, request);
    auto* events = new DialogEvents(request.free);
    DWORD cookie = 0;
    const HRESULT advised = dialog->Advise(events, &cookie);
    result.shown = dialog->Show(nullptr);
    if (SUCCEEDED(advised)) {
        dialog->Unadvise(cookie);
    }
    if (result.shown == S_OK) {
        result.path = resultPath(dialog);
    }
    events->Release();
    if (saveDialog) {
        saveDialog->Release();
    }
    if (open) {
        open->Release();
    }
}

class FileDialogThread final : public QThread {
public:
    using Done = std::function<void(HRESULT, std::wstring)>;

    FileDialogThread(DialogRequest request, Done done)
        : m_request(std::move(request))
        , m_done(std::move(done))
    {
    }

protected:
    void run() override
    {
        DialogResult result;
        showOnStaThread(m_request, result);
        const HRESULT shown = result.shown;
        const std::wstring path = std::move(result.path);
        Done done = std::move(m_done);
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [done = std::move(done), shown, path]() {
                if (done) {
                    done(shown, path);
                }
            },
            Qt::QueuedConnection);
    }

private:
    DialogRequest m_request;
    Done m_done;
};

void deliverDialogResult(PageSession* pages, const OsFileDialogDone& done, HRESULT shown,
                         const std::wstring& path)
{
    if (pages) {
        pages->endExclusivePage();
    }
    if (!done) {
        return;
    }
    if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        done({}, {});
        return;
    }
    if (FAILED(shown)) {
        done({}, QStringLiteral("Could not open the file dialog"));
        return;
    }
    if (path.empty()) {
        done({}, QStringLiteral("Could not read the chosen file"));
        return;
    }
    const QString chosen = QString::fromStdWString(path);
    GAZER_INFO << "File dialog closed" << chosen;
    done(chosen, {});
}

} // namespace

#endif

void runOsFileDialog(PageSession& pages, const OsFileRequest& request, OsFileDialogDone done)
{
    auto failLater = [done](const QString& msg) {
        if (!done) {
            return;
        }
        QTimer::singleShot(0, QCoreApplication::instance(), [done, msg]() { done({}, msg); });
    };

#ifndef Q_OS_WIN
    Q_UNUSED(pages);
    Q_UNUSED(request);
    failLater(QStringLiteral("File dialogs need Windows"));
#else
    QString why;
    if (!pages.beginExclusivePage(QString::fromLatin1(kOsFilePageId), &why)) {
        failLater(why);
        return;
    }

    DialogRequest job;
    job.save = request.kind == OsFileRequest::Kind::Save;
    job.free = boardHole(pages);
    job.title = request.title.toStdWString();
    job.fileName = request.fileName.toStdWString();
    job.extension = request.defaultExtension.toStdWString();
    job.directory = request.directory.toStdWString();
    job.labels.reserve(static_cast<size_t>(request.types.size()));
    job.specs.reserve(static_cast<size_t>(request.types.size()));
    for (const OsFileType& type : request.types) {
        job.labels.push_back(type.label.toStdWString());
        job.specs.push_back(type.spec.toStdWString());
    }

    const bool save = job.save;
    QPointer<PageSession> session(&pages);
    // Next turn, so Show starts after the dwell signal that requested it.
    QTimer::singleShot(0, QCoreApplication::instance(),
                       [job = std::move(job), done, session, save]() mutable {
                           auto* thread = new FileDialogThread(
                               std::move(job), [done, session](HRESULT shown, std::wstring path) {
                                   deliverDialogResult(session.data(), done, shown, path);
                               });
                           QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
                           GAZER_INFO << (save ? "Save" : "Open") << "file dialog";
                           thread->start();
                       });
#endif
}

} // namespace gazer
