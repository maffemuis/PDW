#include "windows11_ui.h"
#include "ui_theme.h"

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
extern int si_index;
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
const UINT_PTR kFilterOptionsWindowSubclassId = 0x50445734;
const UINT_PTR kFilterFindWindowSubclassId = 0x50445735;
const UINT_PTR kFilterDuplicateWindowSubclassId = 0x50445736;
const UINT_PTR kOptionsWindowSubclassId = 0x50445737;
const UINT_PTR kGeneralOptionsWindowSubclassId = 0x50445738;
const UINT_PTR kScreenOptionsWindowSubclassId = 0x50445739;
const UINT_PTR kScrollbackWindowSubclassId = 0x5044573A;
const UINT_PTR kSystemTrayWindowSubclassId = 0x5044573B;
const UINT_PTR kInterfaceSetupWindowSubclassId = 0x5044573C;
const UINT_PTR kLogfileWindowSubclassId = 0x5044573D;
const UINT_PTR kCustomAudioWindowSubclassId = 0x5044573E;
const UINT_PTR kStatisticsWindowSubclassId = 0x5044573F;
const UINT_PTR kColorsWindowSubclassId = 0x50445740;
const UINT kEnableModernShellMessage = WM_APP + 0x51;
// Legacy timer IDs are private to PDW.cpp; mirror only the two main-window
// timers the modern shell observes. PDW_TIMER=101, SECOND_TIMER=103.
const WPARAM kLegacyDecodeTimer = 101;
const WPARAM kLegacySecondTimer = 103;
const int kSettingsPopupCommand = 50001;
const int kResumeMonitorCommand = 50002;
const int kToggleThemeCommand = 50003;

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
    { L"\xE706", L"Theme", kToggleThemeCommand },
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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF accent = palette.accent;
    const COLORREF accentPressed = palette.accentPressed;
    const COLORREF normalBg = palette.controlBackground;
    const COLORREF hoverBg = palette.controlHover;
    const COLORREF fg = palette.textPrimary;
    const COLORREF selectedFg = palette.selectionText;

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
        border = palette.border;
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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF shellBg = palette.shellBackground;
    const COLORREF capsuleBg = palette.controlBackground;
    const COLORREF capsuleBorder = palette.border;

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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF rowBg = palette.controlBackground;
    const COLORREF rowBorder = palette.border;
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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF background = palette.cardHeaderBackground;
    const COLORREF foreground = palette.textSecondary;
    const COLORREF divider = palette.divider;

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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF cardBg = palette.cardBackground;
    const COLORREF cardBorder = palette.border;
    const COLORREF titleBg = palette.cardHeaderBackground;
    const COLORREF titleFg = palette.textPrimary;
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
        // si_index is PDW's original live signal-indicator state (0..20).
        // It follows the raw receive transitions and therefore moves while
        // listening and swings on a received signal. dRX_Quality is a separate
        // decode-quality metric and is shown only as the percentage.
        int signal = si_index;
        if (signal < 0) signal = 0;
        if (signal > 20) signal = 20;

        double quality = dRX_Quality;
        if (quality < 0.0) quality = 0.0;
        if (quality > 100.0) quality = 100.0;

        const int cy = titleRect.top + (titleHeight / 2);
        const int meterWidth = ScaleForDpi(hwnd, 104);
        const int meterHeight = ScaleForDpi(hwnd, 10);
        const int meterRight = card.right - ScaleForDpi(hwnd, 76);
        RECT meter = {
            meterRight - meterWidth,
            cy - meterHeight / 2,
            meterRight,
            cy + meterHeight / 2
        };
        FillRoundedRect(hdc, meter, palette.controlBackground, palette.border,
                        ScaleForDpi(hwnd, 8));

        const int innerPad = ScaleForDpi(hwnd, 2);
        RECT inner = {
            meter.left + innerPad,
            meter.top + innerPad,
            meter.right - innerPad,
            meter.bottom - innerPad
        };
        const int innerWidth = inner.right - inner.left;
        const int liveWidth = (innerWidth * signal) / 20;
        if (liveWidth > 0)
        {
            RECT live = inner;
            live.right = live.left + liveWidth;
            const COLORREF signalColor = signal >= 15
                ? palette.signalHigh
                : (signal >= 8 ? palette.signalMid : palette.signalLow);
            FillRoundedRect(hdc, live, signalColor, signalColor,
                            ScaleForDpi(hwnd, 6));
        }

        // A small marker makes low-level/noise movement visible even when only
        // one or two of the old signal steps are active.
        const int markerX = inner.left + (innerWidth * signal) / 20;
        HPEN markerPen = CreatePen(PS_SOLID, ScaleForDpi(hwnd, 2), palette.textSecondary);
        HGDIOBJ oldPen = SelectObject(hdc, markerPen);
        MoveToEx(hdc, markerX, meter.top - ScaleForDpi(hwnd, 2), NULL);
        LineTo(hdc, markerX, meter.bottom + ScaleForDpi(hwnd, 2));
        SelectObject(hdc, oldPen);
        DeleteObject(markerPen);

        wchar_t qualityText[32] = {};
        if (quality > 0.0)
            swprintf(qualityText, ARRAYSIZE(qualityText), L"%.1f%%", quality);
        else
            lstrcpyW(qualityText, L"--.-%");

        SetTextColor(hdc, quality > 0.0 && quality < 90.0
                     ? palette.warning : titleFg);
        oldFont = SelectObject(hdc, GetHeaderFont());
        RECT rxText = {
            meter.right + ScaleForDpi(hwnd, 8), titleRect.top,
            card.right - ScaleForDpi(hwnd, 14), titleRect.bottom
        };
        DrawTextW(hdc, qualityText, -1, &rxText,
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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF bg = palette.shellBackground;
    const COLORREF line = palette.divider;
    const COLORREF fg = palette.textSecondary;
    const COLORREF green = palette.success;
    const COLORREF paused = palette.warning;
    const COLORREF gray = palette.textMuted;

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
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC target = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_CLIPCHILDREN);
    if (!target) target = GetDC(hwnd);
    if (!target) return;

    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;
    HDC hdc = (buffer && bitmap) ? buffer : target;

    const COLORREF workspace = pdw::CurrentThemePalette().workspaceBackground;
    HBRUSH background = CreateSolidBrush(workspace);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    DrawTopNavigation(hdc, hwnd, client);
    DrawCommandStrip(hdc, hwnd, client);
    DrawCard(hdc, hwnd, g_pane1Card, g_pane1Body, L"Monitored messages", true);
    DrawCard(hdc, hwnd, g_pane2Card, g_pane2Body, L"Filtered messages", false);
    DrawStatusBar(hdc, hwnd);

    if (buffer && bitmap)
    {
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    }
    else if (buffer)
    {
        DeleteDC(buffer);
    }

    ReleaseDC(hwnd, target);
}

