#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "..\\Headers\\Resource.h"

namespace {

const UINT_PTR kInteractionSubclassId = 0x50445721;
const UINT_PTR kFilterResizeSubclassId = 0x50445722;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
const UINT kShellCommandMessage = WM_APP + 0x52;
const wchar_t* kMainClassName = L"WinPDWWndClass";
const wchar_t* kMainInteractionProp = L"PDW.ModernInteractionInstalled";
const wchar_t* kFilterResizeProp = L"PDW.FilterResizeInstalled";

const int kShellHeight = 49;
const int kNavHeight = 24;
const int kCommandTop = 25;
const int kCommandHeight = 23;
const int kSettingsTarget = 5;
const int kCommandTargetBase = 100;
const int kSettingsPopupCommand = 50001;
const int kResumeMonitorCommand = 50002;

HHOOK g_callHook = NULL;
HHOOK g_mouseHook = NULL;
HWND g_mainWindow = NULL;
bool g_settingsMenuOpen = false;
bool g_settingsPressed = false;
bool g_closeMenuFromSettings = false;
int g_pressedTarget = -1;
int g_hoverTarget = -1;

HFONT g_shellTextFont = NULL;
HFONT g_shellBoldFont = NULL;
HFONT g_shellIconFont = NULL;

struct ShellTarget
{
    RECT rect;
    int command;
};

ShellTarget g_navTargets[6];
ShellTarget g_commandTargets[11];
int g_navTargetCount = 0;
int g_commandTargetCount = 0;

int ScaleForDpi(HWND hwnd, int value)
{
    UINT dpi = 96;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
        GetDpiForWindowFn getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
        if (getDpiForWindow) dpi = getDpiForWindow(hwnd);
    }
    return MulDiv(value, static_cast<int>(dpi), 96);
}

bool IsMainWindow(HWND hwnd)
{
    wchar_t className[64] = {};
    return GetClassNameW(hwnd, className, ARRAYSIZE(className)) > 0 &&
           lstrcmpW(className, kMainClassName) == 0;
}

bool IsFilterDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;
    return GetDlgItem(hwnd, IDC_FILTERS) != NULL;
}

HFONT GetShellTextFont()
{
    if (!g_shellTextFont)
        g_shellTextFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return g_shellTextFont;
}

HFONT GetShellBoldFont()
{
    if (!g_shellBoldFont)
        g_shellBoldFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return g_shellBoldFont;
}

HFONT GetShellIconFont()
{
    if (!g_shellIconFont)
        g_shellIconFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    return g_shellIconFont;
}

