#include "windows11_ui.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>

#include "..\\Headers\\pdw.h"
#include "..\\Headers\\gfx.h"
#include "..\\Headers\\initapp.h"
#include "..\\Headers\\Resource.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

extern double dRX_Quality;
extern bool bPauseFlag;

namespace {

const DWORD kDwmUseImmersiveDarkMode = 20;
const DWORD kDwmWindowCornerPreference = 33;
const DWORD kDwmSystemBackdropType = 38;
const int kDwmCornerRound = 2;
const int kDwmBackdropMainWindow = 2;
const UINT_PTR kMainWindowSubclassId = 0x50445711;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
const WPARAM kLegacySecondTimer = 103;
const int kSettingsPopupCommand = 50001;
const int kResumeMonitorCommand = 50002;
const int kShellHeight = 49;
const int kNavHeight = 24;
const int kCommandTop = 25;
const int kCommandHeight = 23;

HFONT g_dialogFont = NULL;
HFONT g_headerFont = NULL;
HFONT g_iconFont = NULL;
HHOOK g_dialogHook = NULL;
bool g_chromeEnabled = false;
bool g_legacyMenuDetached = false;

struct ShellHitTarget
{
    RECT rect;
    int command;
};

ShellHitTarget g_navTargets[6];
ShellHitTarget g_commandTargets[11];
int g_navTargetCount = 0;
int g_commandTargetCount = 0;

bool AppsPreferDarkMode()
{
    HKEY key = NULL;
    DWORD value = 1;
    DWORD size = sizeof(value);

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    {
        return false;
    }

    const LONG result = RegQueryValueExW(key, L"AppsUseLightTheme", NULL, NULL,
                                         reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && value == 0;
}

HFONT GetDialogFont()
{
    if (g_dialogFont) return g_dialogFont;

    NONCLIENTMETRICSW metrics;
    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        g_dialogFont = CreateFontIndirectW(&metrics.lfMessageFont);
    }

    if (!g_dialogFont)
    {
        g_dialogFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }
    return g_dialogFont;
}

HFONT GetHeaderFont()
{
    if (g_headerFont) return g_headerFont;
    g_headerFont = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!g_headerFont) g_headerFont = GetDialogFont();
    return g_headerFont;
}

HFONT GetIconFont()
{
    if (g_iconFont) return g_iconFont;
    g_iconFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    return g_iconFont;
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
{
    HFONT font = reinterpret_cast<HFONT>(fontParam);
    if (font) SendMessage(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SetWindowTheme(child, L"Explorer", NULL);
    return TRUE;
}

void ApplyRoundedCorners(HWND hwnd)
{
    if (!hwnd) return;
    int preference = kDwmCornerRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference,
                          &preference, sizeof(preference));
}

void HideLegacyToolbar(HWND mainWindow)
{
    HWND toolbar = FindWindowExW(mainWindow, NULL, TOOLBARCLASSNAMEW, NULL);
    if (toolbar) ShowWindow(toolbar, SW_HIDE);
}

void DetachLegacyMenu(HWND mainWindow)
{
    if (g_legacyMenuDetached) return;
    if (!ghMenu) ghMenu = GetMenu(mainWindow);
    if (!ghMenu) return;
    if (GetMenu(mainWindow) == ghMenu)
    {
        SetMenu(mainWindow, NULL);
        DrawMenuBar(mainWindow);
    }
    g_legacyMenuDetached = true;
}

void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF fill, COLORREF border, int radius)
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

void DrawIconTextButton(HDC hdc, const RECT& rect, const wchar_t* icon,
                        const wchar_t* label, bool selected, bool dark)
{
    const COLORREF accent = RGB(0, 120, 212);
    const COLORREF bg = dark ? RGB(38, 38, 38) : RGB(250, 250, 250);
    const COLORREF fg = dark ? RGB(245, 245, 245) : RGB(30, 30, 30);
    const COLORREF selectedFg = RGB(255, 255, 255);
    const COLORREF border = dark ? RGB(62, 62, 62) : RGB(228, 228, 228);

    if (selected)
        FillRoundedRect(hdc, rect, accent, accent, 14);
    else
        FillRoundedRect(hdc, rect, bg, bg, 14);

    int iconWidth = 0;
    if (icon && icon[0])
    {
        RECT iconRect = rect;
        iconRect.left += 10;
        iconRect.right = iconRect.left + 20;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, selected ? selectedFg : accent);
        HFONT iconFont = GetIconFont();
        HGDIOBJ oldFont = iconFont ? SelectObject(hdc, iconFont) : NULL;
        DrawTextW(hdc, icon, -1, &iconRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) SelectObject(hdc, oldFont);
        iconWidth = 24;
    }

    RECT textRect = rect;
    textRect.left += 10 + iconWidth;
    textRect.right -= 8;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, selected ? selectedFg : fg);
    HFONT font = selected ? GetHeaderFont() : GetDialogFont();
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    DrawTextW(hdc, label, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (oldFont) SelectObject(hdc, oldFont);

    if (!selected)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, border);
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, rect.right, rect.top + 5, NULL);
        LineTo(hdc, rect.right, rect.bottom - 5);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }
}

