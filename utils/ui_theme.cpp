#include "ui_theme.h"

#include <cctype>
#include <cstring>

namespace pdw {
namespace {

UiTheme g_currentTheme = UiTheme::Dark;

bool EqualsIgnoreCase(const char* left, const char* right)
{
    if (!left || !right) return false;
    while (*left && *right)
    {
        const unsigned char a = static_cast<unsigned char>(*left++);
        const unsigned char b = static_cast<unsigned char>(*right++);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return *left == '\0' && *right == '\0';
}

const ThemePalette kDark = {
    RGB(18, 22, 27),    // windowBackground
    RGB(22, 28, 35),    // workspaceBackground
    RGB(25, 31, 38),    // shellBackground
    RGB(31, 38, 46),    // controlBackground
    RGB(39, 48, 58),    // controlHover
    RGB(28, 35, 43),    // cardBackground
    RGB(32, 40, 49),    // cardHeaderBackground
    RGB(19, 24, 30),    // paneBackground
    RGB(23, 29, 36),    // paneAlternateBackground
    RGB(57, 68, 80),    // border
    RGB(48, 58, 69),    // divider
    RGB(235, 240, 245), // textPrimary
    RGB(184, 194, 204), // textSecondary
    RGB(132, 145, 158), // textMuted
    RGB(38, 79, 113),   // selectionBackground
    RGB(245, 249, 252), // selectionText
    RGB(42, 143, 231),  // accent
    RGB(20, 112, 196),  // accentPressed
    RGB(41, 181, 95),   // success
    RGB(224, 159, 48),  // warning
    RGB(222, 75, 75),   // danger
    RGB(76, 139, 190),  // signalLow
    RGB(42, 143, 231),  // signalMid
    RGB(41, 181, 95)    // signalHigh
};

const ThemePalette kLight = {
    RGB(232, 238, 244), // windowBackground
    RGB(224, 232, 240), // workspaceBackground
    RGB(232, 239, 245), // shellBackground
    RGB(239, 244, 248), // controlBackground
    RGB(228, 237, 245), // controlHover
    RGB(241, 246, 249), // cardBackground
    RGB(230, 238, 244), // cardHeaderBackground
    RGB(240, 245, 248), // paneBackground
    RGB(233, 240, 245), // paneAlternateBackground
    RGB(195, 207, 218), // border
    RGB(207, 217, 226), // divider
    RGB(24, 39, 58),    // textPrimary
    RGB(72, 84, 97),    // textSecondary
    RGB(104, 116, 128), // textMuted
    RGB(214, 232, 250), // selectionBackground
    RGB(24, 39, 58),    // selectionText
    RGB(0, 120, 212),   // accent
    RGB(0, 95, 184),    // accentPressed
    RGB(20, 170, 62),   // success
    RGB(196, 120, 0),   // warning
    RGB(196, 43, 43),   // danger
    RGB(86, 145, 198),  // signalLow
    RGB(0, 120, 212),   // signalMid
    RGB(20, 170, 62)    // signalHigh
};

} // namespace

UiTheme DefaultUiTheme()
{
    return UiTheme::Dark;
}

UiTheme ParseUiTheme(const char* value, UiTheme fallback)
{
    if (!value || !*value) return fallback;
    if (EqualsIgnoreCase(value, "dark") || std::strcmp(value, "0") == 0)
        return UiTheme::Dark;
    if (EqualsIgnoreCase(value, "light") || std::strcmp(value, "1") == 0)
        return UiTheme::Light;
    return fallback;
}

const char* UiThemeIniValue(UiTheme theme)
{
    return theme == UiTheme::Light ? "light" : "dark";
}

UiTheme LoadUiThemeSetting(const char* section, const char* iniPath)
{
    char value[32] = {};
    if (!section || !*section || !iniPath || !*iniPath)
    {
        g_currentTheme = DefaultUiTheme();
        return g_currentTheme;
    }

    GetPrivateProfileStringA(section, "UITheme", "", value,
                             static_cast<DWORD>(sizeof(value)), iniPath);
    g_currentTheme = ParseUiTheme(value, DefaultUiTheme());
    return g_currentTheme;
}

bool SaveUiThemeSetting(UiTheme theme, const char* section, const char* iniPath)
{
    if (!section || !*section || !iniPath || !*iniPath) return false;
    if (!WritePrivateProfileStringA(section, "UITheme", UiThemeIniValue(theme), iniPath))
        return false;
    g_currentTheme = theme;
    return true;
}

void SetCurrentUiTheme(UiTheme theme)
{
    g_currentTheme = theme;
}

UiTheme CurrentUiTheme()
{
    return g_currentTheme;
}

const ThemePalette& CurrentThemePalette()
{
    return GetThemePalette(g_currentTheme);
}

const ThemePalette& GetThemePalette(UiTheme theme)
{
    return theme == UiTheme::Light ? kLight : kDark;
}

} // namespace pdw
