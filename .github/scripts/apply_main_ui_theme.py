from pathlib import Path


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly 1 match, got {count}")
    return text.replace(old, new, 1)

ui_path = Path("utils/windows11_ui.cpp")
pane_path = Path("utils/windows11_pane_skin.cpp")
ui = ui_path.read_text(encoding="utf-8")
pane = pane_path.read_text(encoding="utf-8")

# Theme API is presentation-only and lives outside the legacy PROFILE ABI.
ui = replace_once(ui,
    '#include "windows11_ui.h"\n',
    '#include "windows11_ui.h"\n#include "ui_theme.h"\n',
    "ui theme include")

ui = replace_once(ui,
    'const int kResumeMonitorCommand = 50002;\n',
    'const int kResumeMonitorCommand = 50002;\nconst int kToggleThemeCommand = 50003;\n',
    "theme command")

ui = replace_once(ui,
    '    { L"\\xE715", L"SMTP / network", IDM_MAIL },\n    { L"\\xE946", L"About PDW", IDM_ABOUT }',
    '    { L"\\xE715", L"SMTP / network", IDM_MAIL },\n    { L"\\xE706", L"Theme", kToggleThemeCommand },\n    { L"\\xE946", L"About PDW", IDM_ABOUT }',
    "theme flyout item")

old = '''    const COLORREF accent = RGB(0, 120, 212);\n    const COLORREF accentPressed = RGB(0, 95, 184);\n    const COLORREF normalBg = RGB(246, 249, 252);\n    const COLORREF hoverBg = RGB(240, 246, 252);\n    const COLORREF fg = RGB(32, 32, 32);\n    const COLORREF selectedFg = RGB(255, 255, 255);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF accent = palette.accent;\n    const COLORREF accentPressed = palette.accentPressed;\n    const COLORREF normalBg = palette.controlBackground;\n    const COLORREF hoverBg = palette.controlHover;\n    const COLORREF fg = palette.textPrimary;\n    const COLORREF selectedFg = palette.selectionText;'''
ui = replace_once(ui, old, new, "theme icon button colors")
ui = replace_once(ui,
    '        border = RGB(224, 235, 246);',
    '        border = palette.border;',
    "theme hover border")

old = '''    const COLORREF shellBg = RGB(238, 244, 249);\n    const COLORREF capsuleBg = RGB(244, 248, 251);\n    const COLORREF capsuleBorder = RGB(224, 231, 239);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF shellBg = palette.shellBackground;\n    const COLORREF capsuleBg = palette.controlBackground;\n    const COLORREF capsuleBorder = palette.border;'''
ui = replace_once(ui, old, new, "theme navigation")

old = '''    const COLORREF rowBg = RGB(245, 249, 252);\n    const COLORREF rowBorder = RGB(218, 226, 235);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF rowBg = palette.controlBackground;\n    const COLORREF rowBorder = palette.border;'''
ui = replace_once(ui, old, new, "theme command strip")

old = '''    const COLORREF background = RGB(239, 245, 250);\n    const COLORREF foreground = RGB(45, 45, 45);\n    const COLORREF divider = RGB(225, 230, 236);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF background = palette.cardHeaderBackground;\n    const COLORREF foreground = palette.textSecondary;\n    const COLORREF divider = palette.divider;'''
ui = replace_once(ui, old, new, "theme column headers")

old = '''    const COLORREF cardBg = RGB(248, 251, 253);\n    const COLORREF cardBorder = RGB(206, 217, 229);\n    const COLORREF titleBg = RGB(236, 243, 249);\n    const COLORREF titleFg = RGB(24, 39, 58);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF cardBg = palette.cardBackground;\n    const COLORREF cardBorder = palette.border;\n    const COLORREF titleBg = palette.cardHeaderBackground;\n    const COLORREF titleFg = palette.textPrimary;'''
ui = replace_once(ui, old, new, "theme card")

ui = replace_once(ui,
    '        FillRoundedRect(hdc, meter, RGB(219, 228, 236), RGB(203, 214, 224),\n                        ScaleForDpi(hwnd, 8));',
    '        FillRoundedRect(hdc, meter, palette.controlBackground, palette.border,\n                        ScaleForDpi(hwnd, 8));',
    "theme meter background")
ui = replace_once(ui,
    '''            const COLORREF signalColor = signal >= 15\n                ? RGB(20, 170, 62)\n                : (signal >= 8 ? RGB(0, 120, 212) : RGB(86, 145, 198));''',
    '''            const COLORREF signalColor = signal >= 15\n                ? palette.signalHigh\n                : (signal >= 8 ? palette.signalMid : palette.signalLow);''',
    "theme meter signal colors")