void ApplyMainDwmTheme(HWND hwnd)
{
    if (!hwnd) return;
    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
}

bool TogglePersistedUiTheme(HWND hwnd)
{
    const pdw::UiTheme next = pdw::CurrentUiTheme() == pdw::UiTheme::Dark
        ? pdw::UiTheme::Light
        : pdw::UiTheme::Dark;
    if (!pdw::SaveUiThemeSetting(next, szShortAppName, szIniPathName))
        return false;

    ApplyMainDwmTheme(hwnd);
    if (Pane1.hWnd) InvalidateRect(Pane1.hWnd, NULL, TRUE);
    if (Pane2.hWnd) InvalidateRect(Pane2.hWnd, NULL, TRUE);
    DrawModernWorkspace(hwnd);
    return true;
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
                if (owner && command == kToggleThemeCommand)
                    TogglePersistedUiTheme(owner);
                else if (owner && command)
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
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            HBRUSH bg = CreateSolidBrush(palette.cardBackground);
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
                    FillRoundedRect(hdc, row, palette.controlHover, palette.controlHover,
                                    ScaleForDpi(hwnd, 8));

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, palette.accent);
                HGDIOBJ oldFont = SelectObject(hdc, GetIconFont());
                RECT iconRect = row;
                iconRect.left += ScaleForDpi(hwnd, 10);
                iconRect.right = iconRect.left + ScaleForDpi(hwnd, 24);
                DrawTextW(hdc, g_flyoutItems[i].icon, -1, &iconRect,
                          DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                SelectObject(hdc, oldFont);

                SetTextColor(hdc, palette.textPrimary);
                oldFont = SelectObject(hdc, GetDialogFont());
                RECT text = row;
                text.left += ScaleForDpi(hwnd, 46);
                const wchar_t* label = g_flyoutItems[i].label;
                if (g_flyoutItems[i].command == kToggleThemeCommand)
                    label = pdw::CurrentUiTheme() == pdw::UiTheme::Dark
                        ? L"Theme: Dark" : L"Theme: Light";
                DrawTextW(hdc, label, -1, &text,
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

COLORREF EnsureFilterListContrast(COLORREF color)
{
    const int red = GetRValue(color);
    const int green = GetGValue(color);
    const int blue = GetBValue(color);
    const int luminance = (red * 299 + green * 587 + blue * 114) / 1000;
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();

    if (pdw::CurrentUiTheme() == pdw::UiTheme::Dark)
    {
        if (luminance < 72) return palette.textSecondary;
        return color;
    }
    if (luminance > 218) return palette.textSecondary;
    return color;
}

void DrawModernFilterListItem(const DRAWITEMSTRUCT* item)
{
    if (!item || !item->hwndItem ||
        item->itemID == static_cast<UINT>(-1) ||
        item->itemID >= Profile.filters.size())
        return;

    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const bool selected =
        (ListView_GetItemState(item->hwndItem, static_cast<int>(item->itemID),
                               LVIS_SELECTED) & LVIS_SELECTED) != 0;

    RECT row = item->rcItem;
    HBRUSH background = CreateSolidBrush(
        selected ? palette.selectionBackground : palette.cardBackground);
    FillRect(item->hDC, &row, background);
    DeleteObject(background);

    int labelColor = Profile.filters[item->itemID].label_color;
    if (labelColor < 0 || labelColor > 16) labelColor = 0;

    COLORREF textColor = palette.textPrimary;
    if (selected)
        textColor = palette.selectionText;
    else if (!Profile.filters[item->itemID].label_enabled)
        textColor = palette.textMuted;
    else
        textColor = EnsureFilterListContrast(Profile.color_filterlabel[labelColor]);

    char filterText[MAX_STR_LEN] = {};
    BuildFilterString(filterText, Profile.filters[item->itemID]);

    RECT textRect = row;
    textRect.left += ScaleForDpi(item->hwndItem, 8);
    textRect.right -= ScaleForDpi(item->hwndItem, 8);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, textColor);
    HGDIOBJ oldFont = SelectObject(item->hDC, GetFilterListFont());
    DrawTextA(item->hDC, filterText, -1, &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(item->hDC, oldFont);

    if ((item->itemState & ODS_FOCUS) != 0)
    {
        RECT focus = row;
        InflateRect(&focus, -ScaleForDpi(item->hwndItem, 2),
                    -ScaleForDpi(item->hwndItem, 2));
        DrawFocusRect(item->hDC, &focus);
    }
}

void ConfigureModernFilterControls(HWND hwnd)
{
    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        wchar_t className[32] = {};
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
            lstrcmpiW(className, L"Static") != 0)
            continue;
        RECT rect = {};
        GetWindowRect(child, &rect);
        MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        if (rect.top < ScaleForDpi(hwnd, 70))
            ShowWindow(child, SW_HIDE);
    }
    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    if (list)
    {
        LONG_PTR style = GetWindowLongPtr(list, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(list, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(list, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(list, GWL_EXSTYLE, exStyle);

        SetWindowTheme(list,
                       pdw::CurrentUiTheme() == pdw::UiTheme::Dark
                           ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        ListView_SetExtendedListViewStyleEx(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessage(list, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);
        const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
        ListView_SetBkColor(list, palette.cardBackground);
        ListView_SetTextBkColor(list, palette.cardBackground);
        ListView_SetTextColor(list, palette.textPrimary);
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

    const bool mainFilter = IsFilterDialog(GetParent(item->hwndItem));
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();

    RECT rect = item->rcItem;
    const COLORREF clearColor = mainFilter
        ? palette.windowBackground : RGB(246, 249, 252);
    HBRUSH clear = CreateSolidBrush(clearColor);
    FillRect(item->hDC, &rect, clear);
    DeleteObject(clear);

    const bool enabled = (item->itemState & ODS_DISABLED) == 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool primary = item->CtlID == IDC_FILTERADD || item->CtlID == IDOK;

    COLORREF fill = mainFilter ? palette.controlBackground : RGB(255, 255, 255);
    COLORREF border = mainFilter ? palette.border : RGB(205, 215, 226);
    COLORREF textColor = mainFilter ? palette.textPrimary : RGB(35, 43, 52);
    if (!enabled)
    {
        fill = mainFilter ? palette.controlBackground : RGB(244, 246, 248);
        border = mainFilter ? palette.divider : RGB(226, 231, 236);
        textColor = mainFilter ? palette.textMuted : RGB(150, 157, 165);
    }
    else if (primary)
    {
        fill = mainFilter
            ? (pressed ? palette.accentPressed : palette.accent)
            : (pressed ? RGB(0, 95, 184) : RGB(0, 120, 212));
        border = fill;
        textColor = mainFilter ? palette.selectionText : RGB(255, 255, 255);
    }
    else if (pressed)
    {
        fill = mainFilter ? palette.controlHover : RGB(232, 240, 248);
        border = mainFilter ? palette.accent : RGB(179, 201, 224);
    }

    FillRoundedRect(item->hDC, rect, fill, border,
                    ScaleForDpi(item->hwndItem, 10));

    wchar_t label[64] = {};
    GetWindowTextW(item->hwndItem, label, ARRAYSIZE(label));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, textColor);
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
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    const COLORREF background = palette.windowBackground;
    const COLORREF foreground = palette.textPrimary;
    const COLORREF secondary = palette.textSecondary;
    const COLORREF divider = palette.divider;
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
    DrawTextW(hdc, L"Beheer adressen en regels voor berichtovereenkomsten.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    wchar_t countText[64] = {};
    swprintf(countText, ARRAYSIZE(countText), L"%u filters",
             static_cast<unsigned int>(Profile.filters.size()));
    RECT badge = { client.right - margin - ScaleForDpi(hwnd, 112),
                   ScaleForDpi(hwnd, 22), client.right - margin,
                   ScaleForDpi(hwnd, 52) };
    FillRoundedRect(hdc, badge, palette.controlBackground, palette.border,
                    ScaleForDpi(hwnd, 16));
    SetTextColor(hdc, palette.accent);
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
            if (item && item->CtlID == IDC_FILTERS)
            {
                DrawModernFilterListItem(item);
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

    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));

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

bool IsFilterOptionsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERFILEEN) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERCMDEN) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERDEFTYPE) != NULL;
}

bool IsModernFilterOptionsButton(UINT controlId)
{
    switch (controlId)
    {
        case IDOK:
        case IDCANCEL:
        case IDC_FILTERBROWSE:
        case IDC_FILTERCMDBROWSE:
            return true;
        default:
            return false;
    }
}

void ConfigureModernFilterOptionsControls(HWND hwnd)
{
    const int actionIds[] = {
        IDOK, IDCANCEL, IDC_FILTERBROWSE, IDC_FILTERCMDBROWSE
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
}

int ExpandFilterOptionsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.FilterOptions.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.FilterOptions.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int FilterOptionsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.FilterOptions.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernFilterOptionsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = FilterOptionsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT titleRect = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Filter options", -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc, L"Output files, descriptions and default filter behavior.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK FilterOptionsWindowSubclassProc(HWND hwnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
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
            PaintModernFilterOptionsDialog(hwnd, hdc);
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
                IsModernFilterOptionsButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.FilterOptions.HeaderOffset");
            RemoveWindowSubclass(hwnd, FilterOptionsWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterOptionsDialog(HWND hwnd)
{
    if (!IsFilterOptionsDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterOptionsWindowSubclassProc,
                      kFilterOptionsWindowSubclassId, 0);
    ExpandFilterOptionsForHeader(hwnd);
    ConfigureModernFilterOptionsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsFilterFindDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERFIND) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERFIND_HITS) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERFIND_CASE) != NULL;
}

BOOL CALLBACK HideLegacyFilterFindLabels(HWND child, LPARAM)
{
    wchar_t className[32] = {};
    if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpiW(className, L"Static") != 0)
        return TRUE;

    wchar_t label[64] = {};
    GetWindowTextW(child, label, ARRAYSIZE(label));
    if (lstrcmpiW(label, L"Find :") == 0 || lstrcmpiW(label, L"Hits :") == 0)
        ShowWindow(child, SW_HIDE);
    return TRUE;
}

void LayoutModernFilterFindDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int editHeight = ScaleForDpi(hwnd, 30);
    const int rowTop = header + ScaleForDpi(hwnd, 28);

    HWND edit = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (edit)
        MoveWindow(edit, margin, rowTop,
                   max(1, client.right - 2 * margin), editHeight, TRUE);

    HWND hits = GetDlgItem(hwnd, IDC_FILTERFIND_HITS);
    if (hits)
        MoveWindow(hits, margin + ScaleForDpi(hwnd, 58),
                   rowTop + ScaleForDpi(hwnd, 45),
                   ScaleForDpi(hwnd, 80), ScaleForDpi(hwnd, 22), TRUE);

    HWND caseSensitive = GetDlgItem(hwnd, IDC_FILTERFIND_CASE);
    if (caseSensitive)
        MoveWindow(caseSensitive,
                   margin + ScaleForDpi(hwnd, 155),
                   rowTop + ScaleForDpi(hwnd, 44),
                   ScaleForDpi(hwnd, 125), ScaleForDpi(hwnd, 24), TRUE);

    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close)
        MoveWindow(close,
                   client.right - margin - ScaleForDpi(hwnd, 92),
                   client.bottom - margin - ScaleForDpi(hwnd, 36),
                   ScaleForDpi(hwnd, 92), ScaleForDpi(hwnd, 36), TRUE);
}

