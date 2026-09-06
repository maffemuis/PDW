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
helpers = is_filter + r'''bool IsThemeAwareDialog(HWND hwnd)
{
    return hwnd && (IsFilterDialog(hwnd) ||
                    GetPropW(hwnd, L"PDW.ThemeAwareDialog") != NULL);
}

HBRUSH GetCurrentThemeWindowBrush()
{
    static HBRUSH darkBrush = CreateSolidBrush(
        pdw::GetThemePalette(pdw::UiTheme::Dark).windowBackground);
    static HBRUSH lightBrush = CreateSolidBrush(
        pdw::GetThemePalette(pdw::UiTheme::Light).windowBackground);
    return pdw::CurrentUiTheme() == pdw::UiTheme::Dark
        ? darkBrush : lightBrush;
}

HBRUSH GetCurrentThemeControlBrush()
{
    static HBRUSH darkBrush = CreateSolidBrush(
        pdw::GetThemePalette(pdw::UiTheme::Dark).controlBackground);
    static HBRUSH lightBrush = CreateSolidBrush(
        pdw::GetThemePalette(pdw::UiTheme::Light).controlBackground);
    return pdw::CurrentUiTheme() == pdw::UiTheme::Dark
        ? darkBrush : lightBrush;
}

'''
replace_once(is_filter, helpers, "theme-aware dialog helpers")

new_button = r'''void DrawModernFilterButton(const DRAWITEMSTRUCT* item)
{
    if (!item || !item->hwndItem) return;

    const bool themedDialog = IsThemeAwareDialog(GetParent(item->hwndItem));
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();

    RECT rect = item->rcItem;
    const COLORREF clearColor = themedDialog
        ? palette.windowBackground : RGB(246, 249, 252);
    HBRUSH clear = CreateSolidBrush(clearColor);
    FillRect(item->hDC, &rect, clear);
    DeleteObject(clear);

    const bool enabled = (item->itemState & ODS_DISABLED) == 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool primary = item->CtlID == IDC_FILTERADD || item->CtlID == IDOK;

    COLORREF fill = themedDialog ? palette.controlBackground : RGB(255, 255, 255);
    COLORREF border = themedDialog ? palette.border : RGB(205, 215, 226);
    COLORREF textColor = themedDialog ? palette.textPrimary : RGB(35, 43, 52);
    if (!enabled)
    {
        fill = themedDialog ? palette.controlBackground : RGB(244, 246, 248);
        border = themedDialog ? palette.divider : RGB(226, 231, 236);
        textColor = themedDialog ? palette.textMuted : RGB(150, 157, 165);
    }
    else if (primary)
    {
        fill = themedDialog
            ? (pressed ? palette.accentPressed : palette.accent)
            : (pressed ? RGB(0, 95, 184) : RGB(0, 120, 212));
        border = fill;
        textColor = themedDialog ? palette.selectionText : RGB(255, 255, 255);
    }
    else if (pressed)
    {
        fill = themedDialog ? palette.controlHover : RGB(232, 240, 248);
        border = themedDialog ? palette.accent : RGB(179, 201, 224);
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
    "theme-aware shared filter button renderer")

new_groupbox = r'''LRESULT CALLBACK ModernGroupBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
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

            const bool themedDialog = IsThemeAwareDialog(GetParent(hwnd));
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            const COLORREF card = themedDialog
                ? palette.cardBackground : RGB(255, 255, 255);
            const COLORREF border = themedDialog
                ? palette.border : RGB(212, 221, 231);
            const COLORREF titleColor = themedDialog
                ? palette.textPrimary : RGB(45, 56, 68);
            FillRoundedRect(hdc, client, card, border, ScaleForDpi(hwnd, 10));

            wchar_t label[96] = {};
            GetWindowTextW(hwnd, label, ARRAYSIZE(label));
            if (label[0])
            {
                RECT textRect = client;
                textRect.left += ScaleForDpi(hwnd, 12);
                textRect.right -= ScaleForDpi(hwnd, 12);
                textRect.top += ScaleForDpi(hwnd, 4);
                textRect.bottom = textRect.top + ScaleForDpi(hwnd, 22);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, titleColor);
                HGDIOBJ oldFont = SelectObject(hdc, GetHeaderFont());
                DrawTextW(hdc, label, -1, &textRect,
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

'''
replace_function(
    "LRESULT CALLBACK ModernGroupBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam,",
    "void ShiftFilterEditChildren(HWND hwnd, int deltaY)\n{",
    new_groupbox,
    "theme-aware filter-edit group boxes")

new_config = r'''void ConfigureModernFilterEditControls(HWND hwnd)
{
    const bool dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark;
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

        if (lstrcmpiW(className, L"Button") == 0)
        {
            const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
            if ((style & 0x0F) == BS_GROUPBOX)
            {
                SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                                  kModernGroupBoxSubclassId, 0);
            }
            else if (!IsModernFilterEditButton(
                         static_cast<UINT>(GetDlgCtrlID(child))))
            {
                SetWindowTheme(child,
                               dark ? L"DarkMode_Explorer" : L"Explorer",
                               NULL);
            }
        }
        else if (lstrcmpiW(className, L"Edit") == 0 ||
                 lstrcmpiW(className, L"ComboBox") == 0 ||
                 lstrcmpiW(className, L"ListBox") == 0)
        {
            SetWindowTheme(child,
                           dark ? L"DarkMode_Explorer" : L"Explorer",
                           NULL);
        }
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

        SetWindowTheme(help,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SetWindowPos(help, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }
}

'''
replace_function(
    "void ConfigureModernFilterEditControls(HWND hwnd)\n{",
    "int ExpandFilterEditForHeader(HWND hwnd)\n{",
    new_config,
    "filter-edit child control theming")

new_paint = r'''void PaintModernFilterEditDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    HBRUSH bg = GetCurrentThemeWindowBrush();
    FillRect(hdc, &client, bg);

    const int header = FilterEditHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.textPrimary);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT titleRect = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Filter toevoegen/bewerken", -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, palette.textSecondary);
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Stel de filtervoorwaarden, meldingen en uitvoer voor dit filter in.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             palette.divider);
}

