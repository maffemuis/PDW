from pathlib import Path

path = Path("utils/windows11_ui.cpp")
text = path.read_text(encoding="utf-8")


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


required = [
    "bool IsThemeAwareDialog(HWND hwnd)",
    "HBRUSH GetCurrentThemeWindowBrush()",
    "HBRUSH GetCurrentThemeControlBrush()",
    "L\"Filter toevoegen/bewerken\"",
]
for marker in required:
    if text.count(marker) != 1:
        raise RuntimeError(f"Filter Options theme dependency missing or ambiguous: {marker}")

new_config = r'''void ConfigureModernFilterOptionsControls(HWND hwnd)
{
    const bool dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark;
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

        if (lstrcmpiW(className, L"Button") == 0)
        {
            const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
            if ((style & 0x0F) == BS_GROUPBOX)
            {
                SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                                  kModernGroupBoxSubclassId, 0);
            }
            else if (!IsModernFilterOptionsButton(
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
}

'''
replace_function(
    "void ConfigureModernFilterOptionsControls(HWND hwnd)\n{",
    "int ExpandFilterOptionsForHeader(HWND hwnd)\n{",
    new_config,
    "Filter Options child control theming")

new_paint = r'''void PaintModernFilterOptionsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    FillRect(hdc, &client, GetCurrentThemeWindowBrush());

    const int header = FilterOptionsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.textPrimary);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT titleRect = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, L"Filteropties", -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, palette.textSecondary);
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc,
              L"Stel uitvoerbestanden, beschrijvingen en standaard filtergedrag in.",
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
    "void PaintModernFilterOptionsDialog(HWND hwnd, HDC hdc)\n{",
    "LRESULT CALLBACK FilterOptionsWindowSubclassProc(HWND hwnd, UINT message,",
    new_paint,
    "Filter Options Dutch themed header")

new_proc = r'''LRESULT CALLBACK FilterOptionsWindowSubclassProc(HWND hwnd, UINT message,
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
                IsModernFilterOptionsButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_THEMECHANGED:
            ConfigureModernFilterOptionsControls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.FilterOptions.HeaderOffset");
            RemovePropW(hwnd, L"PDW.ThemeAwareDialog");
            RemoveWindowSubclass(hwnd, FilterOptionsWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

'''
replace_function(
    "LRESULT CALLBACK FilterOptionsWindowSubclassProc(HWND hwnd, UINT message,",
    "void EnableModernFilterOptionsDialog(HWND hwnd)\n{",
    new_proc,
    "Filter Options themed control colors")

new_enable = r'''void EnableModernFilterOptionsDialog(HWND hwnd)
{
    if (!IsFilterOptionsDialog(hwnd)) return;

    SetPropW(hwnd, L"PDW.ThemeAwareDialog",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    SetWindowTheme(hwnd,
                   dark ? L"DarkMode_Explorer" : L"Explorer",
                   NULL);

    SetWindowSubclass(hwnd, FilterOptionsWindowSubclassProc,
                      kFilterOptionsWindowSubclassId, 0);
    ExpandFilterOptionsForHeader(hwnd);
    ConfigureModernFilterOptionsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

'''
replace_function(
    "void EnableModernFilterOptionsDialog(HWND hwnd)\n{",
    "bool IsFilterFindDialog(HWND hwnd)\n{",
    new_enable,
    "Filter Options theme activation")

path.write_text(text, encoding="utf-8", newline="")
print("Applied Dark/Light styling and Dutch header to Filter Options dialog.")
