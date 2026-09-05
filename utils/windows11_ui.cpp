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
const UINT_PTR kFilterWindowSubclassId = 0x50445731;
const UINT_PTR kFilterEditWindowSubclassId = 0x50445732;
const UINT_PTR kModernGroupBoxSubclassId = 0x50445733;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
const WPARAM kLegacySecondTimer = 103;
const int kSettingsPopupCommand = 50001;
const int kResumeMonitorCommand = 50002;

const wchar_t* kSettingsFlyoutClass = L"PDW.Windows11.SettingsFlyout";

HFONT g_dialogFont = NULL;
HFONT g_headerFont = NULL;
HFONT g_titleFont = NULL;
HFONT g_iconFont = NULL;
HFONT g_filterListFont = NULL;
HHOOK g_dialogHook = NULL;
HBRUSH g_dialogSurfaceBrush = NULL;
bool g_chromeEnabled = false;
bool g_legacyMenuDetached = false;

HWND g_mainWindow = NULL;
HWND g_settingsFlyout = NULL;
int g_hoverTarget = -1;
int g_pressedTarget = -1;
int g_flyoutHover = -1;

RECT g_pane1Card = {};
RECT g_pane2Card = {};
RECT g_pane1Body = {};
RECT g_pane2Body = {};
RECT g_statusRect = {};

struct ShellHitTarget
{
    RECT rect;
    int command;
};

ShellHitTarget g_navTargets[6];
ShellHitTarget g_commandTargets[11];
int g_navTargetCount = 0;
int g_commandTargetCount = 0;

struct FlyoutItem
{
    const wchar_t* icon;
    const wchar_t* label;
    int command;
};

const FlyoutItem g_flyoutItems[] = {
    { L"\xE713", L"General", IDM_GENERAL },
    { L"\xE7F4", L"Interface / input", IDM_INTERFACE },
    { L"\xE790", L"Display layout", IDM_SCREENOPTIONS },
    { L"\xE790", L"Colors", IDM_COLOR },
    { L"\xE8D2", L"Font", IDM_FONT },
    { L"\xE8A5", L"Data & logging", IDM_LOGFILE },
    { L"\xE715", L"SMTP / network", IDM_MAIL },
    { L"\xE946", L"About PDW", IDM_ABOUT }
};

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

int ShellHeight(HWND hwnd)       { return ScaleForDpi(hwnd, 112); }
int NavTop(HWND hwnd)            { return ScaleForDpi(hwnd, 8); }
int NavHeight(HWND hwnd)         { return ScaleForDpi(hwnd, 42); }
int CommandTop(HWND hwnd)        { return ScaleForDpi(hwnd, 60); }
int CommandHeight(HWND hwnd)     { return ScaleForDpi(hwnd, 44); }
int WorkspaceMargin(HWND hwnd)   { return ScaleForDpi(hwnd, 12); }
int CardTitleHeight(HWND hwnd)   { return ScaleForDpi(hwnd, 34); }
int ColumnHeaderHeight(HWND hwnd){ return ScaleForDpi(hwnd, 28); }
int CardGap(HWND hwnd)           { return ScaleForDpi(hwnd, 12); }
int StatusHeight(HWND hwnd)      { return ScaleForDpi(hwnd, 34); }

HFONT GetDialogFont()
{
    if (g_dialogFont) return g_dialogFont;
    g_dialogFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return g_dialogFont;
}

HFONT GetHeaderFont()
{
    if (g_headerFont) return g_headerFont;
    g_headerFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return g_headerFont ? g_headerFont : GetDialogFont();
}

HFONT GetTitleFont()
{
    if (g_titleFont) return g_titleFont;
    g_titleFont = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    return g_titleFont ? g_titleFont : GetHeaderFont();
}

HFONT GetIconFont()
{
    if (g_iconFont) return g_iconFont;
    g_iconFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    return g_iconFont;
}

HFONT GetFilterListFont()
{
    if (g_filterListFont) return g_filterListFont;
    g_filterListFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   FIXED_PITCH | FF_MODERN, L"Consolas");
    return g_filterListFont ? g_filterListFont : GetDialogFont();
}

HBRUSH GetDialogSurfaceBrush()
{
    if (!g_dialogSurfaceBrush)
        g_dialogSurfaceBrush = CreateSolidBrush(RGB(246, 249, 252));
    return g_dialogSurfaceBrush;
}

