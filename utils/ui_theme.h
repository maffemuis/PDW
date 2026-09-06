#ifndef PDW_UI_THEME_H
#define PDW_UI_THEME_H

#include <windows.h>

namespace pdw {

enum class UiTheme {
    Dark = 0,
    Light = 1
};

struct ThemePalette {
    COLORREF windowBackground;
    COLORREF workspaceBackground;
    COLORREF shellBackground;
    COLORREF controlBackground;
    COLORREF controlHover;
    COLORREF cardBackground;
    COLORREF cardHeaderBackground;
    COLORREF paneBackground;
    COLORREF paneAlternateBackground;
    COLORREF border;
    COLORREF divider;
    COLORREF textPrimary;
    COLORREF textSecondary;
    COLORREF textMuted;
    COLORREF selectionBackground;
    COLORREF selectionText;
    COLORREF accent;
    COLORREF accentPressed;
    COLORREF success;
    COLORREF warning;
    COLORREF danger;
    COLORREF signalLow;
    COLORREF signalMid;
    COLORREF signalHigh;
};

// A fresh PDW profile deliberately starts in the modern dark theme.
UiTheme DefaultUiTheme();

// INI-facing conversion. Missing/unknown values fall back to the supplied
// default so legacy profiles without a UITheme key remain deterministic.
UiTheme ParseUiTheme(const char* value, UiTheme fallback = UiTheme::Dark);
const char* UiThemeIniValue(UiTheme theme);

const ThemePalette& GetThemePalette(UiTheme theme);

} // namespace pdw

#endif // PDW_UI_THEME_H