void DrawTopNavigation(HDC hdc, const RECT& client, bool dark)
{
    static const wchar_t* labels[] = { L"Monitor", L"Filters", L"Replay", L"Logs", L"SMTP", L"Settings" };
    static const wchar_t* icons[]  = { L"\xE7F4", L"\xE71C", L"\xE72C", L"\xE8A5", L"\xE715", L"\xE713" };
    static const int commands[] = { 0, IDM_FILTERS, IDM_PLAYBACK, IDM_LOGFILE, IDM_MAIL, kSettingsPopupCommand };
    static const int widths[] = { 96, 88, 88, 76, 76, 102 };

    const COLORREF shellBg = dark ? RGB(32, 32, 32) : RGB(249, 249, 249);
    RECT row = { 0, 0, client.right, kNavHeight + 1 };
    HBRUSH brush = CreateSolidBrush(shellBg);
    FillRect(hdc, &row, brush);
    DeleteObject(brush);

    g_navTargetCount = 0;
    int x = 6;
    for (int i = 0; i < 6; ++i)
    {
        if (x + widths[i] >= client.right - 4) break;
        RECT r = { x, 2, x + widths[i], kNavHeight - 1 };
        DrawIconTextButton(hdc, r, icons[i], labels[i], i == 0, dark);
        g_navTargets[g_navTargetCount].rect = r;
        g_navTargets[g_navTargetCount].command = commands[i];
        ++g_navTargetCount;
        x += widths[i] + 3;
    }
}

void DrawCommandStrip(HDC hdc, const RECT& client, bool dark)
{
    static const wchar_t* labels[] = {
        L"Open", L"Save", L"Print", L"Copy", L"Monitor", L"Pause",
        L"Filters", L"Options", L"Stats", L"Clear", L"Mode"
    };
    static const wchar_t* icons[] = {
        L"\xE8B7", L"\xE74E", L"\xE749", L"\xE8C8", L"\xE768", L"\xE769",
        L"\xE71C", L"\xE713", L"\xE9D9", L"\xE894", L"\xE7F4"
    };
    static const int commands[] = {
        IDM_PLAYBACK, IDM_COPY_SAVE, IDM_COPY_PRINT, IDM_COPY_SELECTION,
        kResumeMonitorCommand, IDT_TOOLBAR_BTN9, IDM_FILTERS, IDM_OPTIONS,
        IDM_MONSTAT, IDM_CLEARDISPLAY, IDT_TOOLBAR_BTN12
    };
    static const int widths[] = { 68, 66, 68, 66, 82, 72, 76, 80, 68, 68, 70 };

    const COLORREF rowBg = dark ? RGB(38, 38, 38) : RGB(252, 252, 252);
    const COLORREF rowBorder = dark ? RGB(64, 64, 64) : RGB(226, 226, 226);
    RECT bar = { 5, kCommandTop, client.right - 5, kCommandTop + kCommandHeight };
    FillRoundedRect(hdc, bar, rowBg, rowBorder, 10);

    g_commandTargetCount = 0;
    int x = 8;
    for (int i = 0; i < 11; ++i)
    {
        if (x + widths[i] >= client.right - 8) break;
        RECT r = { x, kCommandTop + 1, x + widths[i], kCommandTop + kCommandHeight - 1 };
        DrawIconTextButton(hdc, r, icons[i], labels[i], false, dark);
        g_commandTargets[g_commandTargetCount].rect = r;
        g_commandTargets[g_commandTargetCount].command = commands[i];
        ++g_commandTargetCount;
        x += widths[i];
    }
}

void DrawModernShell(HWND hwnd)
{
    RECT client;
    GetClientRect(hwnd, &client);
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    const bool dark = AppsPreferDarkMode();
    DrawTopNavigation(hdc, client, dark);
    DrawCommandStrip(hdc, client, dark);
    ReleaseDC(hwnd, hdc);
}