void ConfigureModernFilterFindControls(HWND hwnd)
{
    EnumChildWindows(hwnd, HideLegacyFilterFindLabels, 0);

    HWND edit = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (edit)
    {
        LONG_PTR style = GetWindowLongPtr(edit, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(edit, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(edit, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(edit, GWL_EXSTYLE, exStyle);
        SetWindowTheme(edit, L"Explorer", NULL);
        SendMessage(edit, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(edit, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close)
    {
        LONG_PTR style = GetWindowLongPtr(close, GWL_STYLE);
        style = (style & ~static_cast<LONG_PTR>(0x0F)) | BS_OWNERDRAW;
        SetWindowLongPtr(close, GWL_STYLE, style);
        SetWindowTheme(close, L"", L"");
        SendMessage(close, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }
}

void ResizeModernFilterFindDialog(HWND hwnd)
{
    const int width = ScaleForDpi(hwnd, 430);
    const int height = ScaleForDpi(hwnd, 220);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return;

    const int workW = info.rcWork.right - info.rcWork.left;
    const int workH = info.rcWork.bottom - info.rcWork.top;
    const int actualW = min(width, workW);
    const int actualH = min(height, workH);
    const int x = info.rcWork.left + (workW - actualW) / 2;
    const int y = info.rcWork.top + (workH - actualH) / 2;

    SetWindowPos(hwnd, NULL, x, y, actualW, actualH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void PaintModernFilterFindDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int editTop = header + ScaleForDpi(hwnd, 28);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Find filter", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc, L"Search address, message text or label.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             RGB(216, 224, 233));

    SetTextColor(hdc, RGB(45, 56, 68));
    oldFont = SelectObject(hdc, GetHeaderFont());
    RECT findLabel = { margin, header + ScaleForDpi(hwnd, 5),
                       client.right - margin, editTop - ScaleForDpi(hwnd, 3) };
    DrawTextW(hdc, L"Search", -1, &findLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT hitsLabel = { margin,
                       editTop + ScaleForDpi(hwnd, 44),
                       margin + ScaleForDpi(hwnd, 55),
                       editTop + ScaleForDpi(hwnd, 68) };
    DrawTextW(hdc, L"Hits", -1, &hitsLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    RECT editCard = { margin - 1, editTop - 1,
                      client.right - margin + 1,
                      editTop + ScaleForDpi(hwnd, 31) };
    FillRoundedRect(hdc, editCard, RGB(255, 255, 255), RGB(196, 208, 220),
                    ScaleForDpi(hwnd, 8));
}

LRESULT CALLBACK FilterFindWindowSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId,
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
            PaintModernFilterFindDialog(hwnd, hdc);
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

        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON && item->CtlID == IDCANCEL)
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, FilterFindWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterFindDialog(HWND hwnd)
{
    if (!IsFilterFindDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterFindWindowSubclassProc,
                      kFilterFindWindowSubclassId, 0);
    ResizeModernFilterFindDialog(hwnd);
    ConfigureModernFilterFindControls(hwnd);
    LayoutModernFilterFindDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsFilterDuplicateDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE) != NULL &&
           GetDlgItem(hwnd, IDC_PROGRESS1) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERDUP_PCT) != NULL;
}

void ResizeModernFilterDuplicateDialog(HWND hwnd)
{
    const int width = ScaleForDpi(hwnd, 620);
    const int height = ScaleForDpi(hwnd, 280);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return;

    const int workW = info.rcWork.right - info.rcWork.left;
    const int workH = info.rcWork.bottom - info.rcWork.top;
    const int actualW = min(width, workW);
    const int actualH = min(height, workH);
    const int x = info.rcWork.left + (workW - actualW) / 2;
    const int y = info.rcWork.top + (workH - actualH) / 2;

    SetWindowPos(hwnd, NULL, x, y, actualW, actualH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void LayoutModernFilterDuplicateDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int listTop = header + ScaleForDpi(hwnd, 18);
    const int listHeight = ScaleForDpi(hwnd, 88);
    const int progressTop = listTop + listHeight + ScaleForDpi(hwnd, 18);
    const int buttonHeight = ScaleForDpi(hwnd, 36);
    const int buttonWidth = ScaleForDpi(hwnd, 112);
    const int buttonGap = ScaleForDpi(hwnd, 10);
    const int bottom = client.bottom - margin;

    HWND results = GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE);
    if (results)
        MoveWindow(results, margin, listTop,
                   max(1, client.right - 2 * margin), listHeight, TRUE);

    HWND progress = GetDlgItem(hwnd, IDC_PROGRESS1);
    if (progress)
        MoveWindow(progress, margin, progressTop,
                   max(1, client.right - 2 * margin - ScaleForDpi(hwnd, 62)),
                   ScaleForDpi(hwnd, 14), TRUE);

    HWND pct = GetDlgItem(hwnd, IDC_FILTERDUP_PCT);
    if (pct)
        MoveWindow(pct,
                   client.right - margin - ScaleForDpi(hwnd, 52),
                   progressTop - ScaleForDpi(hwnd, 4),
                   ScaleForDpi(hwnd, 52), ScaleForDpi(hwnd, 24), TRUE);

    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close)
        MoveWindow(close,
                   client.right - margin - buttonWidth,
                   bottom - buttonHeight,
                   buttonWidth, buttonHeight, TRUE);

    HWND find = GetDlgItem(hwnd, IDOK);
    if (find)
        MoveWindow(find,
                   client.right - margin - 2 * buttonWidth - buttonGap,
                   bottom - buttonHeight,
                   buttonWidth, buttonHeight, TRUE);
}

void ConfigureModernFilterDuplicateControls(HWND hwnd)
{
    HWND results = GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE);
    if (results)
    {
        LONG_PTR style = GetWindowLongPtr(results, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(results, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(results, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(results, GWL_EXSTYLE, exStyle);
        SetWindowTheme(results, L"Explorer", NULL);
        SendMessage(results, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(results, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    HWND progress = GetDlgItem(hwnd, IDC_PROGRESS1);
    if (progress)
    {
        SetWindowTheme(progress, L"Explorer", NULL);
        SendMessage(progress, PBM_SETBKCOLOR, 0,
                    static_cast<LPARAM>(RGB(230, 235, 241)));
        SendMessage(progress, PBM_SETBARCOLOR, 0,
                    static_cast<LPARAM>(RGB(0, 103, 192)));
    }

    const int actionIds[] = { IDOK, IDCANCEL };
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

    HWND find = GetDlgItem(hwnd, IDOK);
    if (find) SetWindowTextW(find, L"Find duplicates");
    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close) SetWindowTextW(close, L"Close");
}

void PaintModernFilterDuplicateDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int listTop = header + ScaleForDpi(hwnd, 18);
    const int listHeight = ScaleForDpi(hwnd, 88);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Check duplicate filters", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc, L"Find filters with the same type, address and message text.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             RGB(216, 224, 233));

    RECT resultsCard = {
        margin - 1,
        listTop - 1,
        client.right - margin + 1,
        listTop + listHeight + 1
    };
    FillRoundedRect(hdc, resultsCard, RGB(255, 255, 255), RGB(206, 216, 227),
                    ScaleForDpi(hwnd, 8));
}

LRESULT CALLBACK FilterDuplicateWindowSubclassProc(HWND hwnd, UINT message,
                                                   WPARAM wParam, LPARAM lParam,
                                                   UINT_PTR subclassId,
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
            PaintModernFilterDuplicateDialog(hwnd, hdc);
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

        case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH listBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(listBrush);
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                                 subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterDuplicateDialog(HWND hwnd)
{
    if (!IsFilterDuplicateDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                      kFilterDuplicateWindowSubclassId, 0);
    ResizeModernFilterDuplicateDialog(hwnd);
    ConfigureModernFilterDuplicateControls(hwnd);
    LayoutModernFilterDuplicateDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsOptionsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_DECODEPOCSAG) != NULL &&
           GetDlgItem(hwnd, IDC_DECODEFLEX) != NULL &&
           GetDlgItem(hwnd, IDC_MB_BITSYNC) != NULL &&
           GetDlgItem(hwnd, IDC_ACARS_PC_YES) != NULL &&
           GetDlgItem(hwnd, IDC_GENERALOPTIONS) != NULL;
}

bool IsModernOptionsButton(UINT controlId)
{
    return controlId == IDOK || controlId == IDCANCEL ||
           controlId == IDC_GENERALOPTIONS;
}

void ConfigureModernOptionsControls(HWND hwnd)
{
    const int actionIds[] = { IDOK, IDCANCEL, IDC_GENERALOPTIONS };
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
}

int ExpandOptionsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Options.HeaderOffset");
    if (existing)
    {
        const INT_PTR stored = reinterpret_cast<INT_PTR>(existing);
        return stored > 0 ? static_cast<int>(stored - 1) : 0;
    }

    RECT window = {};
    GetWindowRect(hwnd, &window);
    const int currentHeight = window.bottom - window.top;
    const int header = ScaleForDpi(hwnd, 60);

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

    SetPropW(hwnd, L"PDW.Options.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int OptionsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Options.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernOptionsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = OptionsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 32)
    };
    DrawTextW(hdc, L"Decoder options", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 32),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Protocol decoding, message handling and title-bar information.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK OptionsWindowSubclassProc(HWND hwnd, UINT message,
                                           WPARAM wParam, LPARAM lParam,
                                           UINT_PTR subclassId,
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
            PaintModernOptionsDialog(hwnd, hdc);
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
                IsModernOptionsButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Options.HeaderOffset");
            RemoveWindowSubclass(hwnd, OptionsWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernOptionsDialog(HWND hwnd)
{
    if (!IsOptionsDialog(hwnd)) return;

    SetWindowSubclass(hwnd, OptionsWindowSubclassProc,
                      kOptionsWindowSubclassId, 0);
    ExpandOptionsForHeader(hwnd);
    ConfigureModernOptionsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsGeneralOptionsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_BLOCKDUPLICATE) != NULL &&
           GetDlgItem(hwnd, IDC_DATEFORMAT) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILEPATH) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILEPATHBROWSE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILEPATHDEFAULT) != NULL;
}

bool IsModernGeneralOptionsButton(UINT controlId)
{
    return controlId == IDOK || controlId == IDCANCEL ||
           controlId == IDC_LOGFILEPATHBROWSE ||
           controlId == IDC_LOGFILEPATHDEFAULT;
}

void ConfigureModernGeneralOptionsControls(HWND hwnd)
{
    const int actionIds[] = {
        IDOK, IDCANCEL, IDC_LOGFILEPATHBROWSE, IDC_LOGFILEPATHDEFAULT
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

    HWND path = GetDlgItem(hwnd, IDC_LOGFILEPATH);
    if (path)
    {
        LONG_PTR exStyle = GetWindowLongPtr(path, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(path, GWL_EXSTYLE, exStyle);
        SetWindowTheme(path, L"Explorer", NULL);
        SetWindowPos(path, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }
}

int ExpandGeneralOptionsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.GeneralOptions.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.GeneralOptions.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int GeneralOptionsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.GeneralOptions.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernGeneralOptionsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = GeneralOptionsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"General options", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc, L"Duplicate handling, dates, exit behavior and logfile location.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK GeneralOptionsWindowSubclassProc(HWND hwnd, UINT message,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
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
            PaintModernGeneralOptionsDialog(hwnd, hdc);
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
                IsModernGeneralOptionsButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.GeneralOptions.HeaderOffset");
            RemoveWindowSubclass(hwnd, GeneralOptionsWindowSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernGeneralOptionsDialog(HWND hwnd)
{
    if (!IsGeneralOptionsDialog(hwnd)) return;
    SetWindowSubclass(hwnd, GeneralOptionsWindowSubclassProc,
                      kGeneralOptionsWindowSubclassId, 0);
    ExpandGeneralOptionsForHeader(hwnd);
    ConfigureModernGeneralOptionsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsScreenOptionsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, 801) != NULL &&
           GetDlgItem(hwnd, 807) != NULL &&
           GetDlgItem(hwnd, IDC_FLEXGROUPMODE) != NULL &&
           GetDlgItem(hwnd, IDC_FGM_LOGGING) != NULL &&
           GetDlgItem(hwnd, IDC_FGM_COMBINE) != NULL;
}

void ConfigureModernScreenOptionsControls(HWND hwnd)
{
    const int actionIds[] = { IDOK, IDCANCEL };
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
}

int ExpandScreenOptionsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.ScreenOptions.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.ScreenOptions.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int ScreenOptionsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.ScreenOptions.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernScreenOptionsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = ScreenOptionsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Screen options", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc, L"Choose visible columns and FLEX group display behavior.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK ScreenOptionsWindowSubclassProc(HWND hwnd, UINT message,
                                                 WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR subclassId,
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
            PaintModernScreenOptionsDialog(hwnd, hdc);
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
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.ScreenOptions.HeaderOffset");
            RemoveWindowSubclass(hwnd, ScreenOptionsWindowSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernScreenOptionsDialog(HWND hwnd)
{
    if (!IsScreenOptionsDialog(hwnd)) return;
    SetWindowSubclass(hwnd, ScreenOptionsWindowSubclassProc,
                      kScreenOptionsWindowSubclassId, 0);
    ExpandScreenOptionsForHeader(hwnd);
    ConfigureModernScreenOptionsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsScrollbackDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_SCROLLPANE1) != NULL &&
           GetDlgItem(hwnd, IDC_SCROLLPANE2) != NULL &&
           GetDlgItem(hwnd, IDC_SCROLLSPEED) != NULL &&
           GetDlgItem(hwnd, IDC_PERCENTPANE1) != NULL &&
           GetDlgItem(hwnd, IDC_PERCENTPANE2) != NULL;
}

void FlattenModernScrollbackField(HWND child)
{
    if (!child) return;

    LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtr(child, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(child, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(child, GWL_EXSTYLE, exStyle);

    SetWindowTheme(child, L"Explorer", NULL);
    SendMessage(child, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

void ConfigureModernScrollbackControls(HWND hwnd)
{
    const int fieldIds[] = {
        IDC_SCROLLPANE1, IDC_SCROLLPANE2,
        IDC_PERCENTPANE1, IDC_PERCENTPANE2
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(fieldIds)); ++i)
        FlattenModernScrollbackField(GetDlgItem(hwnd, fieldIds[i]));

    HWND speed = GetDlgItem(hwnd, IDC_SCROLLSPEED);
    if (speed)
    {
        SetWindowTheme(speed, L"Explorer", NULL);
        SendMessage(speed, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }

    const int actionIds[] = { IDOK, IDCANCEL };
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
}

int ExpandScrollbackForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Scrollback.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.Scrollback.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int ScrollbackHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Scrollback.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernScrollbackDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = ScrollbackHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Scrollback & pane sizes", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Retained messages, pane balance and mouse-wheel scrolling speed.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK ScrollbackWindowSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId,
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
            PaintModernScrollbackDialog(hwnd, hdc);
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

        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Scrollback.HeaderOffset");
            RemoveWindowSubclass(hwnd, ScrollbackWindowSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernScrollbackDialog(HWND hwnd)
{
    if (!IsScrollbackDialog(hwnd)) return;
    SetWindowSubclass(hwnd, ScrollbackWindowSubclassProc,
                      kScrollbackWindowSubclassId, 0);
    ExpandScrollbackForHeader(hwnd);
    ConfigureModernScrollbackControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsSystemTrayDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_SYSTEMTRAY) != NULL &&
           GetDlgItem(hwnd, IDC_SYSTEMTRAY_RESTORE) != NULL &&
           GetDlgItem(hwnd, IDC_SYSTEMTRAY_NEW) != NULL &&
           GetDlgItem(hwnd, IDC_SYSTEMTRAY_MONLY) != NULL &&
           GetDlgItem(hwnd, IDC_SYSTEMTRAY_FILTER) != NULL;
}

void ConfigureModernSystemTrayControls(HWND hwnd)
{
    const int actionIds[] = { IDOK, IDCANCEL };
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
        else if (GetDlgCtrlID(child) != IDOK && GetDlgCtrlID(child) != IDCANCEL)
            SetWindowTheme(child, L"Explorer", NULL);
    }
}

int ExpandSystemTrayForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.SystemTray.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.SystemTray.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int SystemTrayHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.SystemTray.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernSystemTrayDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = SystemTrayHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"System tray", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Choose minimize behavior and which incoming messages restore PDW.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK SystemTrayWindowSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId,
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
            PaintModernSystemTrayDialog(hwnd, hdc);
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
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.SystemTray.HeaderOffset");
            RemoveWindowSubclass(hwnd, SystemTrayWindowSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernSystemTrayDialog(HWND hwnd)
{
    if (!IsSystemTrayDialog(hwnd)) return;
    SetWindowSubclass(hwnd, SystemTrayWindowSubclassProc,
                      kSystemTrayWindowSubclassId, 0);
    ExpandSystemTrayForHeader(hwnd);
    ConfigureModernSystemTrayControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsInterfaceSetupDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_COMENABLE) != NULL &&
           GetDlgItem(hwnd, IDC_COMPORT) != NULL &&
           GetDlgItem(hwnd, IDC_RS232MODE) != NULL &&
           GetDlgItem(hwnd, IDC_AUDIOENABLE) != NULL &&
           GetDlgItem(hwnd, IDC_AUDIOCONFIG) != NULL &&
           GetDlgItem(hwnd, IDC_AUDIODEVICES) != NULL &&
           GetDlgItem(hwnd, IDC_AUDIOSAMPLERATE) != NULL;
}

void FlattenModernInterfaceField(HWND child)
{
    if (!child) return;

    LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtr(child, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(child, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(child, GWL_EXSTYLE, exStyle);

    SetWindowTheme(child, L"Explorer", NULL);
    SendMessage(child, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

void ConfigureModernInterfaceSetupControls(HWND hwnd)
{
    FlattenModernInterfaceField(GetDlgItem(hwnd, IDC_COMADDR));

    const int comboIds[] = {
        IDC_COMPORT, IDC_RS232MODE, IDC_LEVEL, IDC_COMIRQ,
        IDC_AUDIOCONFIG, IDC_AUDIODEVICES, IDC_AUDIOSAMPLERATE
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(comboIds)); ++i)
    {
        HWND combo = GetDlgItem(hwnd, comboIds[i]);
        if (!combo) continue;
        SetWindowTheme(combo, L"Explorer", NULL);
        SendMessage(combo, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }

    const int actionIds[] = { IDC_AUDIOCUSTOM, IDOK, IDCANCEL };
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
        else
        {
            const int id = GetDlgCtrlID(child);
            if (id != IDC_AUDIOCUSTOM && id != IDOK && id != IDCANCEL)
                SetWindowTheme(child, L"Explorer", NULL);
        }
    }
}

int ExpandInterfaceSetupForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.InterfaceSetup.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.InterfaceSetup.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int InterfaceSetupHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.InterfaceSetup.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernInterfaceSetupDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = InterfaceSetupHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Interface-instellingen", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Configureer seriële invoer en geluidskaartopname zonder het decodergedrag te wijzigen.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK InterfaceSetupWindowSubclassProc(HWND hwnd, UINT message,
                                                  WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR subclassId,
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
            PaintModernInterfaceSetupDialog(hwnd, hdc);
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

        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item =
                reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDC_AUDIOCUSTOM || item->CtlID == IDOK ||
                 item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.InterfaceSetup.HeaderOffset");
            RemoveWindowSubclass(hwnd, InterfaceSetupWindowSubclassProc,
                                 subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernInterfaceSetupDialog(HWND hwnd)
{
    if (!IsInterfaceSetupDialog(hwnd)) return;
    SetWindowSubclass(hwnd, InterfaceSetupWindowSubclassProc,
                      kInterfaceSetupWindowSubclassId, 0);
    ExpandInterfaceSetupForHeader(hwnd);
    ConfigureModernInterfaceSetupControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsLogfileDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_LOGFILEEN) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILEDATE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGBROWSE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGCOLUMN + 1) != NULL &&
           GetDlgItem(hwnd, IDC_LOGCOLUMN + 7) != NULL;
}

void FlattenModernLogfileField(HWND child)
{
    if (!child) return;
    LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtr(child, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(child, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(child, GWL_EXSTYLE, exStyle);

    SetWindowTheme(child, L"Explorer", NULL);
    SendMessage(child, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

void ConfigureModernLogfileControls(HWND hwnd)
{
    FlattenModernLogfileField(GetDlgItem(hwnd, IDC_LOGFILE));

    const int actionIds[] = { IDC_LOGBROWSE, IDOK, IDCANCEL };
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
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
            lstrcmpiW(className, L"Button") != 0)
            continue;

        const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
        if ((style & 0x0F) == BS_GROUPBOX)
            SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                              kModernGroupBoxSubclassId, 0);
        else
        {
            const int id = GetDlgCtrlID(child);
            if (id != IDC_LOGBROWSE && id != IDOK && id != IDCANCEL)
                SetWindowTheme(child, L"Explorer", NULL);
        }
    }
}

int ExpandLogfileForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Logfile.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.Logfile.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int LogfileHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Logfile.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernLogfileDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = LogfileHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, L"Logfile", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc,
              L"Choose the logfile name and the message columns written to disk.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK LogfileWindowSubclassProc(HWND hwnd, UINT message,
                                           WPARAM wParam, LPARAM lParam,
                                           UINT_PTR subclassId,
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
            PaintModernLogfileDialog(hwnd, hdc);
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
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDC_LOGBROWSE || item->CtlID == IDOK ||
                 item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Logfile.HeaderOffset");
            RemoveWindowSubclass(hwnd, LogfileWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernLogfileDialog(HWND hwnd)
{
    if (!IsLogfileDialog(hwnd)) return;
    SetWindowSubclass(hwnd, LogfileWindowSubclassProc,
                      kLogfileWindowSubclassId, 0);
    ExpandLogfileForHeader(hwnd);
    ConfigureModernLogfileControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsCustomAudioDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_THRESHOLD512) != NULL &&
           GetDlgItem(hwnd, IDC_THRESHOLD2400) != NULL &&
           GetDlgItem(hwnd, IDC_RESYNC512) != NULL &&
           GetDlgItem(hwnd, IDC_RESYNC2400) != NULL &&
           GetDlgItem(hwnd, IDC_CENTERING512) != NULL &&
           GetDlgItem(hwnd, IDC_CENTERING2400) != NULL;
}

void ConfigureModernCustomAudioControls(HWND hwnd)
{
    const int comboIds[] = {
        IDC_THRESHOLD512, IDC_THRESHOLD1200, IDC_THRESHOLD1600, IDC_THRESHOLD2400,
        IDC_RESYNC512, IDC_RESYNC1200, IDC_RESYNC1600, IDC_RESYNC2400,
        IDC_CENTERING512, IDC_CENTERING1200, IDC_CENTERING1600, IDC_CENTERING2400
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(comboIds)); ++i)
    {
        HWND combo = GetDlgItem(hwnd, comboIds[i]);
        if (!combo) continue;
        SetWindowTheme(combo, L"Explorer", NULL);
        SendMessage(combo, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }

    const int actionIds[] = { IDOK, IDCANCEL };
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
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
            lstrcmpiW(className, L"Button") != 0)
            continue;
        const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
        if ((style & 0x0F) == BS_GROUPBOX)
            SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                              kModernGroupBoxSubclassId, 0);
    }
}

int ExpandCustomAudioForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.CustomAudio.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.CustomAudio.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int CustomAudioHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.CustomAudio.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernCustomAudioDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());
    const int header = CustomAudioHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, L"Custom audio setup", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc,
              L"Tune threshold, re-sync and centering values for each decoder rate.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK CustomAudioWindowSubclassProc(HWND hwnd, UINT message,
                                               WPARAM wParam, LPARAM lParam,
                                               UINT_PTR subclassId,
                                               DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernCustomAudioDialog(hwnd, hdc);
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
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.CustomAudio.HeaderOffset");
            RemoveWindowSubclass(hwnd, CustomAudioWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernCustomAudioDialog(HWND hwnd)
{
    if (!IsCustomAudioDialog(hwnd)) return;
    SetWindowSubclass(hwnd, CustomAudioWindowSubclassProc,
                      kCustomAudioWindowSubclassId, 0);
    ExpandCustomAudioForHeader(hwnd);
    ConfigureModernCustomAudioControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsStatisticsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_STATHRF64N) != NULL &&
           GetDlgItem(hwnd, IDC_STATHR_EM_A) != NULL &&
           GetDlgItem(hwnd, IDC_STATDLF64N) != NULL &&
           GetDlgItem(hwnd, IDC_STATDL_EM_A) != NULL &&
           GetDlgItem(hwnd, IDC_STATFILE) != NULL &&
           GetDlgItem(hwnd, IDC_STATFILEEN) != NULL;
}

void FlattenModernStatisticsField(HWND child)
{
    if (!child) return;
    LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtr(child, GWL_STYLE, style);
    LONG_PTR exStyle = GetWindowLongPtr(child, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(child, GWL_EXSTYLE, exStyle);
    SetWindowTheme(child, L"Explorer", NULL);
    SendMessage(child, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

void ConfigureModernStatisticsControls(HWND hwnd)
{
    FlattenModernStatisticsField(GetDlgItem(hwnd, IDC_STATFILE));

    const int actionIds[] = { IDC_STATBROWSE, IDOK, IDCANCEL };
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
        else
        {
            const int id = GetDlgCtrlID(child);
            if (id != IDC_STATBROWSE && id != IDOK && id != IDCANCEL)
                SetWindowTheme(child, L"Explorer", NULL);
        }
    }
}

int ExpandStatisticsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Statistics.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.Statistics.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int StatisticsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Statistics.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernStatisticsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());
    const int header = StatisticsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, L"Decoder statistics", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc,
              L"Hourly and daily message totals with optional statistics-file output.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK StatisticsWindowSubclassProc(HWND hwnd, UINT message,
                                              WPARAM wParam, LPARAM lParam,
                                              UINT_PTR subclassId,
                                              DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernStatisticsDialog(hwnd, hdc);
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
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDC_STATBROWSE || item->CtlID == IDOK ||
                 item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Statistics.HeaderOffset");
            RemoveWindowSubclass(hwnd, StatisticsWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernStatisticsDialog(HWND hwnd)
{
    if (!IsStatisticsDialog(hwnd)) return;
    SetWindowSubclass(hwnd, StatisticsWindowSubclassProc,
                      kStatisticsWindowSubclassId, 0);
    ExpandStatisticsForHeader(hwnd);
    ConfigureModernStatisticsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

bool IsColorsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_COLORBACKGND) != NULL &&
           GetDlgItem(hwnd, IDC_COLORCAPCODE) != NULL &&
           GetDlgItem(hwnd, IDC_COLORTIMESTAMP) != NULL &&
           GetDlgItem(hwnd, IDC_COLORNUMERIC) != NULL &&
           GetDlgItem(hwnd, IDC_COLORDEFAULT) != NULL;
}

bool UseDutchUiLanguage()
{
    LANGID language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(language) == LANG_DUTCH) return true;
    language = GetSystemDefaultUILanguage();
    return PRIMARYLANGID(language) == LANG_DUTCH;
}

const wchar_t* ColorsTitle()
{
    return UseDutchUiLanguage() ? L"Kleuren" : L"Colors";
}

const wchar_t* ColorsSubtitle()
{
    return UseDutchUiLanguage()
        ? L"Pas de weergavekleuren van gedecodeerde berichten aan."
        : L"Customize the display colors used for decoded messages.";
}

void SetColorsDialogText(HWND hwnd)
{
    const bool dutch = UseDutchUiLanguage();
    SetWindowTextW(hwnd, ColorsTitle());
    struct ItemText { int id; const wchar_t* nl; const wchar_t* en; };
    const ItemText items[] = {
        { IDC_COLORBACKGND, L"Achtergrond", L"Background" },
        { IDC_COLORCAPCODE, L"Capcode", L"Address" },
        { IDC_COLORFLEXPHASE, L"FLEX-fase", L"Phase/Function" },
        { IDC_COLORTIMESTAMP, L"Tijd/datum", L"Time/Date" },
        { IDC_COLORBITERRORS, L"Bitfouten", L"Bit Errors" },
        { IDC_COLORNUMERIC, L"Numeriek/toon", L"Numeric/Tone" },
        { IDC_COLORALPHANUM, L"Bericht", L"Message" },
        { IDC_COLORFLEXBIN, L"FLEX binair", L"FLEX Binary" },
        { IDC_COLORFILTMATCH, L"Filtertreffer", L"Filter Match" },
        { IDC_COLORFILTERLABEL, L"Filterlabel", L"Filter Label" },
        { IDC_COLORDEFAULT, L"Standaardkleuren", L"Default Colors" },
        { IDOK, L"OK", L"OK" },
        { IDCANCEL, L"Annuleren", L"Cancel" }
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(items)); ++i)
    {
        HWND child = GetDlgItem(hwnd, items[i].id);
        if (child) SetWindowTextW(child, dutch ? items[i].nl : items[i].en);
    }
}

void ConfigureModernColorsControls(HWND hwnd)
{
    SetColorsDialogText(hwnd);
    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        SendMessage(child, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        wchar_t className[32] = {};
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0)
            continue;
        if (lstrcmpiW(className, L"Button") == 0)
            SetWindowTheme(child, L"Explorer", NULL);
    }
}

int ExpandColorsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Colors.HeaderOffset");
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
    SetPropW(hwnd, L"PDW.Colors.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int ColorsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Colors.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernColorsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());
    const int header = ColorsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, ColorsTitle(), -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc, ColorsSubtitle(), -1,
              &subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK ColorsWindowSubclassProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId,
                                          DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernColorsDialog(hwnd, hdc);
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
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Colors.HeaderOffset");
            RemoveWindowSubclass(hwnd, ColorsWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernColorsDialog(HWND hwnd)
{
    if (!IsColorsDialog(hwnd)) return;
    SetWindowSubclass(hwnd, ColorsWindowSubclassProc,
                      kColorsWindowSubclassId, 0);
    ExpandColorsForHeader(hwnd);
    ConfigureModernColorsControls(hwnd);
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
    if (message == WM_ERASEBKGND)
        return 1;

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
            // RX quality changes at one-second cadence, while the original
            // signal indicator state changes on the 100 ms decoder timer.
            if (wParam == kLegacySecondTimer || wParam == kLegacyDecodeTimer)
                DrawModernWorkspace(hwnd);
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

    LoadUiThemeSetting(szShortAppName, szIniPathName);
    ApplyMainDwmTheme(hwnd);
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
    if (IsFilterOptionsDialog(hwnd)) EnableModernFilterOptionsDialog(hwnd);
    if (IsFilterFindDialog(hwnd)) EnableModernFilterFindDialog(hwnd);
    if (IsFilterDuplicateDialog(hwnd)) EnableModernFilterDuplicateDialog(hwnd);
    if (IsOptionsDialog(hwnd)) EnableModernOptionsDialog(hwnd);
    if (IsGeneralOptionsDialog(hwnd)) EnableModernGeneralOptionsDialog(hwnd);
    if (IsScreenOptionsDialog(hwnd)) EnableModernScreenOptionsDialog(hwnd);
    if (IsScrollbackDialog(hwnd)) EnableModernScrollbackDialog(hwnd);
    if (IsSystemTrayDialog(hwnd)) EnableModernSystemTrayDialog(hwnd);
    if (IsInterfaceSetupDialog(hwnd)) EnableModernInterfaceSetupDialog(hwnd);
    if (IsLogfileDialog(hwnd)) EnableModernLogfileDialog(hwnd);
    if (IsCustomAudioDialog(hwnd)) EnableModernCustomAudioDialog(hwnd);
    if (IsStatisticsDialog(hwnd)) EnableModernStatisticsDialog(hwnd);
    if (IsColorsDialog(hwnd)) EnableModernColorsDialog(hwnd);
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