void ApplyRoundedCorners(HWND hwnd)
{
    if (!hwnd) return;
    int preference = kDwmCornerRound;
    DwmSetWindowAttribute(hwnd, kDwmWindowCornerPreference,
                          &preference, sizeof(preference));
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

void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void HideLegacyToolbar(HWND mainWindow)
{
    if (hToolbar) ShowWindow(hToolbar, SW_HIDE);
    HWND toolbar = FindWindowExW(mainWindow, NULL, TOOLBARCLASSNAMEW, NULL);
    if (toolbar) ShowWindow(toolbar, SW_HIDE);
}

void DetachLegacyMenu(HWND mainWindow)
{
    if (g_legacyMenuDetached) return;
    if (!ghMenu) ghMenu = GetMenu(mainWindow);
    if (GetMenu(mainWindow))
    {
        SetMenu(mainWindow, NULL);
        DrawMenuBar(mainWindow);
    }
    g_legacyMenuDetached = true;
}

void BuildShellTargets(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    const int navTop = NavTop(hwnd);
    const int navHeight = NavHeight(hwnd);
    const int navWidths[] = {
        ScaleForDpi(hwnd, 128), ScaleForDpi(hwnd, 116), ScaleForDpi(hwnd, 116),
        ScaleForDpi(hwnd, 104), ScaleForDpi(hwnd, 106), ScaleForDpi(hwnd, 130)
    };
    const int navCommands[] = {
        0, IDM_FILTERS, IDM_PLAYBACK, IDM_LOGFILE, IDM_MAIL, kSettingsPopupCommand
    };

    g_navTargetCount = 0;
    int x = ScaleForDpi(hwnd, 14);
    for (int i = 0; i < 6; ++i)
    {
        if (x + navWidths[i] >= client.right - ScaleForDpi(hwnd, 8)) break;
        SetRect(&g_navTargets[g_navTargetCount].rect,
                x, navTop, x + navWidths[i], navTop + navHeight);
        g_navTargets[g_navTargetCount].command = navCommands[i];
        ++g_navTargetCount;
        x += navWidths[i] + ScaleForDpi(hwnd, 4);
    }

    const int commandTop = CommandTop(hwnd);
    const int commandHeight = CommandHeight(hwnd);
    const int commandWidths[] = {
        ScaleForDpi(hwnd, 94), ScaleForDpi(hwnd, 90), ScaleForDpi(hwnd, 92),
        ScaleForDpi(hwnd, 90), ScaleForDpi(hwnd, 112), ScaleForDpi(hwnd, 96),
        ScaleForDpi(hwnd, 106), ScaleForDpi(hwnd, 112), ScaleForDpi(hwnd, 96),
        ScaleForDpi(hwnd, 94), ScaleForDpi(hwnd, 100)
    };
    const int commands[] = {
        IDM_PLAYBACK, IDM_COPY_SAVE, IDM_COPY_PRINT, IDM_COPY_SELECTION,
        kResumeMonitorCommand, IDT_TOOLBAR_BTN9, IDM_FILTERS, IDM_OPTIONS,
        IDM_MONSTAT, IDM_CLEARDISPLAY, IDT_TOOLBAR_BTN12
    };

    g_commandTargetCount = 0;
    x = ScaleForDpi(hwnd, 22);
    for (int i = 0; i < 11; ++i)
    {
        if (x + commandWidths[i] >= client.right - ScaleForDpi(hwnd, 22)) break;
        SetRect(&g_commandTargets[g_commandTargetCount].rect,
                x, commandTop + ScaleForDpi(hwnd, 4),
                x + commandWidths[i], commandTop + commandHeight - ScaleForDpi(hwnd, 4));
        g_commandTargets[g_commandTargetCount].command = commands[i];
        ++g_commandTargetCount;
        x += commandWidths[i] + ScaleForDpi(hwnd, 4);
    }
}

int HitTestShell(HWND hwnd, POINT point)
{
    BuildShellTargets(hwnd);
    for (int i = 0; i < g_navTargetCount; ++i)
        if (PtInRect(&g_navTargets[i].rect, point)) return i;
    for (int i = 0; i < g_commandTargetCount; ++i)
        if (PtInRect(&g_commandTargets[i].rect, point)) return 100 + i;
    return -1;
}

int CommandForTarget(int target)
{
    if (target >= 0 && target < g_navTargetCount)
        return g_navTargets[target].command;
    const int index = target - 100;
    if (index >= 0 && index < g_commandTargetCount)
        return g_commandTargets[index].command;
    return 0;
}

void DrawIconTextButton(HDC hdc, const RECT& rect, const wchar_t* icon,
                        const wchar_t* label, bool selected, bool hovered, bool pressed)
{
    const COLORREF accent = RGB(0, 120, 212);
    const COLORREF accentPressed = RGB(0, 95, 184);
    const COLORREF normalBg = RGB(250, 250, 250);
    const COLORREF hoverBg = RGB(240, 246, 252);
    const COLORREF fg = RGB(32, 32, 32);
    const COLORREF selectedFg = RGB(255, 255, 255);

    COLORREF fill = normalBg;
    COLORREF border = normalBg;
    if (selected || pressed)
    {
        fill = pressed ? accentPressed : accent;
        border = fill;
    }
    else if (hovered)
    {
        fill = hoverBg;
        border = RGB(224, 235, 246);
    }
    FillRoundedRect(hdc, rect, fill, border, ScaleForDpi(g_mainWindow, 18));

    SetBkMode(hdc, TRANSPARENT);
    const bool active = selected || pressed;
    SetTextColor(hdc, active ? selectedFg : accent);

    RECT iconRect = rect;
    iconRect.left += ScaleForDpi(g_mainWindow, 12);
    iconRect.right = iconRect.left + ScaleForDpi(g_mainWindow, 22);
    HGDIOBJ oldFont = SelectObject(hdc, GetIconFont());
    DrawTextW(hdc, icon, -1, &iconRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    RECT textRect = rect;
    textRect.left += ScaleForDpi(g_mainWindow, 40);
    textRect.right -= ScaleForDpi(g_mainWindow, 10);
    SetTextColor(hdc, active ? selectedFg : fg);
    oldFont = SelectObject(hdc, active ? GetHeaderFont() : GetDialogFont());
    DrawTextW(hdc, label, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
}

void DrawTopNavigation(HDC hdc, HWND hwnd, const RECT& client)
{
    BuildShellTargets(hwnd);
    const COLORREF shellBg = RGB(247, 250, 253);
    const COLORREF capsuleBg = RGB(249, 251, 253);
    const COLORREF capsuleBorder = RGB(224, 231, 239);

    RECT row = { 0, 0, client.right, CommandTop(hwnd) - ScaleForDpi(hwnd, 3) };
    HBRUSH rowBrush = CreateSolidBrush(shellBg);
    FillRect(hdc, &row, rowBrush);
    DeleteObject(rowBrush);

    if (g_navTargetCount > 0)
    {
        RECT capsule = g_navTargets[0].rect;
        capsule.left -= ScaleForDpi(hwnd, 4);
        capsule.top -= ScaleForDpi(hwnd, 2);
        capsule.right = g_navTargets[g_navTargetCount - 1].rect.right + ScaleForDpi(hwnd, 4);
        capsule.bottom += ScaleForDpi(hwnd, 2);
        FillRoundedRect(hdc, capsule, capsuleBg, capsuleBorder, ScaleForDpi(hwnd, 24));
    }

    const wchar_t* labels[] = { L"Monitor", L"Filters", L"Replay", L"Logs", L"SMTP", L"Settings" };
    const wchar_t* icons[]  = { L"\xE7F4", L"\xE71C", L"\xE72C", L"\xE8A5", L"\xE715", L"\xE713" };

    for (int i = 0; i < g_navTargetCount; ++i)
    {
        const bool selected = (i == 0 && !g_settingsFlyout) || (i == 5 && g_settingsFlyout);
        DrawIconTextButton(hdc, g_navTargets[i].rect, icons[i], labels[i], selected,
                           g_hoverTarget == i, g_pressedTarget == i);
    }
}

void DrawCommandStrip(HDC hdc, HWND hwnd, const RECT& client)
{
    BuildShellTargets(hwnd);
    const COLORREF rowBg = RGB(252, 253, 254);
    const COLORREF rowBorder = RGB(218, 226, 235);
    const int top = CommandTop(hwnd);
    const int height = CommandHeight(hwnd);

    RECT bar = {
        ScaleForDpi(hwnd, 14), top,
        client.right - ScaleForDpi(hwnd, 14), top + height
    };
    FillRoundedRect(hdc, bar, rowBg, rowBorder, ScaleForDpi(hwnd, 12));

    const wchar_t* labels[] = {
        L"Open", L"Save", L"Print", L"Copy", L"Monitor", L"Pause",
        L"Filters", L"Options", L"Stats", L"Clear", L"Mode"
    };
    const wchar_t* icons[] = {
        L"\xE8B7", L"\xE74E", L"\xE749", L"\xE8C8", L"\xE768", L"\xE769",
        L"\xE71C", L"\xE713", L"\xE9D9", L"\xE894", L"\xE7F4"
    };

    for (int i = 0; i < g_commandTargetCount; ++i)
    {
        const int target = 100 + i;
        DrawIconTextButton(hdc, g_commandTargets[i].rect, icons[i], labels[i], false,
                           g_hoverTarget == target, g_pressedTarget == target);
    }
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

void DrawColumnHeaders(HDC hdc, HWND hwnd, const RECT& body, bool filteredPane)
{
    if (cxChar == 0) return;
    SetMessageItemPositionsWidth();

    const int headerHeight = ColumnHeaderHeight(hwnd);
    const int y = body.top - headerHeight;
    const COLORREF background = RGB(249, 251, 253);
    const COLORREF foreground = RGB(45, 45, 45);
    const COLORREF divider = RGB(225, 230, 236);

    RECT header = { body.left, y, body.right, body.top };
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(hdc, &header, brush);
    DeleteObject(brush);

    const int saved = SaveDC(hdc);
    IntersectClipRect(hdc, header.left, header.top, header.right, header.bottom);

    int left = body.left + (filteredPane ? PL2_SCount : PL1_SCount);
    for (int i = 0; i < 7; ++i)
    {
        const int item = Profile.ScreenColumns[i];
        if (item == 0) break;
        int width = 0;
        if (item == 7)
            width = body.right - left;
        else
        {
            width = iItemWidths[item];
            if (i == 0) width += cxChar;
        }
        if (width <= 0) continue;

        RECT cell = { left, y, left + width, body.top };
        DrawLine(hdc, cell.right - 1, cell.top + ScaleForDpi(hwnd, 6),
                 cell.right - 1, cell.bottom - ScaleForDpi(hwnd, 6), divider);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, foreground);
        HGDIOBJ oldFont = SelectObject(hdc, GetHeaderFont());
        RECT text = cell;
        text.left += ScaleForDpi(hwnd, 10);
        text.right -= ScaleForDpi(hwnd, 6);
        DrawTextA(hdc, HeaderLabelForItem(item), -1, &text,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
        left += width;
        if (left >= body.right) break;
    }

    RestoreDC(hdc, saved);
    DrawLine(hdc, body.left, body.top - 1, body.right, body.top - 1, divider);
}

void DrawCard(HDC hdc, HWND hwnd, const RECT& card, const RECT& body,
              const wchar_t* title, bool withRx)
{
    const COLORREF cardBg = RGB(255, 255, 255);
    const COLORREF cardBorder = RGB(206, 217, 229);
    const COLORREF titleBg = RGB(245, 249, 253);
    const COLORREF titleFg = RGB(24, 39, 58);
    const int radius = ScaleForDpi(hwnd, 10);
    const int titleHeight = CardTitleHeight(hwnd);

    FillRoundedRect(hdc, card, cardBg, cardBorder, radius);

    RECT titleRect = { card.left + 1, card.top + 1, card.right - 1, card.top + titleHeight };
    HBRUSH titleBrush = CreateSolidBrush(titleBg);
    FillRect(hdc, &titleRect, titleBrush);
    DeleteObject(titleBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, titleFg);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT textRect = titleRect;
    textRect.left += ScaleForDpi(hwnd, 14);
    textRect.right -= ScaleForDpi(hwnd, 14);
    DrawTextW(hdc, title, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    if (withRx)
    {
        const bool active = !bPauseFlag && dRX_Quality > 0.0;
        const COLORREF dot = active ? RGB(20, 170, 62) : RGB(154, 160, 168);
        const int dotSize = ScaleForDpi(hwnd, 10);
        const int dotRight = card.right - ScaleForDpi(hwnd, 14);
        const int cy = titleRect.top + (titleHeight / 2);
        HBRUSH dotBrush = CreateSolidBrush(dot);
        HPEN dotPen = CreatePen(PS_SOLID, 1, dot);
        HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
        HGDIOBJ oldPen = SelectObject(hdc, dotPen);
        Ellipse(hdc, dotRight - dotSize, cy - dotSize / 2,
                dotRight, cy + dotSize / 2);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(dotPen);
        DeleteObject(dotBrush);

        SetTextColor(hdc, titleFg);
        oldFont = SelectObject(hdc, GetHeaderFont());
        RECT rxText = { dotRight - ScaleForDpi(hwnd, 70), titleRect.top,
                        dotRight - ScaleForDpi(hwnd, 16), titleRect.bottom };
        DrawTextW(hdc, L"RX-Q", -1, &rxText,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }

    DrawLine(hdc, card.left + 1, card.top + titleHeight,
             card.right - 1, card.top + titleHeight, cardBorder);
    DrawColumnHeaders(hdc, hwnd, body, wcscmp(title, L"Filtered messages") == 0);
}

const wchar_t* CurrentModeLabel()
{
    if (Profile.monitor_acars) return L"ACARS";
    if (Profile.monitor_mobitex) return L"MOBITEX";
    if (Profile.monitor_ermes) return L"ERMES";
    return L"POCSAG / FLEX";
}

void DrawStatusBar(HDC hdc, HWND hwnd)
{
    const COLORREF bg = RGB(249, 251, 253);
    const COLORREF line = RGB(216, 224, 233);
    const COLORREF fg = RGB(42, 51, 61);
    const COLORREF green = RGB(20, 170, 62);
    const COLORREF paused = RGB(196, 120, 0);
    const COLORREF gray = RGB(154, 160, 168);

    HBRUSH brush = CreateSolidBrush(bg);
    FillRect(hdc, &g_statusRect, brush);
    DeleteObject(brush);
    DrawLine(hdc, g_statusRect.left, g_statusRect.top,
             g_statusRect.right, g_statusRect.top, line);

    const int centerY = (g_statusRect.top + g_statusRect.bottom) / 2;
    const int dot = ScaleForDpi(hwnd, 10);
    const COLORREF stateColor = bPauseFlag ? paused : green;
    HBRUSH dotBrush = CreateSolidBrush(stateColor);
    HPEN dotPen = CreatePen(PS_SOLID, 1, stateColor);
    HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
    HGDIOBJ oldPen = SelectObject(hdc, dotPen);
    Ellipse(hdc, ScaleForDpi(hwnd, 16), centerY - dot / 2,
            ScaleForDpi(hwnd, 16) + dot, centerY + dot / 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(dotPen);
    DeleteObject(dotBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    HGDIOBJ oldFont = SelectObject(hdc, GetDialogFont());
    RECT ready = { ScaleForDpi(hwnd, 34), g_statusRect.top,
                   ScaleForDpi(hwnd, 120), g_statusRect.bottom };
    DrawTextW(hdc, bPauseFlag ? L"Paused" : L"Ready", -1, &ready,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    wchar_t filters[64] = {};
    swprintf(filters, ARRAYSIZE(filters), L"Filters: %u",
             static_cast<unsigned int>(Profile.filters.size()));
    RECT filterRect = { g_statusRect.right - ScaleForDpi(hwnd, 330), g_statusRect.top,
                        g_statusRect.right - ScaleForDpi(hwnd, 220), g_statusRect.bottom };
    DrawTextW(hdc, filters, -1, &filterRect,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT modeRect = { g_statusRect.right - ScaleForDpi(hwnd, 210), g_statusRect.top,
                      g_statusRect.right - ScaleForDpi(hwnd, 90), g_statusRect.bottom };
    DrawTextW(hdc, CurrentModeLabel(), -1, &modeRect,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const bool rxActive = !bPauseFlag && dRX_Quality > 0.0;
    const COLORREF rxColor = rxActive ? green : gray;
    HBRUSH rxBrush = CreateSolidBrush(rxColor);
    HPEN rxPen = CreatePen(PS_SOLID, 1, rxColor);
    oldBrush = SelectObject(hdc, rxBrush);
    oldPen = SelectObject(hdc, rxPen);
    const int rxX = g_statusRect.right - ScaleForDpi(hwnd, 70);
    Ellipse(hdc, rxX, centerY - dot / 2, rxX + dot, centerY + dot / 2);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(rxPen);
    DeleteObject(rxBrush);

    RECT rxText = { rxX + ScaleForDpi(hwnd, 16), g_statusRect.top,
                    g_statusRect.right - ScaleForDpi(hwnd, 12), g_statusRect.bottom };
    DrawTextW(hdc, L"RX-Q", -1, &rxText,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);
}

void LayoutModernWorkspace(HWND hwnd)
{
    if (!Pane1.hWnd || !Pane2.hWnd) return;

    RECT client = {};
    GetClientRect(hwnd, &client);
    const int margin = WorkspaceMargin(hwnd);
    const int shellHeight = ShellHeight(hwnd);
    const int titleHeight = CardTitleHeight(hwnd);
    const int columnHeight = ColumnHeaderHeight(hwnd);
    const int gap = CardGap(hwnd);
    const int statusHeight = StatusHeight(hwnd);

    const int workspaceTop = shellHeight + margin;
    const int workspaceBottom = client.bottom - statusHeight - margin;
    const int available = workspaceBottom - workspaceTop - gap -
                          2 * (titleHeight + columnHeight);
    if (available < ScaleForDpi(hwnd, 160)) return;

    int firstBodyHeight = (available * Profile.percent) / 100;
    const int minBody = ScaleForDpi(hwnd, 90);
    if (firstBodyHeight < minBody) firstBodyHeight = minBody;
    if (available - firstBodyHeight < minBody) firstBodyHeight = available - minBody;

    const int left = margin;
    const int right = client.right - margin;

    g_pane1Card.left = left;
    g_pane1Card.top = workspaceTop;
    g_pane1Card.right = right;
    g_pane1Card.bottom = workspaceTop + titleHeight + columnHeight + firstBodyHeight;

    g_pane1Body.left = left + 1;
    g_pane1Body.top = workspaceTop + titleHeight + columnHeight;
    g_pane1Body.right = right - 1;
    g_pane1Body.bottom = g_pane1Card.bottom - 1;

    g_pane2Card.left = left;
    g_pane2Card.top = g_pane1Card.bottom + gap;
    g_pane2Card.right = right;
    g_pane2Card.bottom = workspaceBottom;

    g_pane2Body.left = left + 1;
    g_pane2Body.top = g_pane2Card.top + titleHeight + columnHeight;
    g_pane2Body.right = right - 1;
    g_pane2Body.bottom = g_pane2Card.bottom - 1;

    g_statusRect.left = 0;
    g_statusRect.top = client.bottom - statusHeight;
    g_statusRect.right = client.right;
    g_statusRect.bottom = client.bottom;

    pane1Pos = g_pane1Body.top;
    pane1Height = g_pane1Body.bottom - g_pane1Body.top;
    pane2Pos = g_pane2Body.top;
    pane2Height = g_pane2Body.bottom - g_pane2Body.top;

    MoveWindow(Pane1.hWnd, g_pane1Body.left, g_pane1Body.top,
               g_pane1Body.right - g_pane1Body.left,
               g_pane1Body.bottom - g_pane1Body.top, TRUE);
    MoveWindow(Pane2.hWnd, g_pane2Body.left, g_pane2Body.top,
               g_pane2Body.right - g_pane2Body.left,
               g_pane2Body.bottom - g_pane2Body.top, TRUE);

    RECT paneRect = {};
    if (GetWindowRect(Pane1.hWnd, &paneRect)) pane1Top = paneRect.top;
}

void DrawModernWorkspace(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HDC hdc = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_CLIPCHILDREN);
    if (!hdc) hdc = GetDC(hwnd);
    if (!hdc) return;

    const COLORREF workspace = RGB(241, 246, 251);
    HBRUSH background = CreateSolidBrush(workspace);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    DrawTopNavigation(hdc, hwnd, client);
    DrawCommandStrip(hdc, hwnd, client);
    DrawCard(hdc, hwnd, g_pane1Card, g_pane1Body, L"Monitored messages", true);
    DrawCard(hdc, hwnd, g_pane2Card, g_pane2Body, L"Filtered messages", false);
    DrawStatusBar(hdc, hwnd);

    ReleaseDC(hwnd, hdc);
}

void CloseSettingsFlyout()
{
    if (g_settingsFlyout && IsWindow(g_settingsFlyout))
        DestroyWindow(g_settingsFlyout);
    g_settingsFlyout = NULL;
}

LRESULT CALLBACK SettingsFlyoutProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const int rowHeight = ScaleForDpi(g_mainWindow ? g_mainWindow : hwnd, 38);
    const int topPadding = ScaleForDpi(g_mainWindow ? g_mainWindow : hwnd, 8);

    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
        {
            const int y = GET_Y_LPARAM(lParam) - topPadding;
            const int count = static_cast<int>(ARRAYSIZE(g_flyoutItems));
            int hover = (y >= 0) ? (y / rowHeight) : -1;
            if (hover < 0 || hover >= count) hover = -1;
            if (hover != g_flyoutHover)
            {
                g_flyoutHover = hover;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            return 0;
        }

        case WM_MOUSELEAVE:
            g_flyoutHover = -1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_LBUTTONUP:
        {
            const int y = GET_Y_LPARAM(lParam) - topPadding;
            const int index = y >= 0 ? y / rowHeight : -1;
            const int count = static_cast<int>(ARRAYSIZE(g_flyoutItems));
            if (index >= 0 && index < count)
            {
                const int command = g_flyoutItems[index].command;
                HWND owner = GetWindow(hwnd, GW_OWNER);
                DestroyWindow(hwnd);
                if (owner && command)
                    PostMessage(owner, WM_COMMAND, MAKEWPARAM(command, 0), 0);
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdc, &client, bg);
            DeleteObject(bg);

            const int count = static_cast<int>(ARRAYSIZE(g_flyoutItems));
            for (int i = 0; i < count; ++i)
            {
                RECT row = {
                    ScaleForDpi(hwnd, 8), topPadding + i * rowHeight,
                    client.right - ScaleForDpi(hwnd, 8), topPadding + (i + 1) * rowHeight
                };
                if (i == g_flyoutHover)
                    FillRoundedRect(hdc, row, RGB(239, 246, 252), RGB(239, 246, 252),
                                    ScaleForDpi(hwnd, 8));

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0, 120, 212));
                HGDIOBJ oldFont = SelectObject(hdc, GetIconFont());
                RECT iconRect = row;
                iconRect.left += ScaleForDpi(hwnd, 10);
                iconRect.right = iconRect.left + ScaleForDpi(hwnd, 24);
                DrawTextW(hdc, g_flyoutItems[i].icon, -1, &iconRect,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);

                SetTextColor(hdc, RGB(32, 32, 32));
                oldFont = SelectObject(hdc, GetDialogFont());
                RECT text = row;
                text.left += ScaleForDpi(hwnd, 46);
                DrawTextW(hdc, g_flyoutItems[i].label, -1, &text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_NCDESTROY:
            if (g_settingsFlyout == hwnd) g_settingsFlyout = NULL;
            g_flyoutHover = -1;
            if (g_mainWindow) InvalidateRect(g_mainWindow, NULL, FALSE);
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

void EnsureSettingsFlyoutClass()
{
    static bool registered = false;
    if (registered) return;

    WNDCLASSW wc = {};
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = SettingsFlyoutProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = kSettingsFlyoutClass;
    RegisterClassW(&wc);
    registered = true;
}

void ToggleSettingsFlyout(HWND hwnd)
{
    if (g_settingsFlyout && IsWindow(g_settingsFlyout))
    {
        CloseSettingsFlyout();
        DrawModernWorkspace(hwnd);
        return;
    }

    BuildShellTargets(hwnd);
    if (g_navTargetCount < 6) return;
    EnsureSettingsFlyoutClass();

    const int width = ScaleForDpi(hwnd, 260);
    const int rowHeight = ScaleForDpi(hwnd, 38);
    const int height = ScaleForDpi(hwnd, 16) +
                       rowHeight * static_cast<int>(ARRAYSIZE(g_flyoutItems));

    RECT anchor = g_navTargets[5].rect;
    POINT point = { anchor.left, anchor.bottom + ScaleForDpi(hwnd, 6) };
    ClientToScreen(hwnd, &point);

    g_settingsFlyout = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kSettingsFlyoutClass, L"", WS_POPUP,
        point.x, point.y, width, height,
        hwnd, NULL, GetModuleHandleW(NULL), NULL);

    if (g_settingsFlyout)
    {
        ApplyRoundedCorners(g_settingsFlyout);
        SetWindowPos(g_settingsFlyout, HWND_TOP, point.x, point.y, width, height,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);
        DrawModernWorkspace(hwnd);
    }
}

void DispatchShellCommand(HWND hwnd, int command)
{
    if (command == 0) return;
    if (command == kSettingsPopupCommand)
    {
        ToggleSettingsFlyout(hwnd);
        return;
    }
    if (command == kResumeMonitorCommand)
    {
        if (bPauseFlag)
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDT_TOOLBAR_BTN9, 0), 0);
        return;
    }
    SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(command, 0), 0);
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

void ConfigureModernFilterControls(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    if (list)
    {
        LONG_PTR style = GetWindowLongPtr(list, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(list, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(list, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(list, GWL_EXSTYLE, exStyle);

        SetWindowTheme(list, L"Explorer", NULL);
        ListView_SetExtendedListViewStyleEx(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessage(list, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);
        if (!Profile.FilterWindowColors)
        {
            SendMessage(list, LVM_SETBKCOLOR, 0, RGB(255, 255, 255));
            SendMessage(list, LVM_SETTEXTBKCOLOR, 0, CLR_NONE);
        }
        SetWindowPos(list, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    const int ids[] = {
        IDC_FILTERADD, IDC_FILTEREDIT, IDC_FILTERDEL,
        IDC_FILTEROPTIONS, IDC_FILTERFIND, IDOK
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(ids)); ++i)
    {
        HWND button = GetDlgItem(hwnd, ids[i]);
        if (!button) continue;
        LONG_PTR style = GetWindowLongPtr(button, GWL_STYLE);
        style = (style & ~static_cast<LONG_PTR>(0x0F)) | BS_OWNERDRAW;
        SetWindowLongPtr(button, GWL_STYLE, style);
        SetWindowTheme(button, L"", L"");
        SendMessage(button, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }
}

void LayoutFilterDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int cx = client.right;
    const int cy = client.bottom;
    if (cx <= 0 || cy <= 0) return;

    const int margin = ScaleForDpi(hwnd, 16);
    const int gap = ScaleForDpi(hwnd, 8);
    const int headerHeight = ScaleForDpi(hwnd, 76);
    const int footerHeight = ScaleForDpi(hwnd, 60);
    const int buttonHeight = ScaleForDpi(hwnd, 36);
    const int buttonWidth = ScaleForDpi(hwnd, 94);
    const int buttonY = cy - margin - buttonHeight;

    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    if (list)
    {
        const int listBottom = cy - footerHeight;
        MoveWindow(list, margin, headerHeight,
                   max(1, cx - 2 * margin),
                   max(1, listBottom - headerHeight), TRUE);
        ListView_SetColumnWidth(list, 0,
            max(100, cx - 2 * margin - ScaleForDpi(hwnd, 8)));
    }

    int x = margin;
    const int leftIds[] = { IDC_FILTERADD, IDC_FILTEREDIT, IDC_FILTERDEL };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(leftIds)); ++i)
    {
        HWND button = GetDlgItem(hwnd, leftIds[i]);
        if (button)
            MoveWindow(button, x, buttonY, buttonWidth, buttonHeight, TRUE);
        x += buttonWidth + gap;
    }

    x = cx - margin - buttonWidth;
    HWND ok = GetDlgItem(hwnd, IDOK);
    if (ok) MoveWindow(ok, x, buttonY, buttonWidth, buttonHeight, TRUE);
    x -= buttonWidth + gap;

    HWND find = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (find) MoveWindow(find, x, buttonY, buttonWidth, buttonHeight, TRUE);
    x -= buttonWidth + gap;

    HWND options = GetDlgItem(hwnd, IDC_FILTEROPTIONS);
    if (options) MoveWindow(options, x, buttonY, buttonWidth, buttonHeight, TRUE);
}

bool IsModernFilterButton(UINT controlId)
{
    switch (controlId)
    {
        case IDC_FILTERADD:
        case IDC_FILTEREDIT:
        case IDC_FILTERDEL:
        case IDC_FILTEROPTIONS:
        case IDC_FILTERFIND:
        case IDOK:
            return true;
        default:
            return false;
    }
}

void DrawModernFilterButton(const DRAWITEMSTRUCT* item)
{
    if (!item || !item->hwndItem) return;

    RECT rect = item->rcItem;
    HBRUSH clear = CreateSolidBrush(RGB(246, 249, 252));
    FillRect(item->hDC, &rect, clear);
    DeleteObject(clear);

    const bool enabled = (item->itemState & ODS_DISABLED) == 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool primary = item->CtlID == IDC_FILTERADD || item->CtlID == IDOK;

    COLORREF fill = RGB(255, 255, 255);
    COLORREF border = RGB(205, 215, 226);
    COLORREF text = RGB(35, 43, 52);
    if (!enabled)
    {
        fill = RGB(244, 246, 248);
        border = RGB(226, 231, 236);
        text = RGB(150, 157, 165);
    }
    else if (primary)
    {
        fill = pressed ? RGB(0, 95, 184) : RGB(0, 120, 212);
        border = fill;
        text = RGB(255, 255, 255);
    }
    else if (pressed)
    {
        fill = RGB(232, 240, 248);
        border = RGB(179, 201, 224);
    }

    FillRoundedRect(item->hDC, rect, fill, border,
                    ScaleForDpi(item->hwndItem, 10));

    wchar_t label[64] = {};
    GetWindowTextW(item->hwndItem, label, ARRAYSIZE(label));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text);
    HGDIOBJ oldFont = SelectObject(item->hDC,
        primary ? GetHeaderFont() : GetDialogFont());
    DrawTextW(item->hDC, label, -1, &rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(item->hDC, oldFont);

    if (enabled && (item->itemState & ODS_FOCUS))
    {
        RECT focus = rect;
        InflateRect(&focus, -ScaleForDpi(item->hwndItem, 5),
                    -ScaleForDpi(item->hwndItem, 5));
        DrawFocusRect(item->hDC, &focus);
    }
}

void PaintModernFilterDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const COLORREF background = RGB(246, 249, 252);
    const COLORREF foreground = RGB(24, 39, 58);
    const COLORREF secondary = RGB(91, 103, 116);
    const COLORREF divider = RGB(216, 224, 233);
    const int margin = ScaleForDpi(hwnd, 16);
    const int headerHeight = ScaleForDpi(hwnd, 76);
    const int footerHeight = ScaleForDpi(hwnd, 60);

    HBRUSH bg = CreateSolidBrush(background);
    FillRect(hdc, &client, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, foreground);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 13),
                   client.right - margin - ScaleForDpi(hwnd, 130),
                   ScaleForDpi(hwnd, 39) };
    DrawTextW(hdc, L"Filters", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, secondary);
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 40),
                      client.right - margin - ScaleForDpi(hwnd, 130),
                      ScaleForDpi(hwnd, 62) };
    DrawTextW(hdc, L"Manage address and message matching rules.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    wchar_t countText[64] = {};
    swprintf(countText, ARRAYSIZE(countText), L"%u filters",
             static_cast<unsigned int>(Profile.filters.size()));
    RECT badge = { client.right - margin - ScaleForDpi(hwnd, 112),
                   ScaleForDpi(hwnd, 22), client.right - margin,
                   ScaleForDpi(hwnd, 52) };
    FillRoundedRect(hdc, badge, RGB(232, 242, 252), RGB(205, 224, 242),
                    ScaleForDpi(hwnd, 16));
    SetTextColor(hdc, RGB(0, 95, 160));
    oldFont = SelectObject(hdc, GetHeaderFont());
    DrawTextW(hdc, countText, -1, &badge,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, headerHeight - 1,
             client.right - margin, headerHeight - 1, divider);
    DrawLine(hdc, margin, client.bottom - footerHeight,
             client.right - margin, client.bottom - footerHeight, divider);
}

LRESULT CALLBACK FilterWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                          LPARAM lParam, UINT_PTR subclassId,
                                          DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernFilterDialog(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON && IsModernFilterButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_GETMINMAXINFO:
        {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = ScaleForDpi(hwnd, 720);
            info->ptMinTrackSize.y = ScaleForDpi(hwnd, 480);
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

        case WM_SYSCOMMAND:
        {
            const WPARAM command = wParam & 0xFFF0;
            if (command == SC_MAXIMIZE)
            {
                ShowWindow(hwnd, SW_MAXIMIZE);
                return 0;
            }
            if (command == SC_RESTORE)
            {
                ShowWindow(hwnd, SW_RESTORE);
                return 0;
            }
            break;
        }

        case WM_NCLBUTTONDBLCLK:
            if (wParam == HTCAPTION)
            {
                ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                return 0;
            }
            break;

        case WM_SIZE:
        {
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            if (wParam != SIZE_MINIMIZED) LayoutFilterDialog(hwnd);
            return result;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, FilterWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableResizableFilterDialog(HWND hwnd)
{
    if (!IsFilterDialog(hwnd)) return;

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(DS_MODALFRAME);
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowSubclass(hwnd, FilterWindowSubclassProc, kFilterWindowSubclassId, 0);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfo(monitor, &info))
    {
        const int workW = info.rcWork.right - info.rcWork.left;
        const int workH = info.rcWork.bottom - info.rcWork.top;
        int width = workW * 70 / 100;
        int height = workH * 68 / 100;
        if (width < ScaleForDpi(hwnd, 760)) width = ScaleForDpi(hwnd, 760);
        if (height < ScaleForDpi(hwnd, 480)) height = ScaleForDpi(hwnd, 480);
        if (width > workW) width = workW;
        if (height > workH) height = workH;
        const int x = info.rcWork.left + (workW - width) / 2;
        const int y = info.rcWork.top + (workH - height) / 2;
        SetWindowPos(hwnd, NULL, x, y, width, height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    ConfigureModernFilterControls(hwnd);
    LayoutFilterDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsFilterEditDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERTYPE) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERCAPCODE) != NULL &&
           GetDlgItem(hwnd, IDC_FILTER_APPLY) != NULL;
}

bool IsModernFilterEditButton(UINT controlId)
{
    switch (controlId)
    {
        case IDOK:
        case IDCANCEL:
        case IDC_FILTER_APPLY:
        case IDC_FILTER_PREVIOUS:
        case IDC_FILTER_NEXT:
        case IDC_FILTERRESET:
        case IDC_SEPFILTERFILEBROWSE1:
        case IDC_SEPFILTERFILEBROWSE2:
        case IDC_SEPFILTERFILEBROWSE3:
            return true;
        default:
            return false;
    }
}

LRESULT CALLBACK ModernGroupBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                            LPARAM lParam, UINT_PTR subclassId,
                                            DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);

            const COLORREF card = RGB(255, 255, 255);
            const COLORREF border = RGB(212, 221, 231);
            const COLORREF title = RGB(45, 56, 68);
            FillRoundedRect(hdc, client, card, border, ScaleForDpi(hwnd, 10));

            wchar_t label[96] = {};
            GetWindowTextW(hwnd, label, ARRAYSIZE(label));
            if (label[0])
            {
                RECT text = client;
                text.left += ScaleForDpi(hwnd, 12);
                text.right -= ScaleForDpi(hwnd, 12);
                text.top += ScaleForDpi(hwnd, 4);
                text.bottom = text.top + ScaleForDpi(hwnd, 22);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, title);
                HGDIOBJ oldFont = SelectObject(hdc, GetHeaderFont());
                DrawTextW(hdc, label, -1, &text,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernGroupBoxSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void ShiftFilterEditChildren(HWND hwnd, int deltaY)
{
    if (deltaY <= 0) return;

    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        RECT rect = {};
        if (!GetWindowRect(child, &rect)) continue;
        MapWindowPoints(NULL, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        SetWindowPos(child, NULL,
                     rect.left, rect.top + deltaY,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ConfigureModernFilterEditControls(HWND hwnd)
{
    const int actionIds[] = {
        IDOK, IDCANCEL, IDC_FILTER_APPLY, IDC_FILTER_PREVIOUS,
        IDC_FILTER_NEXT, IDC_FILTERRESET,
        IDC_SEPFILTERFILEBROWSE1, IDC_SEPFILTERFILEBROWSE2,
        IDC_SEPFILTERFILEBROWSE3
    };

    for (int i = 0; i < static_cast<int>(ARRAYSIZE(actionIds)); ++i)
    {
        HWND button = GetDlgItem(hwnd, actionIds[i]);
        if (!button) continue;

        LONG_PTR style = GetWindowLongPtr(button, GWL_STYLE);
        style = (style & ~static_cast<LONG_PTR>(0x0F)) | BS_OWNERDRAW;
        SetWindowLongPtr(button, GWL_STYLE, style);
        SetWindowTheme(button, L"", L"");
        SendMessage(button, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }

    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        wchar_t className[32] = {};
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0)
            continue;
        if (lstrcmpiW(className, L"Button") != 0)
            continue;

        const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
        if ((style & 0x0F) == BS_GROUPBOX)
            SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                              kModernGroupBoxSubclassId, 0);
    }

    HWND help = GetDlgItem(hwnd, IDC_FILTEREDITHELP);
    if (help)
    {
        LONG_PTR style = GetWindowLongPtr(help, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(help, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(help, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(help, GWL_EXSTYLE, exStyle);

        SetWindowPos(help, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }
}

int ExpandFilterEditForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
    if (existing)
    {
        const INT_PTR stored = reinterpret_cast<INT_PTR>(existing);
        return stored > 0 ? static_cast<int>(stored - 1) : 0;
    }

    RECT window = {};
    GetWindowRect(hwnd, &window);
    const int currentHeight = window.bottom - window.top;
    const int header = ScaleForDpi(hwnd, 58);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    int applied = 0;

    if (GetMonitorInfo(monitor, &info))
    {
        const int workHeight = info.rcWork.bottom - info.rcWork.top;
        if (currentHeight + header <= workHeight)
        {
            int y = window.top - header / 2;
            if (y < info.rcWork.top) y = info.rcWork.top;
            if (y + currentHeight + header > info.rcWork.bottom)
                y = info.rcWork.bottom - currentHeight - header;

            SetWindowPos(hwnd, NULL, window.left, y,
                         window.right - window.left, currentHeight + header,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            ShiftFilterEditChildren(hwnd, header);
            applied = header;
        }
    }

    SetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int FilterEditHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernFilterEditDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HBRUSH bg = GetDialogSurfaceBrush();
    FillRect(hdc, &client, bg);

    const int header = FilterEditHeaderOffset(hwnd);
    if (header <= 0) return;

    wchar_t caption[128] = {};
    GetWindowTextW(hwnd, caption, ARRAYSIZE(caption));
    const wchar_t* title =
        wcsstr(caption, L"Add") ? L"Add filter" :
        (wcsstr(caption, L"multiple") ? L"Edit selected filters" : L"Edit filter");

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT titleRect = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, title, -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc, L"Matching, notification and output settings.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK FilterEditWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                              LPARAM lParam, UINT_PTR subclassId,
                                              DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernFilterEditDialog(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(40, 48, 58));
            return reinterpret_cast<LRESULT>(GetDialogSurfaceBrush());
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                IsModernFilterEditButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
            RemoveWindowSubclass(hwnd, FilterEditWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterEditDialog(HWND hwnd)
{
    if (!IsFilterEditDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterEditWindowSubclassProc,
                      kFilterEditWindowSubclassId, 0);
    ExpandFilterEditForHeader(hwnd);
    ConfigureModernFilterEditControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
{
    HFONT font = reinterpret_cast<HFONT>(fontParam);
    if (font) SendMessage(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SetWindowTheme(child, L"Explorer", NULL);
    return TRUE;
}

LRESULT CALLBACK MainWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR subclassId,
                                        DWORD_PTR referenceData)
{
    if (message == WM_GETMINMAXINFO)
    {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const int minWidth = ScaleForDpi(hwnd, 900);
        const int minHeight = ScaleForDpi(hwnd, 600);
        if (info->ptMinTrackSize.x < minWidth) info->ptMinTrackSize.x = minWidth;
        if (info->ptMinTrackSize.y < minHeight) info->ptMinTrackSize.y = minHeight;
        return result;
    }

    if (message == WM_LBUTTONDOWN)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (point.y < ShellHeight(hwnd))
        {
            const int target = HitTestShell(hwnd, point);
            if (target >= 0)
            {
                if (target == 5 && g_settingsFlyout)
                {
                    CloseSettingsFlyout();
                    g_pressedTarget = -1;
                    DrawModernWorkspace(hwnd);
                    return 0;
                }
                if (g_settingsFlyout) CloseSettingsFlyout();
                g_pressedTarget = target;
                SetCapture(hwnd);
                DrawModernWorkspace(hwnd);
                return 0;
            }
        }
    }

    if (message == WM_LBUTTONUP && g_pressedTarget >= 0)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int pressed = g_pressedTarget;
        const int target = HitTestShell(hwnd, point);
        g_pressedTarget = -1;
        if (GetCapture() == hwnd) ReleaseCapture();
        DrawModernWorkspace(hwnd);
        if (target == pressed)
            DispatchShellCommand(hwnd, CommandForTarget(pressed));
        return 0;
    }

    if (message == WM_MOUSEMOVE)
    {
        POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int hover = point.y < ShellHeight(hwnd) ? HitTestShell(hwnd, point) : -1;
        if (hover != g_hoverTarget)
        {
            g_hoverTarget = hover;
            DrawModernWorkspace(hwnd);
        }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
    }

    if (message == WM_MOUSELEAVE)
    {
        g_hoverTarget = -1;
        DrawModernWorkspace(hwnd);
    }

    if (message == WM_KEYDOWN && wParam == VK_ESCAPE && g_settingsFlyout)
    {
        CloseSettingsFlyout();
        DrawModernWorkspace(hwnd);
        return 0;
    }

    if (message == WM_SETCURSOR)
    {
        POINT screen = {};
        GetCursorPos(&screen);
        POINT client = screen;
        ScreenToClient(hwnd, &client);
        if (client.y >= 0 && client.y < ShellHeight(hwnd) &&
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
            DetachLegacyMenu(hwnd);
            HideLegacyToolbar(hwnd);
            LayoutModernWorkspace(hwnd);
            DrawModernWorkspace(hwnd);
            break;

        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED)
            {
                HideLegacyToolbar(hwnd);
                LayoutModernWorkspace(hwnd);
                DrawModernWorkspace(hwnd);
            }
            break;

        case WM_PAINT:
        case WM_NOTIFY:
            DrawModernWorkspace(hwnd);
            break;

        case WM_TIMER:
            if (wParam == kLegacySecondTimer) DrawModernWorkspace(hwnd);
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && g_settingsFlyout)
            {
                HWND next = reinterpret_cast<HWND>(lParam);
                if (next != g_settingsFlyout) CloseSettingsFlyout();
            }
            break;

        case WM_NCDESTROY:
            CloseSettingsFlyout();
            RemoveWindowSubclass(hwnd, MainWindowSubclassProc, subclassId);
            if (g_mainWindow == hwnd) g_mainWindow = NULL;
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

    g_mainWindow = hwnd;
    g_chromeEnabled = true;
    if (!ghMenu) ghMenu = GetMenu(hwnd);
    ApplyRoundedCorners(hwnd);

    const BOOL dark = FALSE;
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
    if (IsFilterDialog(hwnd)) EnableResizableFilterDialog(hwnd);
    if (IsFilterEditDialog(hwnd)) EnableModernFilterEditDialog(hwnd);
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
