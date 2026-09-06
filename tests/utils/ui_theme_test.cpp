#include "ui_theme.h"

#include <cassert>
#include <cstring>

int main()
{
    using pdw::UiTheme;

    // Fresh/missing configuration must be dark by default.
    assert(pdw::DefaultUiTheme() == UiTheme::Dark);
    assert(pdw::ParseUiTheme(nullptr) == UiTheme::Dark);
    assert(pdw::ParseUiTheme("") == UiTheme::Dark);

    // Persisted values remain stable and case-insensitive.
    assert(pdw::ParseUiTheme("dark") == UiTheme::Dark);
    assert(pdw::ParseUiTheme("DARK") == UiTheme::Dark);
    assert(pdw::ParseUiTheme("0") == UiTheme::Dark);
    assert(pdw::ParseUiTheme("light") == UiTheme::Light);
    assert(pdw::ParseUiTheme("Light") == UiTheme::Light);
    assert(pdw::ParseUiTheme("1") == UiTheme::Light);

    // Unknown legacy/future values fail closed to the caller-selected default.
    assert(pdw::ParseUiTheme("unknown", UiTheme::Dark) == UiTheme::Dark);
    assert(pdw::ParseUiTheme("unknown", UiTheme::Light) == UiTheme::Light);

    assert(std::strcmp(pdw::UiThemeIniValue(UiTheme::Dark), "dark") == 0);
    assert(std::strcmp(pdw::UiThemeIniValue(UiTheme::Light), "light") == 0);

    const pdw::ThemePalette& dark = pdw::GetThemePalette(UiTheme::Dark);
    const pdw::ThemePalette& light = pdw::GetThemePalette(UiTheme::Light);

    // Palettes must be genuinely distinct, not aliases of the same light skin.
    assert(dark.workspaceBackground != light.workspaceBackground);
    assert(dark.paneBackground != light.paneBackground);
    assert(dark.textPrimary != light.textPrimary);
    assert(dark.accent != dark.workspaceBackground);
    assert(light.accent != light.workspaceBackground);

    return 0;
}