void ShowSettingsMenu(HWND mainWindow, const RECT& anchor)
{
    HMENU popup = CreatePopupMenu();
    HMENU appearance = CreatePopupMenu();
    if (!popup || !appearance)
    {
        if (appearance) DestroyMenu(appearance);
        if (popup) DestroyMenu(popup);
        return;
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
    ClientToScreen(mainWindow, &pt);
    const UINT command = TrackPopupMenu(popup,
                                        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        pt.x, pt.y, 0, mainWindow, NULL);
    if (command) SendMessage(mainWindow, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    DestroyMenu(popup);
}

bool HandleShellClick(HWND hwnd, POINT point)
{
    for (int i = 0; i < g_navTargetCount; ++i)
    {
        if (!PtInRect(&g_navTargets[i].rect, point)) continue;
        const int command = g_navTargets[i].command;
        if (command == 0) return true;
        if (command == kSettingsPopupCommand)
        {
            ShowSettingsMenu(hwnd, g_navTargets[i].rect);
            return true;
        }
        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return true;
    }

    for (int i = 0; i < g_commandTargetCount; ++i)
    {
        if (!PtInRect(&g_commandTargets[i].rect, point)) continue;
        const int command = g_commandTargets[i].command;
        if (command == kResumeMonitorCommand)
        {
            if (bPauseFlag) SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDT_TOOLBAR_BTN9, 0), 0);
            return true;
        }
        SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
        return true;
    }
    return false;
}

const char* HeaderLabelForItem(int item)
{
    switch (item)
    {
        case 1:
            if (Profile.monitor_acars) return "Aircraft";
            if (Profile.monitor_mobitex) return "MAN";
            return "Address";
        case 2: return "Time";
        case 3: return "Date";
        case 4:
            if (Profile.monitor_acars) return "Msg no.";
            if (Profile.monitor_mobitex) return "Sender";
            return "Mode";
        case 5: return Profile.monitor_acars ? "DBI" : "Type";
        case 6: return Profile.monitor_acars ? "Mode" : "Bitrate";
        case 7: return "Messages";
        default: return "";
    }
}

void DrawHeaderCell(HDC hdc, int x, int y, int width, int height,
                    const char* text, bool dark, bool drawRightEdge)
{
    if (width <= 0 || height <= 0) return;

    const COLORREF background = dark ? RGB(36, 36, 36) : RGB(248, 249, 250);
    const COLORREF foreground = dark ? RGB(242, 242, 242) : RGB(32, 32, 32);
    const COLORREF divider = dark ? RGB(62, 62, 62) : RGB(224, 224, 224);

    RECT cell = { x, y, x + width, y + height };
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(hdc, &cell, brush);
    DeleteObject(brush);

    HPEN pen = CreatePen(PS_SOLID, 1, divider);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, cell.left, cell.bottom - 1, NULL);
    LineTo(hdc, cell.right, cell.bottom - 1);
    if (drawRightEdge)
    {
        MoveToEx(hdc, cell.right - 1, cell.top + 4, NULL);
        LineTo(hdc, cell.right - 1, cell.bottom - 4);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, foreground);
    HFONT font = GetHeaderFont();
    HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
    RECT textRect = { cell.left + 9, cell.top, cell.right - 7, cell.bottom };
    DrawTextA(hdc, text, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (oldFont) SelectObject(hdc, oldFont);
}

