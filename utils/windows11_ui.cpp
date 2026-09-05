#include "windows11_ui.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

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

HFONT g_dialogFont = NULL;
HHOOK g_dialogHook = NULL;

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

    // Mica/backdrop is intentionally best-effort. The legacy PDW client area
    // keeps drawing exactly as before; Windows owns only the non-client shell.
    int backdrop = kDwmBackdropMainWindow;
    DwmSetWindowAttribute(hwnd, kDwmSystemBackdropType, &backdrop, sizeof(backdrop));
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