ui = replace_once(ui,
    '        HPEN markerPen = CreatePen(PS_SOLID, ScaleForDpi(hwnd, 2), RGB(28, 45, 62));',
    '        HPEN markerPen = CreatePen(PS_SOLID, ScaleForDpi(hwnd, 2), palette.textSecondary);',
    "theme meter marker")
ui = replace_once(ui,
    '        SetTextColor(hdc, quality > 0.0 && quality < 90.0\n                     ? RGB(145, 86, 0) : titleFg);',
    '        SetTextColor(hdc, quality > 0.0 && quality < 90.0\n                     ? palette.warning : titleFg);',
    "theme quality warning")

old = '''    const COLORREF bg = RGB(239, 245, 250);\n    const COLORREF line = RGB(216, 224, 233);\n    const COLORREF fg = RGB(42, 51, 61);\n    const COLORREF green = RGB(20, 170, 62);\n    const COLORREF paused = RGB(196, 120, 0);\n    const COLORREF gray = RGB(154, 160, 168);'''
new = '''    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    const COLORREF bg = palette.shellBackground;\n    const COLORREF line = palette.divider;\n    const COLORREF fg = palette.textSecondary;\n    const COLORREF green = palette.success;\n    const COLORREF paused = palette.warning;\n    const COLORREF gray = palette.textMuted;'''
ui = replace_once(ui, old, new, "theme status bar")

ui = replace_once(ui,
    '    const COLORREF workspace = RGB(231, 238, 245);',
    '    const COLORREF workspace = pdw::CurrentThemePalette().workspaceBackground;',
    "theme workspace")

# Main-window DWM styling + persistent theme toggle, inserted after the buffered renderer.
marker = '''void CloseSettingsFlyout()\n{'''
insert = '''void ApplyMainDwmTheme(HWND hwnd)\n{\n    if (!hwnd) return;\n    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;\n    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));\n}\n\nbool TogglePersistedUiTheme(HWND hwnd)\n{\n    const pdw::UiTheme next = pdw::CurrentUiTheme() == pdw::UiTheme::Dark\n        ? pdw::UiTheme::Light\n        : pdw::UiTheme::Dark;\n    if (!pdw::SaveUiThemeSetting(next, szShortAppName, szIniPathName))\n        return false;\n\n    ApplyMainDwmTheme(hwnd);\n    if (Pane1.hWnd) InvalidateRect(Pane1.hWnd, NULL, TRUE);\n    if (Pane2.hWnd) InvalidateRect(Pane2.hWnd, NULL, TRUE);\n    DrawModernWorkspace(hwnd);\n    return true;\n}\n\nvoid CloseSettingsFlyout()\n{'''
ui = replace_once(ui, marker, insert, "theme toggle helpers")

old = '''                const int command = g_flyoutItems[index].command;\n                HWND owner = GetWindow(hwnd, GW_OWNER);\n                DestroyWindow(hwnd);\n                if (owner && command)\n                    PostMessage(owner, WM_COMMAND, MAKEWPARAM(command, 0), 0);'''
new = '''                const int command = g_flyoutItems[index].command;\n                HWND owner = GetWindow(hwnd, GW_OWNER);\n                DestroyWindow(hwnd);\n                if (owner && command == kToggleThemeCommand)\n                    TogglePersistedUiTheme(owner);\n                else if (owner && command)\n                    PostMessage(owner, WM_COMMAND, MAKEWPARAM(command, 0), 0);'''
ui = replace_once(ui, old, new, "theme flyout dispatch")

old = '''            HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));\n            FillRect(hdc, &client, bg);\n            DeleteObject(bg);'''
new = '''            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n            HBRUSH bg = CreateSolidBrush(palette.cardBackground);\n            FillRect(hdc, &client, bg);\n            DeleteObject(bg);'''
ui = replace_once(ui, old, new, "theme flyout background")
ui = replace_once(ui,
    '''                if (i == g_flyoutHover)\n                    FillRoundedRect(hdc, row, RGB(239, 246, 252), RGB(239, 246, 252),\n                                    ScaleForDpi(hwnd, 8));''',
    '''                if (i == g_flyoutHover)\n                    FillRoundedRect(hdc, row, palette.controlHover, palette.controlHover,\n                                    ScaleForDpi(hwnd, 8));''',
    "theme flyout hover")