void FillRounded(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void BuildShellTargets(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    static const int navWidths[] = { 100, 92, 92, 82, 84, 108 };
    static const int navCommands[] = { 0, IDM_FILTERS, IDM_PLAYBACK, IDM_LOGFILE, IDM_MAIL, kSettingsPopupCommand };
    static const int commandWidths[] = { 76, 76, 80, 78, 98, 82, 88, 96, 80, 80, 82 };
    static const int commands[] = {
        IDM_PLAYBACK, IDM_COPY_SAVE, IDM_COPY_PRINT, IDM_COPY_SELECTION,
        kResumeMonitorCommand, IDT_TOOLBAR_BTN9, IDM_FILTERS, IDM_OPTIONS,
        IDM_MONSTAT, IDM_CLEARDISPLAY, IDT_TOOLBAR_BTN12
    };

    g_navTargetCount = 0;
    int x = 6;
    for (int i = 0; i < 6; ++i)
    {
        if (x + navWidths[i] >= client.right - 4) break;
        SetRect(&g_navTargets[g_navTargetCount].rect,
                x, 2, x + navWidths[i], kNavHeight - 1);
        g_navTargets[g_navTargetCount].command = navCommands[i];
        ++g_navTargetCount;
        x += navWidths[i] + 4;
    }

    g_commandTargetCount = 0;
    x = 8;
    for (int i = 0; i < 11; ++i)
    {
        if (x + commandWidths[i] >= client.right - 8) break;
        SetRect(&g_commandTargets[g_commandTargetCount].rect,
                x, kCommandTop + 1,
                x + commandWidths[i], kCommandTop + kCommandHeight - 1);
        g_commandTargets[g_commandTargetCount].command = commands[i];
        ++g_commandTargetCount;
        x += commandWidths[i] + 4;
    }
}

int HitTestShell(HWND hwnd, POINT clientPoint)
{
    BuildShellTargets(hwnd);
    for (int i = 0; i < g_navTargetCount; ++i)
        if (PtInRect(&g_navTargets[i].rect, clientPoint)) return i;
    for (int i = 0; i < g_commandTargetCount; ++i)
        if (PtInRect(&g_commandTargets[i].rect, clientPoint)) return kCommandTargetBase + i;
    return -1;
}

int CommandForTarget(int target)
{
    if (target >= 0 && target < g_navTargetCount)
        return g_navTargets[target].command;
    const int index = target - kCommandTargetBase;
    if (index >= 0 && index < g_commandTargetCount)
        return g_commandTargets[index].command;
    return 0;
}

void DrawShellButton(HDC hdc, const RECT& rect, const wchar_t* icon,
                     const wchar_t* label, bool selected, bool hovered, bool pressed)
{
    const COLORREF accent = RGB(0, 120, 212);
    const COLORREF accentPressed = RGB(0, 99, 177);
    const COLORREF text = RGB(32, 32, 32);
    const COLORREF subtle = RGB(242, 242, 242);
    const COLORREF hover = RGB(236, 236, 236);
    const COLORREF white = RGB(255, 255, 255);

    if (selected || pressed)
        FillRounded(hdc, rect, pressed ? accentPressed : accent,
                    pressed ? accentPressed : accent, 12);
    else if (hovered)
        FillRounded(hdc, rect, hover, hover, 10);
    else
        FillRounded(hdc, rect, subtle, subtle, 10);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, (selected || pressed) ? white : accent);

    RECT iconRect = rect;
    iconRect.left += 8;
    iconRect.right = iconRect.left + 19;
    HGDIOBJ oldIconFont = SelectObject(hdc, GetShellIconFont());
    DrawTextW(hdc, icon, -1, &iconRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldIconFont);

    RECT textRect = rect;
    textRect.left += 31;
    textRect.right -= 6;
    SetTextColor(hdc, (selected || pressed) ? white : text);
    HGDIOBJ oldTextFont = SelectObject(hdc,
        (selected || pressed) ? GetShellBoldFont() : GetShellTextFont());
    DrawTextW(hdc, label, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(hdc, oldTextFont);
}

void DrawCorrectedShell(HWND hwnd)
{
    if (!hwnd) return;
    BuildShellTargets(hwnd);

    RECT client = {};
    GetClientRect(hwnd, &client);
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    const COLORREF topBg = RGB(249, 249, 249);
    const COLORREF commandBg = RGB(252, 252, 252);
    const COLORREF divider = RGB(224, 224, 224);

    RECT top = { 0, 0, client.right, kNavHeight + 1 };
    HBRUSH topBrush = CreateSolidBrush(topBg);
    FillRect(hdc, &top, topBrush);
    DeleteObject(topBrush);

    RECT commandRow = { 0, kCommandTop, client.right, kShellHeight };
    HBRUSH commandBrush = CreateSolidBrush(commandBg);
    FillRect(hdc, &commandRow, commandBrush);
    DeleteObject(commandBrush);

    HPEN dividerPen = CreatePen(PS_SOLID, 1, divider);
    HGDIOBJ oldPen = SelectObject(hdc, dividerPen);
    MoveToEx(hdc, 0, kShellHeight - 1, NULL);
    LineTo(hdc, client.right, kShellHeight - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(dividerPen);

    static const wchar_t* navLabels[] = { L"Monitor", L"Filters", L"Replay", L"Logs", L"SMTP", L"Settings" };
    static const wchar_t* navIcons[] = { L"\xE7F4", L"\xE71C", L"\xE72C", L"\xE8A5", L"\xE715", L"\xE713" };
    for (int i = 0; i < g_navTargetCount; ++i)
    {
        const bool settingsSelected = i == kSettingsTarget && g_settingsMenuOpen;
        const bool monitorSelected = i == 0 && !g_settingsMenuOpen;
        DrawShellButton(hdc, g_navTargets[i].rect, navIcons[i], navLabels[i],
                        settingsSelected || monitorSelected,
                        g_hoverTarget == i,
                        g_pressedTarget == i || (i == kSettingsTarget && g_settingsPressed));
    }

    static const wchar_t* commandLabels[] = {
        L"Open", L"Save", L"Print", L"Copy", L"Monitor", L"Pause",
        L"Filters", L"Options", L"Stats", L"Clear", L"Mode"
    };
    static const wchar_t* commandIcons[] = {
        L"\xE8B7", L"\xE74E", L"\xE749", L"\xE8C8", L"\xE768", L"\xE769",
        L"\xE71C", L"\xE713", L"\xE9D9", L"\xE894", L"\xE7F4"
    };
    for (int i = 0; i < g_commandTargetCount; ++i)
    {
        const int target = kCommandTargetBase + i;
        DrawShellButton(hdc, g_commandTargets[i].rect,
                        commandIcons[i], commandLabels[i], false,
                        g_hoverTarget == target,
                        g_pressedTarget == target);
    }

    ReleaseDC(hwnd, hdc);
}

void RedrawShell(HWND hwnd)
{
    if (!hwnd) return;
    RECT rect = { 0, 0, 0, kShellHeight };
    GetClientRect(hwnd, &rect);
    rect.bottom = kShellHeight;
    InvalidateRect(hwnd, &rect, TRUE);
    UpdateWindow(hwnd);
    DrawCorrectedShell(hwnd);
}

UINT ShowSettingsPopup(HWND hwnd)
{
    BuildShellTargets(hwnd);
    if (g_navTargetCount <= kSettingsTarget) return 0;

    HMENU popup = CreatePopupMenu();
    HMENU appearance = CreatePopupMenu();
    if (!popup || !appearance)
    {
        if (appearance) DestroyMenu(appearance);
        if (popup) DestroyMenu(popup);
        return 0;
    }

    AppendMenuA(appearance, MF_STRING, IDM_COLOR, "Colors");
    AppendMenuA(appearance, MF_STRING, IDM_FONT, "Font");
    AppendMenuA(appearance, MF_STRING, IDM_SCREENOPTIONS, "Display layout");
    AppendMenuA(popup, MF_STRING, IDM_GENERAL, "General");
    AppendMenuA(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(appearance), "Appearance");
    AppendMenuA(popup, MF_STRING, IDM_LOGFILE, "Data & logging");
    AppendMenuA(popup, MF_STRING, IDM_MAIL, "SMTP / Network");
    AppendMenuA(popup, MF_SEPARATOR, 0, NULL);
    AppendMenuA(popup, MF_STRING, IDM_ABOUT, "About PDW");

    RECT anchor = g_navTargets[kSettingsTarget].rect;
    POINT pt = { anchor.left, anchor.bottom + 3 };
    ClientToScreen(hwnd, &pt);

    g_settingsMenuOpen = true;
    g_settingsPressed = false;
    DrawCorrectedShell(hwnd);

    const UINT command = TrackPopupMenu(popup,
                                        TPM_RETURNCMD | TPM_NONOTIFY |
                                        TPM_LEFTALIGN | TPM_TOPALIGN,
                                        pt.x, pt.y, 0, hwnd, NULL);

    g_settingsMenuOpen = false;
    g_settingsPressed = false;
    g_closeMenuFromSettings = false;
    DrawCorrectedShell(hwnd);

    if (command)
        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);

    DestroyMenu(popup);
    return command;
}

void DispatchShellCommand(HWND hwnd, int command)
{
    if (command == 0) return;
    if (command == kSettingsPopupCommand)
    {
        ShowSettingsPopup(hwnd);
        return;
    }
    if (command == kResumeMonitorCommand)
    {
        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDT_TOOLBAR_BTN9, 0), 0);
        return;
    }
    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

void LayoutFilterDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int cx = client.right;
    const int cy = client.bottom;
    if (cx <= 0 || cy <= 0) return;

    const int margin = ScaleForDpi(hwnd, 10);
    const int gap = ScaleForDpi(hwnd, 8);
    const int buttonHeight = ScaleForDpi(hwnd, 28);
    int buttonWidth = cx / 10;
    const int minButtonWidth = ScaleForDpi(hwnd, 68);
    const int maxButtonWidth = ScaleForDpi(hwnd, 112);
    if (buttonWidth < minButtonWidth) buttonWidth = minButtonWidth;
    if (buttonWidth > maxButtonWidth) buttonWidth = maxButtonWidth;

    const int buttonY = cy - margin - buttonHeight;
    int listHeight = buttonY - gap - margin;
    if (listHeight < ScaleForDpi(hwnd, 140)) listHeight = ScaleForDpi(hwnd, 140);

    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    HWND add = GetDlgItem(hwnd, IDC_FILTERADD);
    HWND edit = GetDlgItem(hwnd, IDC_FILTEREDIT);
    HWND remove = GetDlgItem(hwnd, IDC_FILTERDEL);
    HWND options = GetDlgItem(hwnd, IDC_FILTEROPTIONS);
    HWND find = GetDlgItem(hwnd, IDC_FILTERFIND);
    HWND ok = GetDlgItem(hwnd, IDOK);

    HDWP defer = BeginDeferWindowPos(7);
    if (list)
        defer = DeferWindowPos(defer, list, NULL, margin, margin,
                               cx - 2 * margin, listHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);

    int x = margin;
    HWND leftButtons[] = { add, edit, remove };
    for (int i = 0; i < 3; ++i)
    {
        if (leftButtons[i])
            defer = DeferWindowPos(defer, leftButtons[i], NULL, x, buttonY,
                                   buttonWidth, buttonHeight,
                                   SWP_NOZORDER | SWP_NOACTIVATE);
        x += buttonWidth + gap;
    }

    const int middleStart = cx / 2 - buttonWidth - gap / 2;
    if (options)
        defer = DeferWindowPos(defer, options, NULL, middleStart, buttonY,
                               buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    if (find)
        defer = DeferWindowPos(defer, find, NULL,
                               middleStart + buttonWidth + gap, buttonY,
                               buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    if (ok)
        defer = DeferWindowPos(defer, ok, NULL,
                               cx - margin - buttonWidth, buttonY,
                               buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);

    if (defer) EndDeferWindowPos(defer);
    if (list)
        ListView_SetColumnWidth(list, 0,
            max(100, cx - 2 * margin - ScaleForDpi(hwnd, 4)));
}

void PlaceFilterDialog(HWND hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return;

    const int workW = info.rcWork.right - info.rcWork.left;
    const int workH = info.rcWork.bottom - info.rcWork.top;
    int width = workW * 72 / 100;
    int height = workH * 70 / 100;

    const int maxW = ScaleForDpi(hwnd, 1280);
    const int maxH = ScaleForDpi(hwnd, 800);
    const int minW = ScaleForDpi(hwnd, 760);
    const int minH = ScaleForDpi(hwnd, 480);
    if (width > maxW) width = maxW;
    if (height > maxH) height = maxH;
    if (width < minW) width = minW;
    if (height < minH) height = minH;
    if (width > workW) width = workW;
    if (height > workH) height = workH;

    const int left = info.rcWork.left + (workW - width) / 2;
    const int top = info.rcWork.top + (workH - height) / 2;
    SetWindowPos(hwnd, NULL, left, top, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

LRESULT CALLBACK FilterResizeSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                          LPARAM lParam, UINT_PTR subclassId,
                                          DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = ScaleForDpi(hwnd, 520);
            info->ptMinTrackSize.y = ScaleForDpi(hwnd, 340);
            break;
        }

        case WM_NCHITTEST:
        {
            LRESULT hit = DefSubclassProc(hwnd, message, wParam, lParam);
            if (IsZoomed(hwnd)) return hit;
            if (hit != HTCLIENT && hit != HTBORDER) return hit;

            RECT wr = {};
            GetWindowRect(hwnd, &wr);
            const int border = ScaleForDpi(hwnd, 8);
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const bool left = x < wr.left + border;
            const bool right = x >= wr.right - border;
            const bool top = y < wr.top + border;
            const bool bottom = y >= wr.bottom - border;
            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;
            return hit;
        }

        case WM_SIZE:
        {
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            if (wParam != SIZE_MINIMIZED) LayoutFilterDialog(hwnd);
            return result;
        }

        case WM_DPICHANGED:
        {
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            LayoutFilterDialog(hwnd);
            return result;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, kFilterResizeProp);
            RemoveWindowSubclass(hwnd, FilterResizeSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableFilterResize(HWND hwnd)
{
    if (!hwnd || GetPropW(hwnd, kFilterResizeProp)) return;

    SetPropW(hwnd, kFilterResizeProp, reinterpret_cast<HANDLE>(1));

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowSubclass(hwnd, FilterResizeSubclassProc, kFilterResizeSubclassId, 0);
    PlaceFilterDialog(hwnd);
    LayoutFilterDialog(hwnd);
}

LRESULT CALLBACK MainInteractionSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR subclassId,
                                             DWORD_PTR referenceData)
{
    if (message == kShellCommandMessage)
    {
        DispatchShellCommand(hwnd, static_cast<int>(wParam));
        return 0;
    }

    if (message == WM_SETCURSOR)
    {
        POINT screen = {};
        GetCursorPos(&screen);
        POINT client = screen;
        ScreenToClient(hwnd, &client);
        if (client.y >= 0 && client.y < kShellHeight &&
            HitTestShell(hwnd, client) >= 0)
        {
            SetCursor(LoadCursor(NULL, IDC_HAND));
            return TRUE;
        }
    }

    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case kEnableModernShellMessage:
        case WM_PAINT:
        case WM_SIZE:
        case WM_NOTIFY:
        case WM_TIMER:
            DrawCorrectedShell(hwnd);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, kMainInteractionProp);
            RemoveWindowSubclass(hwnd, MainInteractionSubclassProc, subclassId);
            if (g_mainWindow == hwnd) g_mainWindow = NULL;
            break;
    }

    return result;
}

