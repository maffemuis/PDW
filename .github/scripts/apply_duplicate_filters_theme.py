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
    "L\"Filter zoeken\"",
]
for marker in required:
    if text.count(marker) != 1:
        raise RuntimeError(f"Duplicate Filter theme dependency missing or ambiguous: {marker}")

new_config = r'''void ConfigureModernFilterDuplicateControls(HWND hwnd)
{
    const bool dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark;
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();

    HWND results = GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE);
    if (results)
    {
        LONG_PTR style = GetWindowLongPtr(results, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(results, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(results, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(results, GWL_EXSTYLE, exStyle);
        SetWindowTheme(results,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SendMessage(results, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(results, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    HWND progress = GetDlgItem(hwnd, IDC_PROGRESS1);
    if (progress)
    {
        SetWindowTheme(progress,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SendMessage(progress, PBM_SETBKCOLOR, 0,
                    static_cast<LPARAM>(palette.cardBackground));
        SendMessage(progress, PBM_SETBARCOLOR, 0,
                    static_cast<LPARAM>(palette.accent));
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
    if (find) SetWindowTextW(find, L"Duplicaten zoeken");
    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close) SetWindowTextW(close, L"Sluiten");
}

'''
replace_function(
    "void ConfigureModernFilterDuplicateControls(HWND hwnd)\n{",
    "void PaintModernFilterDuplicateDialog(HWND hwnd, HDC hdc)\n{",
    new_config,
    "Duplicate Filter controls")

new_paint = r'''void PaintModernFilterDuplicateDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
    FillRect(hdc, &client, GetCurrentThemeWindowBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int listTop = header + ScaleForDpi(hwnd, 18);
    const int listHeight = ScaleForDpi(hwnd, 88);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, palette.textPrimary);
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Dubbele filters zoeken", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, palette.textSecondary);
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc,
              L"Zoek filters met hetzelfde type, adres en dezelfde berichttekst.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             palette.divider);

    RECT resultsCard = {
        margin - 1,
        listTop - 1,
        client.right - margin + 1,
        listTop + listHeight + 1
    };
    FillRoundedRect(hdc, resultsCard,
                    palette.controlBackground, palette.border,
                    ScaleForDpi(hwnd, 8));
}

'''
replace_function(
    "void PaintModernFilterDuplicateDialog(HWND hwnd, HDC hdc)\n{",
    "LRESULT CALLBACK FilterDuplicateWindowSubclassProc(HWND hwnd, UINT message,",
    new_paint,
    "Duplicate Filter themed Dutch paint")

new_proc = r'''LRESULT CALLBACK FilterDuplicateWindowSubclassProc(HWND hwnd, UINT message,
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
            const pdw::ThemePalette& palette = pdw::CurrentThemePalette();
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, palette.textPrimary);
            return reinterpret_cast<LRESULT>(GetCurrentThemeWindowBrush());
        }

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
                (item->CtlID == IDOK || item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_THEMECHANGED:
            ConfigureModernFilterDuplicateControls(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            break;

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.ThemeAwareDialog");
            RemoveWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                                 subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

'''
replace_function(
    "LRESULT CALLBACK FilterDuplicateWindowSubclassProc(HWND hwnd, UINT message,",
    "void EnableModernFilterDuplicateDialog(HWND hwnd)\n{",
    new_proc,
    "Duplicate Filter themed control colors")

new_enable = r'''void EnableModernFilterDuplicateDialog(HWND hwnd)
{
    if (!IsFilterDuplicateDialog(hwnd)) return;

    SetPropW(hwnd, L"PDW.ThemeAwareDialog",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1)));
    const BOOL dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark));
    SetWindowTheme(hwnd,
                   dark ? L"DarkMode_Explorer" : L"Explorer",
                   NULL);

    SetWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                      kFilterDuplicateWindowSubclassId, 0);
    ResizeModernFilterDuplicateDialog(hwnd);
    ConfigureModernFilterDuplicateControls(hwnd);
    LayoutModernFilterDuplicateDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

'''
replace_function(
    "void EnableModernFilterDuplicateDialog(HWND hwnd)\n{",
    "bool IsOptionsDialog(HWND hwnd)\n{",
    new_enable,
    "Duplicate Filter theme activation")

path.write_text(text, encoding="utf-8", newline="")
print("Applied Dark/Light styling and Dutch localization to Duplicate Filters dialog.")
