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
    "L\"Filteropties\"",
]
for marker in required:
    if text.count(marker) != 1:
        raise RuntimeError(f"Filter Find theme dependency missing or ambiguous: {marker}")

new_config = r'''void ConfigureModernFilterFindControls(HWND hwnd)
{
    EnumChildWindows(hwnd, HideLegacyFilterFindLabels, 0);
    const bool dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark;

    HWND edit = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (edit)
    {
        LONG_PTR style = GetWindowLongPtr(edit, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(edit, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(edit, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(edit, GWL_EXSTYLE, exStyle);
        SetWindowTheme(edit,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SendMessage(edit, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(edit, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    HWND caseSensitive = GetDlgItem(hwnd, IDC_FILTERFIND_CASE);
    if (caseSensitive)
    {
        SetWindowTheme(caseSensitive,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SendMessage(caseSensitive, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
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
        SetWindowTextW(close, L"Sluiten");
    }
}

'''
replace_function(
    "void ConfigureModernFilterFindControls(HWND hwnd)\n{",
    "void ResizeModernFilterFindDialog(HWND hwnd)\n{",
    new_config,
    "Filter Find child controls")

new_paint = r'''void PaintModernFilterFindDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    FillRect(hdc, &client, GetCurrentThemeWindowBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int editTop = header + ScaleForDpi(hwnd, 28);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.textPrimary);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Filter zoeken", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, palette.textSecondary);
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc, L"Zoek op adres, berichttekst of label.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             palette.divider);

    SetTextColor(hdc, palette.textPrimary);
    oldFont = SelectObject(hdc, GetHeaderFont());
    RECT findLabel = { margin, header + ScaleForDpi(hwnd, 5),
                       client.right - margin, editTop - ScaleForDpi(hwnd, 3) };
    DrawTextW(hdc, L"Zoeken", -1, &findLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT hitsLabel = { margin,
                       editTop + ScaleForDpi(hwnd, 44),
                       margin + ScaleForDpi(hwnd, 62),
                       editTop + ScaleForDpi(hwnd, 68) };
    DrawTextW(hdc, L"Treffers", -1, &hitsLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    RECT editCard = { margin - 1, editTop - 1,
                      client.right - margin + 1,
                      editTop + ScaleForDpi(hwnd, 31) };
    FillRoundedRect(hdc, editCard,
                    palette.controlBackground, palette.border,
                    ScaleForDpi(hwnd, 8));
}

'''
replace_function(
    "void PaintModernFilterFindDialog(HWND hwnd, HDC hdc)\n{",
    "LRESULT CALLBACK FilterFindWindowSubclassProc(HWND hwnd, UINT message,",
    new_paint,
    "Filter Find themed Dutch paint")

new_proc = r'''LRESULT CALLBACK FilterFindWindowSubclassProc(HWND hwnd, UINT message,
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
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, palette.textPrimary);
            return reinterpret_cast<LRESULT>(GetCurrentThemeWindowBrush());
        }

        case WM_CTLCOLOREDIT:
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
            if (item && item->CtlType == ODT_BUTTON && item->CtlID == IDCANCEL)
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_THEMECHANGED:
            ConfigureModernFilterFindControls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.ThemeAwareDialog");
            RemoveWindowSubclass(hwnd, FilterFindWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

'''
replace_function(
    "LRESULT CALLBACK FilterFindWindowSubclassProc(HWND hwnd, UINT message,",
    "void EnableModernFilterFindDialog(HWND hwnd)\n{",
    new_proc,
    "Filter Find themed control colors")

new_enable = r'''void EnableModernFilterFindDialog(HWND hwnd)
{
    if (!IsFilterFindDialog(hwnd)) return;

    SetPropW(hwnd, L"PDW.ThemeAwareDialog",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    SetWindowTheme(hwnd,
                   dark ? L"DarkMode_Explorer" : L"Explorer",
                   NULL);

    SetWindowSubclass(hwnd, FilterFindWindowSubclassProc,
                      kFilterFindWindowSubclassId, 0);
    ResizeModernFilterFindDialog(hwnd);
    ConfigureModernFilterFindControls(hwnd);
    LayoutModernFilterFindDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

'''
replace_function(
    "void EnableModernFilterFindDialog(HWND hwnd)\n{",
    "bool IsFilterDuplicateDialog(HWND hwnd)\n{",
    new_enable,
    "Filter Find theme activation")

path.write_text(text, encoding="utf-8", newline="")
print("Applied Dark/Light styling and Dutch localization to Filter Find dialog.")
