#include "input/PixelScroller.h"

#include "input/MouseInjector.h"
#include "utils/Log.h"

#include <memory>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <commctrl.h>
#  include <ole2.h>
#  include <UIAutomationClient.h>
#  include <wchar.h>
#endif

namespace gazer {

#ifdef Q_OS_WIN

namespace {

constexpr double kUiaNoScroll = -1.0;
constexpr UINT kMsgTimeoutMs = 40;

template<typename T>
void comRelease(T*& p)
{
    if (p) {
        p->Release();
        p = nullptr;
    }
}

LRESULT sendTimeout(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DWORD_PTR result = 0;
    SendMessageTimeoutW(hwnd, msg, wParam, lParam, SMTO_ABORTIFHUNG | SMTO_NORMAL, kMsgTimeoutMs,
                        &result);
    return static_cast<LRESULT>(result);
}

bool classIs(HWND hwnd, const wchar_t* name)
{
    wchar_t cls[256]{};
    return hwnd && GetClassNameW(hwnd, cls, 256) > 0 && _wcsicmp(cls, name) == 0;
}

bool classStartsWith(HWND hwnd, const wchar_t* prefix)
{
    wchar_t cls[256]{};
    if (!hwnd || GetClassNameW(hwnd, cls, 256) <= 0) {
        return false;
    }
    return _wcsnicmp(cls, prefix, wcslen(prefix)) == 0;
}

HWND deepestChild(HWND root, POINT screen)
{
    HWND cur = root;
    for (int i = 0; i < 24; ++i) {
        POINT p = screen;
        if (!ScreenToClient(cur, &p)) {
            break;
        }
        HWND child = ChildWindowFromPointEx(cur, p, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
        if (!child || child == cur) {
            break;
        }
        cur = child;
    }
    return cur;
}

struct FindCtx {
    POINT pt{};
    DWORD excludePid = 0;
    HWND result = nullptr;
};

BOOL CALLBACK enumTopLevel(HWND hwnd, LPARAM lp)
{
    auto* ctx = reinterpret_cast<FindCtx*>(lp);
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ctx->excludePid) {
        return TRUE;
    }
    RECT r{};
    if (!GetWindowRect(hwnd, &r) || !PtInRect(&r, ctx->pt)) {
        return TRUE;
    }
    ctx->result = deepestChild(hwnd, ctx->pt);
    return FALSE;
}

HWND windowUnderCursor()
{
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return nullptr;
    }
    FindCtx ctx;
    ctx.pt = pt;
    ctx.excludePid = GetCurrentProcessId();
    EnumWindows(enumTopLevel, reinterpret_cast<LPARAM>(&ctx));
    return ctx.result;
}

double dpiScale(HWND hwnd)
{
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) {
            return nullptr;
        }
        return reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
    }();
    UINT dpi = 96;
    if (fn && hwnd) {
        dpi = fn(hwnd);
    }
    if (dpi == 0) {
        dpi = 96;
    }
    return double(dpi) / 96.0;
}

int takeWhole(double& acc)
{
    const int v = int(acc);
    acc -= v;
    return v;
}

bool looksLikePixelBar(const SCROLLINFO& si, int clientExtent)
{
    if (si.nPage == 0 || clientExtent <= 0) {
        return false;
    }
    return si.nPage >= UINT(clientExtent) * 2 / 3;
}

int maxScrollPos(const SCROLLINFO& si)
{
    const int page = int(si.nPage);
    int maxPos = si.nMax - (page > 0 ? page - 1 : 0);
    if (maxPos < si.nMin) {
        maxPos = si.nMin;
    }
    return maxPos;
}

bool usesHighResWheel(HWND hwnd)
{
    HWND root = hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
    if (!root) {
        root = hwnd;
    }
    if (classStartsWith(root, L"Chrome_") || classStartsWith(hwnd, L"Chrome_")) {
        return true;
    }
    if (classIs(root, L"MozillaWindowClass") || classStartsWith(hwnd, L"Mozilla")) {
        return true;
    }
    return false;
}