'''
replace_function(
    "void PaintModernFilterEditDialog(HWND hwnd, HDC hdc)\n{",
    "LRESULT CALLBACK FilterEditWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,",
    new_paint,
    "filter-edit themed Dutch header")

new_proc = r'''LRESULT CALLBACK FilterEditWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
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
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, palette.textPrimary);
            return reinterpret_cast<LRESULT>(GetCurrentThemeWindowBrush());
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, palette.textPrimary);
            SetBkColor(hdc, palette.controlBackground);
            return reinterpret_cast<LRESULT>(GetCurrentThemeControlBrush());
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

        case WM_THEMECHANGED:
            ConfigureModernFilterEditControls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
            RemovePropW(hwnd, L"PDW.ThemeAwareDialog");
            RemoveWindowSubclass(hwnd, FilterEditWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

'''
replace_function(
    "LRESULT CALLBACK FilterEditWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,",
    "void EnableModernFilterEditDialog(HWND hwnd)\n{",
    new_proc,
    "filter-edit themed control colors")

new_enable = r'''void EnableModernFilterEditDialog(HWND hwnd)
{
    if (!IsFilterEditDialog(hwnd)) return;

    SetPropW(hwnd, L"PDW.ThemeAwareDialog",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    SetWindowTheme(hwnd,
                   dark ? L"DarkMode_Explorer" : L"Explorer",
                   NULL);

    SetWindowSubclass(hwnd, FilterEditWindowSubclassProc,
                      kFilterEditWindowSubclassId, 0);
    ExpandFilterEditForHeader(hwnd);
    ConfigureModernFilterEditControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

'''
replace_function(
    "void EnableModernFilterEditDialog(HWND hwnd)\n{",
    "bool IsFilterOptionsDialog(HWND hwnd)\n{",
    new_enable,
    "filter-edit theme activation")

path.write_text(text, encoding="utf-8", newline="")
print("Applied theme-safe Dark/Light styling and Dutch header to Filter Edit dialog.")
