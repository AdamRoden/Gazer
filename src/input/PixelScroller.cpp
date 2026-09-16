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
constexpr double kUiaMinPctPerPx = 0.5;
constexpr UINT kMsgTimeoutMs = 40;
// Scintilla.h — Notepad++ and other SCI hosts. Do not include the full header.
constexpr UINT kSciLineScroll = 2168;
constexpr UINT kSciTextHeight = 2279;
constexpr UINT kSciSetXOffset = 2397;
constexpr UINT kSciGetXOffset = 2398;

enum class ScrollKind {
    HighResWheel,
    Scintilla,
    ListView,
    Fallback,
};

struct Target {
    ScrollKind kind = ScrollKind::Fallback;
    HWND hwnd = nullptr;
};

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
    const double s = double(dpi) / 96.0;
    return s > 0.1 ? s : 1.0;
}

int takeTowardZero(double& acc)
{
    const int v = int(acc);
    acc -= v;
    return v;
}

int takeDevicePx(double& logical, double scale)
{
    double device = logical * scale;
    const int px = takeTowardZero(device);
    logical = device / scale;
    return px;
}

void putDevicePx(double& logical, int px, double scale)
{
    logical += double(px) / scale;
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

HWND parentWindow(HWND hwnd)
{
    HWND parent = GetAncestor(hwnd, GA_PARENT);
    if (!parent || parent == GetDesktopWindow() || parent == hwnd) {
        return nullptr;
    }
    return parent;
}

bool usesHighResWheel(HWND hwnd)
{
    wchar_t cls[256]{};
    for (HWND h = hwnd; h; h = parentWindow(h)) {
        if (GetClassNameW(h, cls, 256) > 0
            && classLooksLikeHighResWheel(QString::fromWCharArray(cls))) {
            return true;
        }
    }
    return false;
}

HWND skipScrollbar(HWND hwnd)
{
    if (classIs(hwnd, L"ScrollBar")) {
        if (HWND parent = parentWindow(hwnd)) {
            return parent;
        }
    }
    return hwnd;
}

Target resolveTarget(HWND start)
{
    Target t;
    t.hwnd = start;
    if (!start) {
        return t;
    }
    if (usesHighResWheel(start)) {
        t.kind = ScrollKind::HighResWheel;
        return t;
    }
    HWND hwnd = skipScrollbar(start);
    for (int hop = 0; hop < 8 && hwnd; ++hop) {
        if (classIs(hwnd, L"SysListView32")) {
            t.kind = ScrollKind::ListView;
            t.hwnd = hwnd;
            return t;
        }
        if (classStartsWith(hwnd, L"Scintilla")) {
            t.kind = ScrollKind::Scintilla;
            t.hwnd = hwnd;
            return t;
        }
        hwnd = parentWindow(hwnd);
    }
    return t;
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

bool axisMetrics(IUIAutomationScrollPattern* sp, IUIAutomationElement* el, bool vertical,
                 double* pct, double* pctPerPx)
{
    WINBOOL scrollable = FALSE;
    double view = 0.0;
    double p = 0.0;
    RECT bbox{};
    if (vertical) {
        sp->get_CurrentVerticallyScrollable(&scrollable);
        sp->get_CurrentVerticalViewSize(&view);
        sp->get_CurrentVerticalScrollPercent(&p);
    } else {
        sp->get_CurrentHorizontallyScrollable(&scrollable);
        sp->get_CurrentHorizontalViewSize(&view);
        sp->get_CurrentHorizontalScrollPercent(&p);
    }
    if (!scrollable || view <= 0.5 || view >= 99.5 || p < 0.0) {
        return false;
    }
    if (el) {
        el->get_CurrentBoundingRectangle(&bbox);
    }
    const int extent = vertical ? (bbox.bottom - bbox.top) : (bbox.right - bbox.left);
    if (extent < 8) {
        return false;
    }
    const double rangePx = double(extent) * (100.0 - view) / view;
    if (rangePx < 1.0) {
        return false;
    }
    *pct = p;
    *pctPerPx = 100.0 / rangePx;
    return true;
}

bool uiaTooCoarse(IUIAutomationScrollPattern* sp, IUIAutomationElement* el)
{
    double pct = 0.0;
    double pctPerPx = 0.0;
    if (axisMetrics(sp, el, true, &pct, &pctPerPx) && pctPerPx < kUiaMinPctPerPx) {
        return true;
    }
    if (axisMetrics(sp, el, false, &pct, &pctPerPx) && pctPerPx < kUiaMinPctPerPx) {
        return true;
    }
    return false;
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
    HWND cachedHwnd = nullptr;
    ScrollKind cachedKind = ScrollKind::Fallback;
    double remX = 0.0;
    double remY = 0.0;

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
        const int bar = (msg == WM_VSCROLL) ? SB_VERT : SB_HORZ;
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS | SIF_TRACKPOS;
        int pos = 0;
        if (GetScrollInfo(hwnd, bar, &si)) {
            pos = si.nPos;
        }
        sendTimeout(hwnd, msg, MAKEWPARAM(SB_THUMBPOSITION, WORD(pos)), 0);
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
        // Live drag is THUMBTRACK only. THUMBPOSITION every tick makes many
        // apps snap/release the thumb, which shakes the window up and down.
        sendTimeout(hwnd, scrollMsg, MAKEWPARAM(SB_THUMBTRACK, WORD(next)), 0);
        *thumbSlot = hwnd;
        return true;
    }

    bool drainWheel(QString* error, int minAbsUnits)
    {
        const int v = takeWheelUnits(remY, minAbsUnits);
        const int h = takeWheelUnits(remX, minAbsUnits);
        if (v == 0 && h == 0) {
            return true;
        }
        return MouseInjector::scrollWheelRaw(h, v, error);
    }

    Target cachedTarget()
    {
        if (cachedHwnd && IsWindow(cachedHwnd)) {
            Target t;
            t.kind = cachedKind;
            t.hwnd = cachedHwnd;
            return t;
        }
        Target t = resolveTarget(windowUnderCursor());
        cachedHwnd = t.hwnd;
        cachedKind = t.kind;
        return t;
    }

    bool drainListView(HWND hwnd)
    {
        const double scale = dpiScale(hwnd);
        const int dx = takeDevicePx(remX, scale);
        const int dy = takeDevicePx(remY, scale);
        if (dx != 0 || dy != 0) {
            sendTimeout(hwnd, LVM_SCROLL, WPARAM(dx), LPARAM(-dy));
        }
        return true;
    }

    bool drainScintilla(HWND hwnd)
    {
        const double scale = dpiScale(hwnd);
        const int dx = takeDevicePx(remX, scale);
        if (dx != 0) {
            const int xOff = int(sendTimeout(hwnd, kSciGetXOffset, 0, 0));
            int next = xOff + dx;
            if (next < 0) {
                next = 0;
            }
            sendTimeout(hwnd, kSciSetXOffset, WPARAM(next), 0);
        }

        int lineH = int(sendTimeout(hwnd, kSciTextHeight, 0, 0));
        if (lineH < 4 || lineH > 256) {
            lineH = 16;
        }
        const double lineLogical = double(lineH) / scale;
        const int lines = int(remY / lineLogical);
        remY -= double(lines) * lineLogical;
        if (lines != 0) {
            // Positive remY = away (up). Positive SCI_LINESCROLL = later lines.
            sendTimeout(hwnd, kSciLineScroll, 0, LPARAM(-lines));
        }
        return true;
    }

    void drainPixelBars(HWND start, int* dx, int* dy)
    {
        HWND hwnd = skipScrollbar(start);
        for (int hop = 0; hop < 8 && hwnd && (*dx != 0 || *dy != 0); ++hop) {
            const bool v = applyPixelBar(hwnd, SB_VERT, *dy, &thumbVert, WM_VSCROLL);
            const bool h = applyPixelBar(hwnd, SB_HORZ, *dx, &thumbHorz, WM_HSCROLL);
            if (v) {
                *dy = 0;
            }
            if (h) {
                *dx = 0;
            }
            hwnd = parentWindow(hwnd);
        }
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
        if (uiaTooCoarse(found, foundEl)) {
            found->Release();
            foundEl->Release();
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
        double pct = 0.0;
        double pctPerPx = 0.0;
        if (!axisMetrics(sp, uia.element, vertical, &pct, &pctPerPx)) {
            return false;
        }
        double next = pct + (vertical ? -pixels : pixels) * pctPerPx;
        if (next < 0.0) {
            next = 0.0;
        }
        if (next > 100.0) {
            next = 100.0;
        }
        if (next == pct) {
            return false;
        }
        *percentOut = next;
        return true;
    }

    bool drainUia(HWND hwnd, int* dx, int* dy)
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

    bool drainFallback(HWND hwnd, QString* error)
    {
        const double scale = dpiScale(hwnd);
        int dx = takeDevicePx(remX, scale);
        int dy = takeDevicePx(remY, scale);
        if (dx == 0 && dy == 0) {
            return true;
        }
        if (hwnd) {
            drainPixelBars(hwnd, &dx, &dy);
            if (dx == 0 && dy == 0) {
                return true;
            }
            drainUia(hwnd, &dx, &dy);
            if (dx == 0 && dy == 0) {
                return true;
            }
        }
        putDevicePx(remX, dx, scale);
        putDevicePx(remY, dy, scale);
        return drainWheel(error, 1);
    }

    bool scrollBy(double dx, double dy, QString* error)
    {
        if (dx == 0.0 && dy == 0.0) {
            return true;
        }
        remX += dx;
        remY += dy;

        const Target t = cachedTarget();
        switch (t.kind) {
        case ScrollKind::HighResWheel:
            return drainWheel(error, 1);
        case ScrollKind::Scintilla:
            return drainScintilla(t.hwnd);
        case ScrollKind::ListView:
            return drainListView(t.hwnd);
        case ScrollKind::Fallback:
            break;
        }
        return drainFallback(t.hwnd, error);
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
        cachedHwnd = nullptr;
        cachedKind = ScrollKind::Fallback;
        remX = 0.0;
        remY = 0.0;
    }
};

PixelScroller::PixelScroller()
    : d(std::make_unique<Impl>())
{
}

PixelScroller::~PixelScroller() = default;

bool PixelScroller::scrollBy(double dx, double dy, QString* error)
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

bool PixelScroller::scrollBy(double, double, QString* error)
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