IUIAutomationScrollPattern* patternFromElement(IUIAutomationElement* el)
{
    if (!el) {
        return nullptr;
    }
    IUIAutomationScrollPattern* sp = nullptr;
    if (FAILED(el->GetCurrentPatternAs(UIA_ScrollPatternId, IID_IUIAutomationScrollPattern,
                                       reinterpret_cast<void**>(&sp)))
        || !sp) {
        return nullptr;
    }
    BOOL vs = FALSE;
    BOOL hs = FALSE;
    sp->get_CurrentVerticallyScrollable(&vs);
    sp->get_CurrentHorizontallyScrollable(&hs);
    if (!vs && !hs) {
        sp->Release();
        return nullptr;
    }
    return sp;
}

} // namespace

struct PixelScroller::Impl {
    struct UiaState {
        IUIAutomation* automation = nullptr;
        IUIAutomationTreeWalker* walker = nullptr;
        IUIAutomationElement* element = nullptr;
        IUIAutomationScrollPattern* scroll = nullptr;
        HWND hwnd = nullptr;
        HWND noPatternHwnd = nullptr;
        bool initTried = false;
        bool comInit = false;
    };

    UiaState uia;
    HWND thumbVert = nullptr;
    HWND thumbHorz = nullptr;
    double fracX = 0.0;
    double fracY = 0.0;
    double wheelFracV = 0.0;
    double wheelFracH = 0.0;

    ~Impl()
    {
        reset();
        comRelease(uia.walker);
        comRelease(uia.automation);
        if (uia.comInit) {
            CoUninitialize();
            uia.comInit = false;
        }
    }

    void endThumb(HWND& hwnd, UINT msg)
    {
        if (!hwnd || !IsWindow(hwnd)) {
            hwnd = nullptr;
            return;
        }
        sendTimeout(hwnd, msg, MAKEWPARAM(SB_ENDSCROLL, 0), 0);
        hwnd = nullptr;
    }

    void clearUiaElement()
    {
        comRelease(uia.scroll);
        comRelease(uia.element);
        uia.hwnd = nullptr;
    }

    bool ensureUia()
    {
        if (uia.automation) {
            return true;
        }
        if (uia.initTried) {
            return false;
        }
        uia.initTried = true;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (hr == S_OK) {
            uia.comInit = true;
        } else if (FAILED(hr) && hr != RPC_E_CHANGED_MODE && hr != S_FALSE) {
            GAZER_WARN << "PixelScroller: CoInitializeEx failed" << Qt::hex << unsigned(hr);
            return false;
        }

        hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                              reinterpret_cast<void**>(&uia.automation));
        if (FAILED(hr) || !uia.automation) {
            GAZER_WARN << "PixelScroller: UI Automation unavailable";
            return false;
        }

