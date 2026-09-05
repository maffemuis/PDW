#include "windows11_ui.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <string.h>

#include "..\\Headers\\pdw.h"
#include "..\\Headers\\gfx.h"
#include "..\\Headers\\initapp.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

extern double dRX_Quality;

namespace {

// Numeric values are used intentionally so the build remains source-compatible
// with older Windows SDK headers. Unsupported attributes simply fail harmlessly
// on pre-Windows-11 systems.
const DWORD kDwmUseImmersiveDarkMode = 20;
const DWORD kDwmWindowCornerPreference = 33;
const DWORD kDwmSystemBackdropType = 38;
const int kDwmCornerRound = 2;
const int kDwmBackdropMainWindow = 2;
const UINT_PTR kMainWindowSubclassId = 0x50445711;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
const int kModernMenuCommandId = 50000;
const WPARAM kLegacySecondTimer = 103;

HFONT g_dialogFont = NULL;
HFONT g_headerFont = NULL;
HHOOK g_dialogHook = NULL;
bool g_chromeEnabled = false;
bool g_legacyMenuDetached = false;

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

void StyleMainToolbar(HWND mainWindow)
{
    HWND toolbar = FindWindowExW(mainWindow, NULL, TOOLBARCLASSNAMEW, NULL);
    if (!toolbar) return;

    // Preserve all existing command IDs while replacing the 16x16 bitmap strip
    // with a compact Windows 11 text command surface.
    static const char* labels[] = {
        "Log", "Copy", "Monitor", "Filtered", "Save", "Print",
        "Options", "Filters", "Stats", "Pause", "Help", "Clear", "Mode"
    };

    LONG_PTR style = GetWindowLongPtr(toolbar, GWL_STYLE);
    style |= TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST | CCS_NODIVIDER;
    style &= ~WS_BORDER;
    SetWindowLongPtr(toolbar, GWL_STYLE, style);

    SetWindowTheme(toolbar, L"Explorer", NULL);
    SendMessage(toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DOUBLEBUFFER);
    SendMessage(toolbar, TB_SETPADDING, 0, MAKELPARAM(9, 5));

    HFONT font = GetDialogFont();
    if (font) SendMessage(toolbar, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    const int count = static_cast<int>(SendMessage(toolbar, TB_BUTTONCOUNT, 0, 0));
    int textIndex = 0;
    for (int i = 0; i < count && textIndex < static_cast<int>(sizeof(labels) / sizeof(labels[0])); ++i)
    {
        TBBUTTON button;
        ZeroMemory(&button, sizeof(button));
        if (!SendMessage(toolbar, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&button))) continue;
        if (button.fsStyle & BTNS_SEP) continue;

        TBBUTTONINFOA info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_TEXT | TBIF_IMAGE | TBIF_STYLE;
        info.pszText = const_cast<LPSTR>(labels[textIndex++]);
        info.iImage = I_IMAGENONE;
        info.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        SendMessageA(toolbar, TB_SETBUTTONINFOA, button.idCommand,
                     reinterpret_cast<LPARAM>(&info));
    }

    // The old menu bar is exposed through one compact Menu entry. This keeps
    // every legacy function reachable without carrying the classic menu strip.
    if (SendMessage(toolbar, TB_COMMANDTOINDEX, kModernMenuCommandId, 0) == -1)
    {
        const LRESULT stringIndex = SendMessageA(toolbar, TB_ADDSTRINGA, 0,
                                                  reinterpret_cast<LPARAM>("Menu"));
        TBBUTTON menuButton;
        ZeroMemory(&menuButton, sizeof(menuButton));
        menuButton.iBitmap = I_IMAGENONE;
        menuButton.idCommand = kModernMenuCommandId;
        menuButton.fsState = TBSTATE_ENABLED;
        menuButton.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
        menuButton.iString = stringIndex;
        SendMessageA(toolbar, TB_INSERTBUTTONA, 0, reinterpret_cast<LPARAM>(&menuButton));
    }

    SendMessage(toolbar, TB_AUTOSIZE, 0, 0);
    InvalidateRect(toolbar, NULL, TRUE);
}

UINT MenuStateToFlags(UINT state)
{
    UINT flags = MF_STRING;
    if (state & MFS_CHECKED) flags |= MF_CHECKED;
    if (state & (MFS_DISABLED | MFS_GRAYED)) flags |= MF_GRAYED;
    return flags;
}

HMENU CloneMenuTree(HMENU source)
{
    if (!source) return NULL;

    HMENU copy = CreatePopupMenu();
    if (!copy) return NULL;

    const int count = GetMenuItemCount(source);
    for (int i = 0; i < count; ++i)
    {
        char text[256];
        ZeroMemory(text, sizeof(text));

        MENUITEMINFOA item;
        ZeroMemory(&item, sizeof(item));
        item.cbSize = sizeof(item);
        item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU | MIIM_STRING;
        item.dwTypeData = text;
        item.cch = sizeof(text) - 1;

        if (!GetMenuItemInfoA(source, i, TRUE, &item)) continue;

        if (item.fType & MFT_SEPARATOR)
        {
            AppendMenuA(copy, MF_SEPARATOR, 0, NULL);
            continue;
        }

        if (item.hSubMenu)
        {
            HMENU child = CloneMenuTree(item.hSubMenu);
            if (child)
            {
                AppendMenuA(copy, MenuStateToFlags(item.fState) | MF_POPUP,
                            reinterpret_cast<UINT_PTR>(child), text);
            }
        }
        else
        {
            AppendMenuA(copy, MenuStateToFlags(item.fState), item.wID, text);
        }
    }

    return copy;
}

void ShowModernMenu(HWND mainWindow)
{
    if (!ghMenu) ghMenu = GetMenu(mainWindow);
    if (!ghMenu) return;

    HWND toolbar = FindWindowExW(mainWindow, NULL, TOOLBARCLASSNAMEW, NULL);
    if (!toolbar) return;

    HMENU popup = CloneMenuTree(ghMenu);
    if (!popup) return;

    RECT buttonRect;
    ZeroMemory(&buttonRect, sizeof(buttonRect));
    const LRESULT index = SendMessage(toolbar, TB_COMMANDTOINDEX, kModernMenuCommandId, 0);
    if (index >= 0)
    {
        SendMessage(toolbar, TB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&buttonRect));
    }
    MapWindowPoints(toolbar, NULL, reinterpret_cast<POINT*>(&buttonRect), 2);