void DrawPaneHeader(HWND mainWindow, HWND pane, bool filteredPane, bool withRx)
{
    if (!pane || cxChar == 0 || cyChar == 0) return;

    RECT paneRect;
    if (!GetWindowRect(pane, &paneRect)) return;
    POINT points[2] = { { paneRect.left, paneRect.top }, { paneRect.right, paneRect.bottom } };
    MapWindowPoints(NULL, mainWindow, points, 2);

    RECT client;
    GetClientRect(mainWindow, &client);
    const int headerHeight = TITLE_BAR_SIZE;
    const int y = points[0].y - headerHeight - 1;
    if (y < kShellHeight - 2) return;

    HDC hdc = GetDC(mainWindow);
    if (!hdc) return;

    SetMessageItemPositionsWidth();
    const bool dark = AppsPreferDarkMode();
    const int rxWidth = withRx ? 82 : 0;
    int left = filteredPane ? PL2_SCount : PL1_SCount;

    for (int i = 0; i < 7; ++i)
    {
        const int item = Profile.ScreenColumns[i];
        if (item == 0) break;
        int width = 0;
        if (item == 7) width = client.right - left - rxWidth;
        else
        {
            width = iItemWidths[item];
            if (i == 0) width += cxChar;
        }

        const char* label = HeaderLabelForItem(item);
        if (item == 7) label = filteredPane ? "Filtered messages" : "Monitored messages";
        DrawHeaderCell(hdc, left, y, width, headerHeight, label, dark, item != 7 || withRx);
        left += width;
    }

    if (withRx)
    {
        const COLORREF fg = dark ? RGB(235, 235, 235) : RGB(30, 30, 30);
        const COLORREF dot = dRX_Quality > 0.0 ? RGB(16, 124, 16) : RGB(145, 145, 145);
        const int rxLeft = client.right - rxWidth;
        DrawHeaderCell(hdc, rxLeft, y, rxWidth, headerHeight, "", dark, false);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        HFONT font = GetHeaderFont();
        HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
        RECT rxText = { rxLeft + 7, y, client.right - 20, y + headerHeight };
        DrawTextA(hdc, "RX-Q", -1, &rxText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) SelectObject(hdc, oldFont);
        HBRUSH dotBrush = CreateSolidBrush(dot);
        HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
        HPEN dotPen = CreatePen(PS_SOLID, 1, dot);
        HGDIOBJ oldDotPen = SelectObject(hdc, dotPen);
        Ellipse(hdc, client.right - 16, y + 6, client.right - 8, y + 14);
        SelectObject(hdc, oldDotPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(dotPen);
        DeleteObject(dotBrush);
    }

    ReleaseDC(mainWindow, hdc);
}

void DrawModernMainChrome(HWND hwnd)
{
    DrawModernShell(hwnd);
    HWND pane1 = FindWindowExA(hwnd, NULL, "WinPDWPane1Class", NULL);
    HWND pane2 = FindWindowExA(hwnd, NULL, "WinPDWPane2Class", NULL);
    DrawPaneHeader(hwnd, pane1, false, true);
    DrawPaneHeader(hwnd, pane2, true, false);
}

LRESULT CALLBACK MainWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR subclassId,
                                        DWORD_PTR referenceData)
{
    if (message == WM_LBUTTONUP)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (point.y < kShellHeight && HandleShellClick(hwnd, point)) return 0;
    }

    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case kEnableModernShellMessage:
            DetachLegacyMenu(hwnd);
            HideLegacyToolbar(hwnd);
            DrawModernMainChrome(hwnd);
            break;

        case WM_PAINT:
        case WM_SIZE:
        case WM_NOTIFY:
            DrawModernMainChrome(hwnd);
            break;

        case WM_TIMER:
            if (wParam == kLegacySecondTimer) DrawModernMainChrome(hwnd);
            break;

        case WM_SETCURSOR:
        {
            POINT screen;
            GetCursorPos(&screen);
            POINT client = screen;
            ScreenToClient(hwnd, &client);
            if (client.y >= 0 && client.y < kShellHeight)
            {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, MainWindowSubclassProc, subclassId);
            break;
    }

    return result;
}

LRESULT CALLBACK DialogCallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && lParam)
    {
        const CWPRETSTRUCT* info = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
        if (info->message == WM_INITDIALOG)
            pdw::ApplyWindows11DialogStyle(info->hwnd);
    }
    return CallNextHookEx(g_dialogHook, code, wParam, lParam);
}

} // namespace

namespace pdw {

void ApplyWindows11MainWindowStyle(HWND hwnd)
{
    if (!hwnd) return;

    g_chromeEnabled = true;
    if (!ghMenu) ghMenu = GetMenu(hwnd);
    ApplyRoundedCorners(hwnd);

    const BOOL dark = AppsPreferDarkMode() ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    int backdrop = kDwmBackdropMainWindow;
    DwmSetWindowAttribute(hwnd, kDwmSystemBackdropType, &backdrop, sizeof(backdrop));

    SetWindowSubclass(hwnd, MainWindowSubclassProc, kMainWindowSubclassId, 0);
    PostMessage(hwnd, kEnableModernShellMessage, 0, 0);
}

void InstallWindows11DialogStyling()
{
    if (g_dialogHook) return;
    g_dialogHook = SetWindowsHookExW(WH_CALLWNDPROCRET, DialogCallWndRetProc,
                                    NULL, GetCurrentThreadId());
}

void ApplyWindows11DialogStyle(HWND hwnd)
{
    if (!hwnd) return;
    ApplyRoundedCorners(hwnd);
    SetWindowTheme(hwnd, L"Explorer", NULL);
    HFONT font = GetDialogFont();
    if (font) SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    EnumChildWindows(hwnd, StyleDialogChild, reinterpret_cast<LPARAM>(font));
    InvalidateRect(hwnd, NULL, TRUE);
}

void ApplyWindows11ControlStyle(HWND hwnd)
{
    if (!hwnd) return;
    SetWindowTheme(hwnd, L"Explorer", NULL);
}

bool IsWindows11ChromeEnabled()
{
    return g_chromeEnabled;
}

} // namespace pdw
