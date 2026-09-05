#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include "..\\Headers\\Resource.h"

namespace {

const UINT_PTR kInteractionSubclassId = 0x50445721;
const UINT_PTR kFilterResizeSubclassId = 0x50445722;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
const wchar_t* kMainClassName = L"WinPDWWndClass";
const wchar_t* kMainInteractionProp = L"PDW.ModernInteractionInstalled";
const wchar_t* kFilterResizeProp = L"PDW.FilterResizeInstalled";

HHOOK g_hook = NULL;
bool g_settingsMenuOpen = false;
bool g_settingsPressed = false;
ULONGLONG g_swallowSettingsClickUntil = 0;

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
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;
    return GetDlgItem(hwnd, IDC_FILTERS) != NULL;
}

bool GetSettingsRect(HWND hwnd, RECT* rect)
{
    if (!rect) return false;
    RECT client = {};
    GetClientRect(hwnd, &client);

    static const int widths[] = { 96, 88, 88, 76, 76, 102 };
    int x = 6;
    for (int i = 0; i < 5; ++i) x += widths[i] + 3;

    const int right = x + widths[5];
    if (right >= client.right - 4) return false;
    SetRect(rect, x, 2, right, 23);
    return true;
}

bool PointInSettings(HWND hwnd, POINT point)
{
    RECT rect = {};
    return GetSettingsRect(hwnd, &rect) && PtInRect(&rect, point) != FALSE;
}

void DrawSettingsState(HWND hwnd)
{
    if (!g_settingsMenuOpen && !g_settingsPressed) return;

    RECT rect = {};
    if (!GetSettingsRect(hwnd, &rect)) return;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    const COLORREF accent = g_settingsPressed ? RGB(0, 99, 177) : RGB(0, 120, 212);
    HBRUSH brush = CreateSolidBrush(accent);
    HPEN pen = CreatePen(PS_SOLID, 1, accent);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 14, 14);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HFONT iconFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    if (iconFont)
    {
        HGDIOBJ oldFont = SelectObject(hdc, iconFont);
        RECT iconRect = { rect.left + 10, rect.top, rect.left + 30, rect.bottom };
        DrawTextW(hdc, L"\xE713", -1, &iconRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        DeleteObject(iconFont);
    }

    HFONT textFont = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (textFont)
    {
        HGDIOBJ oldFont = SelectObject(hdc, textFont);
        RECT textRect = { rect.left + 34, rect.top, rect.right - 8, rect.bottom };
        DrawTextW(hdc, L"Settings", -1, &textRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        DeleteObject(textFont);
    }

    ReleaseDC(hwnd, hdc);
}

void RepaintSettings(HWND hwnd)
{
    RECT rect = {};
    if (GetSettingsRect(hwnd, &rect))
    {
        InvalidateRect(hwnd, &rect, TRUE);
        UpdateWindow(hwnd);
    }
}

UINT ShowSettingsPopup(HWND hwnd)
{
    RECT anchor = {};
    if (!GetSettingsRect(hwnd, &anchor)) return 0;

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

    POINT pt = { anchor.left, anchor.bottom + 3 };
    ClientToScreen(hwnd, &pt);

    g_settingsMenuOpen = true;
    g_settingsPressed = false;
    RepaintSettings(hwnd);

    const UINT command = TrackPopupMenu(popup,
                                        TPM_RETURNCMD | TPM_NONOTIFY |
                                        TPM_LEFTALIGN | TPM_TOPALIGN,
                                        pt.x, pt.y, 0, hwnd, NULL);

    POINT cursor = {};
    GetCursorPos(&cursor);
    ScreenToClient(hwnd, &cursor);
    if (!command && PointInSettings(hwnd, cursor))
        g_swallowSettingsClickUntil = GetTickCount64() + 500;

    g_settingsMenuOpen = false;
    g_settingsPressed = false;
    RepaintSettings(hwnd);

    if (command)
        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);

    DestroyMenu(popup);
    return command;
}

void LayoutFilterDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int cx = client.right - client.left;
    const int cy = client.bottom - client.top;
    if (cx <= 0 || cy <= 0) return;

    const int margin = ScaleForDpi(hwnd, 10);
    const int gap = ScaleForDpi(hwnd, 8);
    const int buttonHeight = ScaleForDpi(hwnd, 28);
    int buttonWidth = cx / 10;
    const int minButtonWidth = ScaleForDpi(hwnd, 72);
    const int maxButtonWidth = ScaleForDpi(hwnd, 110);
    if (buttonWidth < minButtonWidth) buttonWidth = minButtonWidth;
    if (buttonWidth > maxButtonWidth) buttonWidth = maxButtonWidth;

    const int buttonY = cy - margin - buttonHeight;
    const int listHeight = buttonY - gap - margin;

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
    if (add)
        defer = DeferWindowPos(defer, add, NULL, x, buttonY, buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    x += buttonWidth + gap;
    if (edit)
        defer = DeferWindowPos(defer, edit, NULL, x, buttonY, buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    x += buttonWidth + gap;
    if (remove)
        defer = DeferWindowPos(defer, remove, NULL, x, buttonY, buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);

    const int middleStart = cx / 2 - buttonWidth - gap / 2;
    if (options)
        defer = DeferWindowPos(defer, options, NULL, middleStart, buttonY,
                               buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    if (find)
        defer = DeferWindowPos(defer, find, NULL, middleStart + buttonWidth + gap,
                               buttonY, buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);

    if (ok)
        defer = DeferWindowPos(defer, ok, NULL, cx - margin - buttonWidth, buttonY,
                               buttonWidth, buttonHeight,
                               SWP_NOZORDER | SWP_NOACTIVATE);

    if (defer) EndDeferWindowPos(defer);

    if (list)
        ListView_SetColumnWidth(list, 0, max(100, cx - 2 * margin - ScaleForDpi(hwnd, 4)));
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
            info->ptMinTrackSize.x = ScaleForDpi(hwnd, 700);
            info->ptMinTrackSize.y = ScaleForDpi(hwnd, 420);
            break;
        }

        case WM_SIZE:
        {
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            if (wParam != SIZE_MINIMIZED) LayoutFilterDialog(hwnd);
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

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(DS_MODALFRAME);
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    SetPropW(hwnd, kFilterResizeProp, reinterpret_cast<HANDLE>(1));
    SetWindowSubclass(hwnd, FilterResizeSubclassProc, kFilterResizeSubclassId, 0);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
    LayoutFilterDialog(hwnd);
}

LRESULT CALLBACK MainInteractionSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                             LPARAM lParam, UINT_PTR subclassId,
                                             DWORD_PTR referenceData)
{
    if (message == WM_LBUTTONDOWN)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PointInSettings(hwnd, point))
        {
            g_settingsPressed = true;
            RepaintSettings(hwnd);
            return 0;
        }
    }

    if (message == WM_LBUTTONUP)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PointInSettings(hwnd, point))
        {
            if (g_swallowSettingsClickUntil &&
                GetTickCount64() <= g_swallowSettingsClickUntil)
            {
                g_swallowSettingsClickUntil = 0;
                g_settingsPressed = false;
                RepaintSettings(hwnd);
                return 0;
            }

            if (g_settingsMenuOpen)
            {
                EndMenu();
                return 0;
            }

            g_settingsPressed = false;
            ShowSettingsPopup(hwnd);
            return 0;
        }

        if (g_settingsPressed)
        {
            g_settingsPressed = false;
            RepaintSettings(hwnd);
        }
    }

    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case WM_PAINT:
        case WM_SIZE:
        case WM_TIMER:
            DrawSettingsState(hwnd);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, kMainInteractionProp);
            RemoveWindowSubclass(hwnd, MainInteractionSubclassProc, subclassId);
            break;
    }

    return result;
}

void InstallMainInteractionFix(HWND hwnd)
{
    if (!hwnd || GetPropW(hwnd, kMainInteractionProp)) return;
    SetPropW(hwnd, kMainInteractionProp, reinterpret_cast<HANDLE>(1));
    SetWindowSubclass(hwnd, MainInteractionSubclassProc, kInteractionSubclassId, 0);
}

LRESULT CALLBACK ThreadCallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && lParam)
    {
        const CWPRETSTRUCT* info = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
        if (info->message == kEnableModernShellMessage && IsMainWindow(info->hwnd))
            InstallMainInteractionFix(info->hwnd);
        else if (info->message == WM_INITDIALOG && IsFilterDialog(info->hwnd))
            EnableFilterResize(info->hwnd);
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

struct InteractionBootstrap
{
    InteractionBootstrap()
    {
        g_hook = SetWindowsHookExW(WH_CALLWNDPROCRET, ThreadCallWndRetProc,
                                  NULL, GetCurrentThreadId());
    }
};

InteractionBootstrap g_bootstrap;

} // namespace