    const UINT command = TrackPopupMenu(popup,
                                        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        buttonRect.left, buttonRect.bottom + 2, 0,
                                        mainWindow, NULL);
    if (command)
    {
        SendMessage(mainWindow, WM_COMMAND, MAKEWPARAM(command, 0), 0);
    }

    DestroyMenu(popup);
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

    POINT points[2] = {
        { paneRect.left, paneRect.top },
        { paneRect.right, paneRect.bottom }
    };
    MapWindowPoints(NULL, mainWindow, points, 2);

    RECT client;
    GetClientRect(mainWindow, &client);

    const int headerHeight = TITLE_BAR_SIZE;
    const int y = points[0].y - headerHeight - 1;
    if (y < 0) return;

    HDC hdc = GetDC(mainWindow);
    if (!hdc) return;

    SetMessageItemPositionsWidth();
    const bool dark = AppsPreferDarkMode();
    const int rxWidth = withRx ? 92 : 0;
    int left = filteredPane ? PL2_SCount : PL1_SCount;

    for (int i = 0; i < 7; ++i)
    {
        const int item = Profile.ScreenColumns[i];
        if (item == 0) break;

        int width = 0;
        if (item == 7)
        {
            width = client.right - left - rxWidth;
        }
        else
        {
            width = iItemWidths[item];
            if (i == 0) width += cxChar;
        }

        const char* label = HeaderLabelForItem(item);
        if (item == 7) label = filteredPane ? "Filtered messages" : "Monitored messages";

        DrawHeaderCell(hdc, left, y, width, headerHeight, label, dark,
                       item != 7 || withRx);
        left += width;
    }

    if (withRx)
    {
        char rxText[32];
        COLORREF rxColor;
        if (dRX_Quality <= 0.0)
        {
            strcpy_s(rxText, sizeof(rxText), "RX idle");
            rxColor = dark ? RGB(180, 180, 180) : RGB(96, 96, 96);
        }
        else
        {
            sprintf_s(rxText, sizeof(rxText), "RX %.0f%%", dRX_Quality);
            rxColor = dRX_Quality >= 96.0 ? RGB(16, 124, 16) : RGB(196, 43, 28);
        }

        const int rxLeft = client.right - rxWidth;
        DrawHeaderCell(hdc, rxLeft, y, rxWidth, headerHeight, "", dark, false);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, rxColor);
        HFONT font = GetHeaderFont();
        HGDIOBJ oldFont = font ? SelectObject(hdc, font) : NULL;
        RECT rxRect = { rxLeft + 4, y, client.right - 4, y + headerHeight };
        DrawTextA(hdc, rxText, -1, &rxRect,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (oldFont) SelectObject(hdc, oldFont);
    }

    ReleaseDC(mainWindow, hdc);
}

void DrawModernMainChrome(HWND hwnd)
{
    HWND pane1 = FindWindowExA(hwnd, NULL, "WinPDWPane1Class", NULL);
    HWND pane2 = FindWindowExA(hwnd, NULL, "WinPDWPane2Class", NULL);
    DrawPaneHeader(hwnd, pane1, false, true);
    DrawPaneHeader(hwnd, pane2, true, false);
}

LRESULT CALLBACK MainWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR subclassId,
                                        DWORD_PTR referenceData)
{
    if (message == WM_COMMAND && LOWORD(wParam) == kModernMenuCommandId)
    {
        ShowModernMenu(hwnd);
        return 0;
    }

    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case kEnableModernShellMessage:
            DetachLegacyMenu(hwnd);
            StyleMainToolbar(hwnd);
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
        {
            pdw::ApplyWindows11DialogStyle(info->hwnd);
        }
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

    StyleMainToolbar(hwnd);
    SetWindowSubclass(hwnd, MainWindowSubclassProc, kMainWindowSubclassId, 0);

    // Defer removing the old menu strip until WinMain has populated dynamic
    // language entries and menu check states. The posted message is handled
    // only once the application's normal message loop starts.
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
