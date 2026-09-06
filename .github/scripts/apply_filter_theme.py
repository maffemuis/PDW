from pathlib import Path

path = Path("utils/windows11_ui.cpp")
text = path.read_text(encoding="utf-8")


def replace_once(old, new, label):
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly 1 match, got {count}")
    text = text.replace(old, new, 1)


def replace_function(start_marker, next_marker, replacement, label):
    global text
    start = text.find(start_marker)
    if start < 0:
        raise RuntimeError(f"{label}: start marker missing")
    end = text.find(next_marker, start)
    if end < 0:
        raise RuntimeError(f"{label}: end marker missing")
    if text.find(start_marker, start + 1) >= 0:
        raise RuntimeError(f"{label}: start marker is not unique")
    text = text[:start] + replacement + text[end:]

is_filter = '''bool IsFilterDialog(HWND hwnd)\n{\n    if (!hwnd) return false;\n    wchar_t className[32] = {};\n    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||\n        lstrcmpW(className, L"#32770") != 0)\n        return false;\n    return GetDlgItem(hwnd, IDC_FILTERS) != NULL;\n}\n\n'''
helpers = is_filter + r'''COLORREF EnsureFilterListContrast(COLORREF color)
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

'''
replace_once(is_filter, helpers, "filter theme helpers")

replace_once(
    '        SetWindowTheme(list, L"Explorer", NULL);\n',
    '        SetWindowTheme(list,\n                       pdw::CurrentUiTheme() == pdw::UiTheme::Dark\n                           ? L"DarkMode_Explorer" : L"Explorer",\n                       NULL);\n',
    "filter list native theme")

replace_once(
    '''        ListView_SetBkColor(list, RGB(247, 250, 252));\n        ListView_SetTextBkColor(list, RGB(247, 250, 252));\n        ListView_SetTextColor(list, RGB(32, 40, 48));''',
    '''        const pdw::ThemePalette& palette = pdw::CurrentThemePalette();\n        ListView_SetBkColor(list, palette.cardBackground);\n        ListView_SetTextBkColor(list, palette.cardBackground);\n        ListView_SetTextColor(list, palette.textPrimary);''',
    "filter list palette")

new_button = r'''void DrawModernFilterButton(const DRAWITEMSTRUCT* item)
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

'''
replace_function(
    "void DrawModernFilterButton(const DRAWITEMSTRUCT* item)\n{",
    "void PaintModernFilterDialog(HWND hwnd, HDC hdc)\n{",
    new_button,
    "filter button renderer")

new_paint = r'''void PaintModernFilterDialog(HWND hwnd, HDC hdc)
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

'''
replace_function(
    "void PaintModernFilterDialog(HWND hwnd, HDC hdc)\n{",
    "LRESULT CALLBACK FilterWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,",
    new_paint,
    "filter dialog renderer")

old_draw = '''            if (item && item->CtlType == ODT_BUTTON && IsModernFilterButton(item->CtlID))\n            {\n                DrawModernFilterButton(item);\n                return TRUE;\n            }\n            break;'''
new_draw = '''            if (item && item->CtlType == ODT_BUTTON && IsModernFilterButton(item->CtlID))\n            {\n                DrawModernFilterButton(item);\n                return TRUE;\n            }\n            if (item && item->CtlID == IDC_FILTERS)\n            {\n                DrawModernFilterListItem(item);\n                return TRUE;\n            }\n            break;'''
replace_once(old_draw, new_draw, "filter owner draw dispatch")

replace_once(
    '''void EnableResizableFilterDialog(HWND hwnd)\n{\n    if (!IsFilterDialog(hwnd)) return;\n\n    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);''',
    '''void EnableResizableFilterDialog(HWND hwnd)\n{\n    if (!IsFilterDialog(hwnd)) return;\n\n    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;\n    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));\n\n    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);''',
    "filter DWM theme")

path.write_text(text, encoding="utf-8", newline="")
print("Applied theme-safe Dark/Light styling to the Filters main dialog only.")
