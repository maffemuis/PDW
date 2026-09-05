#include "windows11_ui.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>

#include "..\\Headers\\pdw.h"
#include "..\\Headers\\gfx.h"
#include "..\\Headers\\initapp.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

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

HFONT g_dialogFont = NULL;
HFONT g_headerFont = NULL;
HHOOK g_dialogHook = NULL;

extern double dRX_Quality;

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
        // Windows supplies the correct Segoe UI family and current DPI-aware
        // system metrics, so legacy dialogs no longer force an XP-era font.
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

    // Explorer theme maps buttons, edits, combo boxes, list views and scroll
    // bars to the current Windows common-control visual style.
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

    // Keep every existing command ID, but replace the 16x16 bitmap strip with
    // a compact Windows-11-style text command bar. All commands remain available
    // from the normal menu as well.
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
    SendMessage(toolbar, TB_SETPADDING, 0, MAKELPARAM(7, 3));

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
        SendMessageA(toolbar, TB_SETBUTTONINFOA, button.idCommand, reinterpret_cast<LPARAM>(&info));
    }

    SendMessage(toolbar, TB_AUTOSIZE, 0, 0);
    InvalidateRect(toolbar, NULL, TRUE);
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
    const int rxWidth = withRx ? 58 : 0;
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
        char rxText[24];
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
    const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

    switch (message)
    {
        case WM_PAINT:
        case WM_SIZE:
        case WM_NOTIFY:
            DrawModernMainChrome(hwnd);
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
            // WH_CALLWNDPROCRET runs after the existing dialog procedure has
            // initialized all controls, so this does not change legacy logic.
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

    ApplyRoundedCorners(hwnd);

    const BOOL dark = AppsPreferDarkMode() ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));

    // Mica/backdrop is intentionally best-effort. The legacy decoder and pane
    // rendering stay intact; only the surrounding shell/chrome is modernized.
    int backdrop = kDwmBackdropMainWindow;
    DwmSetWindowAttribute(hwnd, kDwmSystemBackdropType, &backdrop, sizeof(backdrop));

    // CreateWindow has completed at this point, so PDW's legacy font metrics
    // are initialized. Styling the toolbar here therefore cannot trigger the
    // early-paint divide-by-zero that the first preview exposed.
    StyleMainToolbar(hwnd);
    SetWindowSubclass(hwnd, MainWindowSubclassProc, kMainWindowSubclassId, 0);
}

void InstallWindows11DialogStyling()
{
    if (g_dialogHook) return;

    // Thread-local only: PDW's own dialogs are modernized, never unrelated
    // processes/windows. Failure is non-fatal and leaves the legacy UI intact.
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

} // namespace pdw