ui = replace_once(ui,
    '                SetTextColor(hdc, RGB(0, 120, 212));',
    '                SetTextColor(hdc, palette.accent);',
    "theme flyout icon")
ui = replace_once(ui,
    '''                SetTextColor(hdc, RGB(32, 32, 32));\n                oldFont = SelectObject(hdc, GetDialogFont());\n                RECT text = row;\n                text.left += ScaleForDpi(hwnd, 46);\n                DrawTextW(hdc, g_flyoutItems[i].label, -1, &text,\n                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);''',
    '''                SetTextColor(hdc, palette.textPrimary);\n                oldFont = SelectObject(hdc, GetDialogFont());\n                RECT text = row;\n                text.left += ScaleForDpi(hwnd, 46);\n                const wchar_t* label = g_flyoutItems[i].label;\n                if (g_flyoutItems[i].command == kToggleThemeCommand)\n                    label = pdw::CurrentUiTheme() == pdw::UiTheme::Dark\n                        ? L"Theme: Dark" : L"Theme: Light";\n                DrawTextW(hdc, label, -1, &text,\n                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);''',
    "theme flyout label")

old = '''    const BOOL dark = FALSE;\n    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));'''
new = '''    LoadUiThemeSetting(szShortAppName, szIniPathName);\n    ApplyMainDwmTheme(hwnd);'''
ui = replace_once(ui, old, new, "load persisted main theme")

# Pane renderer: preserve legacy semantic colors but enforce contrast against the selected theme.
pane = replace_once(pane,
    '#include "windows11_ui.h"\n',
    '#include "windows11_ui.h"\n#include "ui_theme.h"\n',
    "pane theme include")

old = '''COLORREF EnsureLightSurfaceContrast(COLORREF color)\n{\n    const int red = GetRValue(color);\n    const int green = GetGValue(color);\n    const int blue = GetBValue(color);\n    const int luminance = (red * 299 + green * 587 + blue * 114) / 1000;\n\n    // Legacy profiles were often tuned for a black monitor background. Keep\n    // configured semantic colors where possible, but prevent near-white text\n    // from disappearing on the new light workspace.\n    if (luminance > 218)\n        return RGB(45, 52, 61);\n    return color;\n}'''
new = '''COLORREF EnsureSurfaceContrast(COLORREF color)\n{\n    const int red = GetRValue(color);\n    const int green = GetGValue(color);\n    const int blue = GetBValue(color);\n    const int luminance = (red * 299 + green * 587 + blue * 114) / 1000;\n    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n\n    // Legacy profiles may have been tuned either for the original black pane\n    // or for the modern light pane. Keep semantic colors where possible and\n    // only substitute when contrast would become unreadable.\n    if (pdw::CurrentUiTheme() == pdw::UiTheme::Dark)\n    {\n        if (luminance < 70) return palette.textSecondary;\n        return color;\n    }\n    if (luminance > 218) return palette.textSecondary;\n    return color;\n}'''
pane = replace_once(pane, old, new, "theme-aware message contrast")
pane = pane.replace('EnsureLightSurfaceContrast(', 'EnsureSurfaceContrast(')
if 'EnsureLightSurfaceContrast(' in pane:
    raise RuntimeError("legacy light-only contrast helper still referenced")
pane = replace_once(pane,
    '            return RGB(35, 42, 51);',
    '            return pdw::CurrentThemePalette().textPrimary;',
    "theme default message color")
pane = replace_once(pane,
    '    HBRUSH brush = CreateSolidBrush(RGB(214, 232, 250));',
    '    HBRUSH brush = CreateSolidBrush(pdw::CurrentThemePalette().selectionBackground);',
    "theme selection")
pane = replace_once(pane,
    '    HBRUSH base = CreateSolidBrush(RGB(247, 250, 252));',
    '    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n    HBRUSH base = CreateSolidBrush(palette.paneBackground);',
    "theme pane background")
pane = replace_once(pane,
    '            HBRUSH alternate = CreateSolidBrush(RGB(240, 246, 250));',
    '            HBRUSH alternate = CreateSolidBrush(palette.paneAlternateBackground);',
    "theme alternate row")
pane = replace_once(pane,
    '        HPEN separator = CreatePen(PS_SOLID, 1, RGB(238, 242, 246));',
    '        HPEN separator = CreatePen(PS_SOLID, 1, palette.divider);',
    "theme pane divider")

ui_path.write_text(ui, encoding="utf-8", newline="")
pane_path.write_text(pane, encoding="utf-8", newline="")
print("Wired dark/light palette into main shell, RX meter, settings flyout and message panes.")