        IUIAutomation2* a2 = nullptr;
        if (SUCCEEDED(uia.automation->QueryInterface(IID_IUIAutomation2,
                                                     reinterpret_cast<void**>(&a2)))
            && a2) {
            a2->put_AutoSetFocus(FALSE);
            a2->put_ConnectionTimeout(80);
            a2->put_TransactionTimeout(80);
            a2->Release();
        }
        uia.automation->get_ControlViewWalker(&uia.walker);
        return true;
    }

    bool applyPixelBar(HWND hwnd, int bar, int delta, HWND* thumbSlot, UINT scrollMsg)
    {
        if (delta == 0) {
            return true;
        }
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        if (!GetScrollInfo(hwnd, bar, &si)) {
            return false;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int extent = (bar == SB_HORZ) ? (rc.right - rc.left) : (rc.bottom - rc.top);
        if (!looksLikePixelBar(si, extent)) {
            return false;
        }
        const int lo = si.nMin;
        const int hi = maxScrollPos(si);
        if (hi > 0xffff) {
            return false;
        }
        int next = (bar == SB_VERT) ? (si.nPos - delta) : (si.nPos + delta);
        if (next < lo) {
            next = lo;
        }
        if (next > hi) {
            next = hi;
        }
        if (next == si.nPos) {
            return true;
        }
        si.nPos = next;
        si.nTrackPos = next;
        si.fMask = SIF_POS | SIF_TRACKPOS;
        SetScrollInfo(hwnd, bar, &si, TRUE);
        const WPARAM track = MAKEWPARAM(SB_THUMBTRACK, WORD(next));
        const WPARAM pos = MAKEWPARAM(SB_THUMBPOSITION, WORD(next));
        sendTimeout(hwnd, scrollMsg, track, 0);
        sendTimeout(hwnd, scrollMsg, pos, 0);
        *thumbSlot = hwnd;
        return true;
    }

    bool nativeScroll(HWND start, int* dx, int* dy)
    {
        HWND hwnd = start;
        if (classIs(hwnd, L"ScrollBar")) {
            HWND parent = GetAncestor(hwnd, GA_PARENT);
            if (parent) {
                hwnd = parent;
            }
        }

        for (int hop = 0; hop < 8 && hwnd && (*dx != 0 || *dy != 0); ++hop) {
            if (classIs(hwnd, L"SysListView32")) {
                sendTimeout(hwnd, LVM_SCROLL, WPARAM(int(*dx)), LPARAM(int(-*dy)));
                *dx = 0;
                *dy = 0;
                return true;
            }
            const bool v = applyPixelBar(hwnd, SB_VERT, *dy, &thumbVert, WM_VSCROLL);
            const bool h = applyPixelBar(hwnd, SB_HORZ, *dx, &thumbHorz, WM_HSCROLL);
            if (v) {
                *dy = 0;
            }
            if (h) {
                *dx = 0;
            }
            if (*dx == 0 && *dy == 0) {
                return true;
            }
            HWND parent = GetAncestor(hwnd, GA_PARENT);
            if (!parent || parent == GetDesktopWindow() || parent == hwnd) {
                break;
            }
            hwnd = parent;
        }
        return *dx == 0 && *dy == 0;
    }

    bool cacheScrollPattern(HWND hwnd, POINT pt)
    {
        if (uia.scroll && uia.hwnd == hwnd) {
            return true;
        }
        clearUiaElement();
        if (uia.noPatternHwnd == hwnd) {
            return false;
        }
        if (!ensureUia()) {
            return false;
        }

        IUIAutomationElement* el = nullptr;
        if (FAILED(uia.automation->ElementFromPoint(pt, &el)) || !el) {
            uia.noPatternHwnd = hwnd;
            return false;
        }

        IUIAutomationScrollPattern* found = nullptr;
        IUIAutomationElement* foundEl = nullptr;
        for (int hop = 0; hop < 12 && el; ++hop) {
            if (IUIAutomationScrollPattern* sp = patternFromElement(el)) {
                found = sp;
                foundEl = el;
                el = nullptr;
                break;
            }
            IUIAutomationElement* parent = nullptr;
            if (uia.walker) {
                uia.walker->GetParentElement(el, &parent);
            }
            el->Release();
            el = parent;
        }
        comRelease(el);

        if (!found) {
            uia.noPatternHwnd = hwnd;
            return false;
        }
        uia.element = foundEl;
        uia.scroll = found;
        uia.hwnd = hwnd;
        uia.noPatternHwnd = nullptr;
        return true;
    }

    bool uiaAxis(IUIAutomationScrollPattern* sp, bool vertical, int pixels, double* percentOut)
    {
        WINBOOL scrollable = FALSE;
        double view = 0.0;
        double pct = 0.0;
        RECT bbox{};
        if (vertical) {
            sp->get_CurrentVerticallyScrollable(&scrollable);
            sp->get_CurrentVerticalViewSize(&view);
            sp->get_CurrentVerticalScrollPercent(&pct);
        } else {
            sp->get_CurrentHorizontallyScrollable(&scrollable);
            sp->get_CurrentHorizontalViewSize(&view);
            sp->get_CurrentHorizontalScrollPercent(&pct);
        }
        if (!scrollable || view <= 0.5 || view >= 99.5 || pct < 0.0) {
            return false;
        }
        if (uia.element) {
            uia.element->get_CurrentBoundingRectangle(&bbox);
        }
        const int extent = vertical ? (bbox.bottom - bbox.top) : (bbox.right - bbox.left);
        if (extent < 8) {
            return false;
        }
        const double rangePx = double(extent) * (100.0 - view) / view;
        if (rangePx < 1.0) {
            return false;
        }
        const double deltaPct = (vertical ? -pixels : pixels) * 100.0 / rangePx;
        double next = pct + deltaPct;
        if (next < 0.0) {
            next = 0.0;
        }
        if (next > 100.0) {
            next = 100.0;
        }
        *percentOut = next;
        return true;
    }

    bool uiaScroll(HWND hwnd, int* dx, int* dy)
    {
        POINT pt{};
        if (!GetCursorPos(&pt)) {
            return false;
        }
        if (!cacheScrollPattern(hwnd, pt) || !uia.scroll) {
            return false;
        }

        double vPct = kUiaNoScroll;
        double hPct = kUiaNoScroll;
        bool didV = false;
        bool didH = false;
        if (*dy != 0) {
            didV = uiaAxis(uia.scroll, true, *dy, &vPct);
        }
        if (*dx != 0) {
            didH = uiaAxis(uia.scroll, false, *dx, &hPct);
        }
        if (!didV && !didH) {
            return false;
        }
        if (FAILED(uia.scroll->SetScrollPercent(hPct, vPct))) {
            clearUiaElement();
            return false;
        }
        if (didV) {
            *dy = 0;
        }
        if (didH) {
            *dx = 0;
        }
        return *dx == 0 && *dy == 0;
    }

    bool wheelLeftover(int dx, int dy, double scale, QString* error)
    {
        if (dx == 0 && dy == 0) {
            return true;
        }
        const double pxPerUnit =
            PixelScroller::kPixelsPerNotch * (scale > 0.1 ? scale : 1.0) / 120.0;
        if (pxPerUnit <= 0.0) {
            return false;
        }
        wheelFracV += double(dy) / pxPerUnit;
        wheelFracH += double(dx) / pxPerUnit;
        const int v = takeWhole(wheelFracV);
        const int h = takeWhole(wheelFracH);
        bool any = true;
        if (v != 0) {
            any = MouseInjector::scrollDelta(v, error) && any;
        }
        if (h != 0) {
            any = MouseInjector::scrollHorizontalDelta(h, error) && any;
        }
        return any;
    }

    bool scrollBy(int dx, int dy, QString* error)
    {
        if (dx == 0 && dy == 0) {
            return true;
        }

        HWND hwnd = windowUnderCursor();
        const double scale = dpiScale(hwnd);
        fracX += double(dx) * scale;
        fracY += double(dy) * scale;
        int px = takeWhole(fracX);
        int py = takeWhole(fracY);
        if (px == 0 && py == 0) {
            return true;
        }

        int rx = px;
        int ry = py;
        if (hwnd) {
            nativeScroll(hwnd, &rx, &ry);
            if (rx == 0 && ry == 0) {
                return true;
            }
            if (!usesHighResWheel(hwnd)) {
                uiaScroll(hwnd, &rx, &ry);
                if (rx == 0 && ry == 0) {
                    return true;
                }
            }
        }
        return wheelLeftover(rx, ry, scale, error);
    }

    void lift()
    {
        endThumb(thumbVert, WM_VSCROLL);
        endThumb(thumbHorz, WM_HSCROLL);
    }

    void reset()
    {
        lift();
        clearUiaElement();
        uia.noPatternHwnd = nullptr;
        fracX = 0.0;
        fracY = 0.0;
        wheelFracV = 0.0;
        wheelFracH = 0.0;
    }
};

PixelScroller::PixelScroller()
    : d(std::make_unique<Impl>())
{
}

PixelScroller::~PixelScroller() = default;

bool PixelScroller::scrollBy(int dx, int dy, QString* error)
{
    return d->scrollBy(dx, dy, error);
}

void PixelScroller::lift()
{
    d->lift();
}

void PixelScroller::reset()
{
    d->reset();
}

#else

struct PixelScroller::Impl {};

PixelScroller::PixelScroller()
    : d(std::make_unique<Impl>())
{
}

PixelScroller::~PixelScroller() = default;

bool PixelScroller::scrollBy(int, int, QString* error)
{
    if (error) {
        *error = QStringLiteral("Pixel scroll only supported on Windows");
    }
    return false;
}

void PixelScroller::lift() {}

void PixelScroller::reset() {}

#endif

} // namespace gazer