void InstallMainInteractionFix(HWND hwnd)
{
    if (!hwnd || GetPropW(hwnd, kMainInteractionProp)) return;
    g_mainWindow = hwnd;
    SetPropW(hwnd, kMainInteractionProp, reinterpret_cast<HANDLE>(1));
    SetWindowSubclass(hwnd, MainInteractionSubclassProc, kInteractionSubclassId, 0);
    DrawCorrectedShell(hwnd);
}

LRESULT CALLBACK ThreadMouseProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code < 0 || !g_mainWindow || !IsWindow(g_mainWindow))
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);

    const MOUSEHOOKSTRUCT* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
    POINT clientPoint = mouse->pt;
    ScreenToClient(g_mainWindow, &clientPoint);
    const int target = (clientPoint.y >= 0 && clientPoint.y < kShellHeight)
        ? HitTestShell(g_mainWindow, clientPoint) : -1;

    if (wParam == WM_MOUSEMOVE)
    {
        if (target != g_hoverTarget)
        {
            g_hoverTarget = target;
            DrawCorrectedShell(g_mainWindow);
        }
        return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    }

    if (wParam == WM_LBUTTONDOWN && target >= 0)
    {
        if (target == kSettingsTarget && g_settingsMenuOpen)
        {
            g_closeMenuFromSettings = true;
            g_settingsPressed = false;
            g_pressedTarget = -1;
            EndMenu();
            return 1;
        }

        g_pressedTarget = target;
        g_settingsPressed = target == kSettingsTarget;
        DrawCorrectedShell(g_mainWindow);
        return 1;
    }

    if (wParam == WM_LBUTTONUP)
    {
        if (g_closeMenuFromSettings)
        {
            g_closeMenuFromSettings = false;
            g_pressedTarget = -1;
            g_settingsPressed = false;
            DrawCorrectedShell(g_mainWindow);
            return 1;
        }

        if (g_pressedTarget >= 0)
        {
            const int pressed = g_pressedTarget;
            g_pressedTarget = -1;
            g_settingsPressed = false;
            DrawCorrectedShell(g_mainWindow);
            if (target == pressed)
            {
                const int command = CommandForTarget(pressed);
                PostMessage(g_mainWindow, kShellCommandMessage,
                            static_cast<WPARAM>(command), 0);
            }
            return 1;
        }
    }

    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

LRESULT CALLBACK ThreadCallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && lParam)
    {
        const CWPRETSTRUCT* info = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
        if (info->message == kEnableModernShellMessage && IsMainWindow(info->hwnd))
            InstallMainInteractionFix(info->hwnd);

        // Do not rely solely on WM_INITDIALOG. The legacy filter dialog moves
        // itself during initialization, so apply the resize contract as soon
        // as its list control exists and keep it idempotent via a window prop.
        if (IsFilterDialog(info->hwnd))
            EnableFilterResize(info->hwnd);
    }
    return CallNextHookEx(g_callHook, code, wParam, lParam);
}

struct InteractionBootstrap
{
    InteractionBootstrap()
    {
        const DWORD threadId = GetCurrentThreadId();
        g_callHook = SetWindowsHookExW(WH_CALLWNDPROCRET, ThreadCallWndRetProc,
                                      NULL, threadId);
        g_mouseHook = SetWindowsHookExW(WH_MOUSE, ThreadMouseProc,
                                       NULL, threadId);
    }
};

InteractionBootstrap g_bootstrap;

} // namespace
