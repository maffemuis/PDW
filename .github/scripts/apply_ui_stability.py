from pathlib import Path

ui_path = Path("utils/windows11_ui.cpp")
theme_path = Path("utils/ui_theme.cpp")
ui = ui_path.read_text(encoding="utf-8")
theme = theme_path.read_text(encoding="utf-8")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly 1 match, got {count}")
    return text.replace(old, new, 1)


def replace_function(text, start_marker, next_marker, replacement, label):
    start = text.find(start_marker)
    if start < 0:
        raise RuntimeError(f"{label}: start marker missing")
    end = text.find(next_marker, start)
    if end < 0:
        raise RuntimeError(f"{label}: end marker missing")
    if text.find(start_marker, start + 1) >= 0:
        raise RuntimeError(f"{label}: start marker is not unique")
    return text[:start] + replacement + text[end:]


old_light = '''const ThemePalette kLight = {\n    RGB(238, 243, 248), // windowBackground\n    RGB(231, 238, 245), // workspaceBackground\n    RGB(238, 244, 249), // shellBackground\n    RGB(246, 249, 252), // controlBackground\n    RGB(235, 243, 250), // controlHover\n    RGB(248, 251, 253), // cardBackground\n    RGB(236, 243, 249), // cardHeaderBackground\n    RGB(247, 250, 252), // paneBackground\n    RGB(240, 246, 250), // paneAlternateBackground\n    RGB(203, 214, 224), // border\n    RGB(216, 224, 233), // divider'''
new_light = '''const ThemePalette kLight = {\n    RGB(232, 238, 244), // windowBackground\n    RGB(224, 232, 240), // workspaceBackground\n    RGB(232, 239, 245), // shellBackground\n    RGB(239, 244, 248), // controlBackground\n    RGB(228, 237, 245), // controlHover\n    RGB(241, 246, 249), // cardBackground\n    RGB(230, 238, 244), // cardHeaderBackground\n    RGB(240, 245, 248), // paneBackground\n    RGB(233, 240, 245), // paneAlternateBackground\n    RGB(195, 207, 218), // border\n    RGB(207, 217, 226), // divider'''
theme = replace_once(theme, old_light, new_light, "muted light palette")

old_workspace = '''void DrawModernWorkspace(HWND hwnd)\n{\n    RECT client = {};\n    GetClientRect(hwnd, &client);\n    const int width = client.right - client.left;\n    const int height = client.bottom - client.top;\n    if (width <= 0 || height <= 0) return;\n\n    HDC target = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_CLIPCHILDREN);\n    if (!target) target = GetDC(hwnd);\n    if (!target) return;\n\n    HDC buffer = CreateCompatibleDC(target);\n    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;\n    HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;\n    HDC hdc = (buffer && bitmap) ? buffer : target;\n\n    const COLORREF workspace = pdw::CurrentThemePalette().workspaceBackground;\n    HBRUSH background = CreateSolidBrush(workspace);\n    FillRect(hdc, &client, background);\n    DeleteObject(background);\n\n    DrawTopNavigation(hdc, hwnd, client);\n    DrawCommandStrip(hdc, hwnd, client);\n    DrawCard(hdc, hwnd, g_pane1Card, g_pane1Body, L"Monitored messages", true);\n    DrawCard(hdc, hwnd, g_pane2Card, g_pane2Body, L"Filtered messages", false);\n    DrawStatusBar(hdc, hwnd);\n\n    if (buffer && bitmap)\n    {\n        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);\n        SelectObject(buffer, oldBitmap);\n        DeleteObject(bitmap);\n        DeleteDC(buffer);\n    }\n    else if (buffer)\n    {\n        DeleteDC(buffer);\n    }\n\n    ReleaseDC(hwnd, target);\n}\n\n'''
new_workspace = r'''void PaintModernWorkspace(HWND hwnd, HDC target, const RECT* paintRect)
{
    if (!target) return;

    RECT client = {};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    const int savedTarget = SaveDC(target);
    if (paintRect)
        IntersectClipRect(target, paintRect->left, paintRect->top,
                          paintRect->right, paintRect->bottom);

    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;
    HDC hdc = (buffer && bitmap) ? buffer : target;

    const COLORREF workspace = pdw::CurrentThemePalette().workspaceBackground;
    HBRUSH background = CreateSolidBrush(workspace);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    DrawTopNavigation(hdc, hwnd, client);
    DrawCommandStrip(hdc, hwnd, client);
    DrawCard(hdc, hwnd, g_pane1Card, g_pane1Body, L"Monitored messages", true);
    DrawCard(hdc, hwnd, g_pane2Card, g_pane2Body, L"Filtered messages", false);
    DrawStatusBar(hdc, hwnd);

    if (buffer && bitmap)
    {
        RECT copy = client;
        if (paintRect)
        {
            RECT clipped = {};
            if (IntersectRect(&clipped, &client, paintRect)) copy = clipped;
            else SetRectEmpty(&copy);
        }
        if (!IsRectEmpty(&copy))
            BitBlt(target, copy.left, copy.top,
                   copy.right - copy.left, copy.bottom - copy.top,
                   buffer, copy.left, copy.top, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    }
    else if (buffer)
    {
        DeleteDC(buffer);
    }

    RestoreDC(target, savedTarget);
}

void DrawModernWorkspace(HWND hwnd)
{
    if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

void InvalidateModernShell(HWND hwnd)
{
    if (!hwnd) return;
    RECT client = {};
    GetClientRect(hwnd, &client);
    RECT shell = { 0, 0, client.right, ShellHeight(hwnd) };
    InvalidateRect(hwnd, &shell, FALSE);
}

void InvalidateModernRx(HWND hwnd)
{
    if (!hwnd || IsRectEmpty(&g_pane1Card)) return;
    RECT rx = g_pane1Card;
    rx.bottom = min(rx.bottom, rx.top + CardTitleHeight(hwnd) + 1);
    InvalidateRect(hwnd, &rx, FALSE);
}

void InvalidateModernStatus(HWND hwnd)
{
    if (!hwnd || IsRectEmpty(&g_statusRect)) return;
    InvalidateRect(hwnd, &g_statusRect, FALSE);
}

'''
ui = replace_once(ui, old_workspace, new_workspace, "WM_PAINT-buffered workspace renderer")

ui = replace_once(
    ui,
    '    const bool rxActive = !bPauseFlag && dRX_Quality > 0.0;\n',
    '    const bool rxActive = !bPauseFlag && si_index > 0;\n',
    "status RX state uses live signal")
ui = replace_once(
    ui,
    '    DrawTextW(hdc, L"RX-Q", -1, &rxText,\n',
    '    DrawTextW(hdc, L"RX", -1, &rxText,\n',
    "status RX label")

ui = replace_once(
    ui,
    '        if (rect.top < ScaleForDpi(hwnd, 70))\n            ShowWindow(child, SW_HIDE);\n',
    '        if (rect.top < ScaleForDpi(hwnd, 80))\n            ShowWindow(child, SW_HIDE);\n',
    "hide full legacy Filters header")

new_filter_edit_config = r'''void ConfigureModernFilterEditControls(HWND hwnd)
{
    const bool dark = pdw::CurrentUiTheme() == pdw::UiTheme::Dark;
    SetWindowTextW(hwnd, L"PDW - Filter toevoegen/bewerken");

    struct LabelUpdate { int id; const wchar_t* text; };
    const LabelUpdate labels[] = {
        { IDC_FILTERREJECT, L"Weigeren" },
        { IDC_FILTERMATCHEXACT, L"Exact bericht" },
        { IDC_FILTERLABELEN, L"Filterlabel tonen" },
        { IDC_FILTER_MONITOR_ONLY, L"Alleen monitor" },
        { IDC_FILTERCMD, L"Opdrachtbestand inschakelen" },
        { IDC_FILTERSMTP, L"E-mail verzenden (uitgeschakeld)" },
        { IDC_FILTERRESET, L"Standaard" },
        { IDC_HITCOUNTER_BOX, L"Trefferteller" },
        { IDC_SEPFILTERFILEEN, L"Afzonderlijk filterbestand" },
        { IDC_SEPFILTERFILEBROWSE1, L"Bladeren..." },
        { IDC_SEPFILTERFILEBROWSE2, L"Bladeren..." },
        { IDC_SEPFILTERFILEBROWSE3, L"Bladeren..." },
        { IDC_FILTER_APPLY, L"Toepassen" },
        { IDCANCEL, L"Annuleren" },
        { IDC_DONTCHANGE, L"LET OP: grijs betekent 'Niet wijzigen'" }
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(labels)); ++i)
    {
        HWND control = GetDlgItem(hwnd, labels[i].id);
        if (control) SetWindowTextW(control, labels[i].text);
    }

    HWND hits = GetDlgItem(hwnd, IDC_FILTERHITS);
    if (hits)
    {
        wchar_t value[128] = {};
        GetWindowTextW(hits, value, ARRAYSIZE(value));
        if (wcsncmp(value, L"Number of hits:", 15) == 0)
        {
            const wchar_t* number = value + 15;
            while (*number == L' ') ++number;
            wchar_t translated[128] = {};
            swprintf(translated, ARRAYSIZE(translated), L"Aantal treffers: %s", number);
            SetWindowTextW(hits, translated);
        }
    }

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
        else if (lstrcmpiW(className, L"Static") == 0)
        {
            wchar_t text[96] = {};
            GetWindowTextW(child, text, ARRAYSIZE(text));
            if (lstrcmpW(text, L"Filter type") == 0)
                SetWindowTextW(child, L"Filtertype");
            else if (lstrcmpW(text, L"Address") == 0)
                SetWindowTextW(child, L"Adres");
            else if (lstrcmpW(text, L"Color") == 0)
                SetWindowTextW(child, L"Kleur");
            else if (lstrcmpW(text, L"Text") == 0)
                SetWindowTextW(child, L"Tekst");
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

        SetWindowTextW(help,
                       L"Stel de filtervoorwaarden in; grijze velden worden niet gewijzigd.");
        SetWindowTheme(help,
                       dark ? L"DarkMode_Explorer" : L"Explorer",
                       NULL);
        SetWindowPos(help, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }
}

void LayoutModernFilterEditAcceptance(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int margin = ScaleForDpi(hwnd, 14);
    const int gap = ScaleForDpi(hwnd, 8);

    HWND sepBox = GetDlgItem(hwnd, IDC_SEPFILTERBOX);
    RECT box = {};
    if (sepBox && GetWindowRect(sepBox, &box))
    {
        MapWindowPoints(NULL, hwnd, reinterpret_cast<POINT*>(&box), 2);
        box.left = margin;
        box.right = client.right - margin;
        box.bottom = box.top + ScaleForDpi(hwnd, 118);
        MoveWindow(sepBox, box.left, box.top,
                   max(1, box.right - box.left),
                   max(1, box.bottom - box.top), TRUE);

        HWND enable = GetDlgItem(hwnd, IDC_SEPFILTERFILEEN);
        if (enable)
            MoveWindow(enable, box.left + ScaleForDpi(hwnd, 12),
                       box.top + ScaleForDpi(hwnd, 9),
                       max(1, box.right - box.left - ScaleForDpi(hwnd, 24)),
                       ScaleForDpi(hwnd, 22), TRUE);

        const int browseIds[] = {
            IDC_SEPFILTERFILEBROWSE1, IDC_SEPFILTERFILEBROWSE2,
            IDC_SEPFILTERFILEBROWSE3
        };
        const int editIds[] = {
            IDC_SEPFILTERFILE1, IDC_SEPFILTERFILE2, IDC_SEPFILTERFILE3
        };
        const int browseWidth = ScaleForDpi(hwnd, 84);
        const int rowHeight = ScaleForDpi(hwnd, 22);
        for (int i = 0; i < 3; ++i)
        {
            const int y = box.top + ScaleForDpi(hwnd, 34) +
                          i * ScaleForDpi(hwnd, 25);
            HWND browse = GetDlgItem(hwnd, browseIds[i]);
            HWND edit = GetDlgItem(hwnd, editIds[i]);
            if (browse)
                MoveWindow(browse, box.left + ScaleForDpi(hwnd, 12), y,
                           browseWidth, rowHeight, TRUE);
            if (edit)
            {
                const int editLeft = box.left + ScaleForDpi(hwnd, 12) +
                                     browseWidth + gap;
                MoveWindow(edit, editLeft, y,
                           max(1, box.right - ScaleForDpi(hwnd, 12) - editLeft),
                           rowHeight, TRUE);
            }
        }
    }

    const int buttonHeight = ScaleForDpi(hwnd, 32);
    const int helpHeight = ScaleForDpi(hwnd, 22);
    const int helpY = client.bottom - margin - helpHeight;
    const int buttonY = helpY - gap - buttonHeight;
    const int widths[] = {
        ScaleForDpi(hwnd, 66), ScaleForDpi(hwnd, 36), ScaleForDpi(hwnd, 88),
        ScaleForDpi(hwnd, 36), ScaleForDpi(hwnd, 88)
    };
    const int ids[] = { IDOK, IDC_FILTER_PREVIOUS, IDC_FILTER_APPLY,
                        IDC_FILTER_NEXT, IDCANCEL };
    int totalWidth = 4 * gap;
    for (int i = 0; i < 5; ++i) totalWidth += widths[i];
    int x = max(margin, (client.right - totalWidth) / 2);
    for (int i = 0; i < 5; ++i)
    {
        HWND button = GetDlgItem(hwnd, ids[i]);
        if (button) MoveWindow(button, x, buttonY, widths[i], buttonHeight, TRUE);
        x += widths[i] + gap;
    }

    HWND help = GetDlgItem(hwnd, IDC_FILTEREDITHELP);
    if (help)
        MoveWindow(help, margin, helpY,
                   max(1, client.right - 2 * margin), helpHeight, TRUE);
}

'''
ui = replace_function(
    ui,
    "void ConfigureModernFilterEditControls(HWND hwnd)\n{",
    "int ExpandFilterEditForHeader(HWND hwnd)\n{",
    new_filter_edit_config,
    "Filter Edit Dutch labels and DPI layout helper")

new_expand = r'''int ExpandFilterEditForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
    if (existing)
    {
        const INT_PTR stored = reinterpret_cast<INT_PTR>(existing);
        return stored > 0 ? static_cast<int>(stored - 1) : 0;
    }

    RECT window = {};
    GetWindowRect(hwnd, &window);
    const int currentWidth = window.right - window.left;
    const int currentHeight = window.bottom - window.top;
    const int header = ScaleForDpi(hwnd, 58);
    const int extraBottom = ScaleForDpi(hwnd, 72);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    int applied = 0;

    if (GetMonitorInfo(monitor, &info))
    {
        const int workWidth = info.rcWork.right - info.rcWork.left;
        const int workHeight = info.rcWork.bottom - info.rcWork.top;
        int targetWidth = max(currentWidth, ScaleForDpi(hwnd, 460));
        int targetHeight = currentHeight + header + extraBottom;
        targetWidth = min(targetWidth, workWidth);
        targetHeight = min(targetHeight, workHeight);

        if (targetHeight >= currentHeight + header)
        {
            int x = window.left - (targetWidth - currentWidth) / 2;
            int y = window.top - header / 2;
            if (x < info.rcWork.left) x = info.rcWork.left;
            if (x + targetWidth > info.rcWork.right) x = info.rcWork.right - targetWidth;
            if (y < info.rcWork.top) y = info.rcWork.top;
            if (y + targetHeight > info.rcWork.bottom)
                y = info.rcWork.bottom - targetHeight;

            SetWindowPos(hwnd, NULL, x, y, targetWidth, targetHeight,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            ShiftFilterEditChildren(hwnd, header);
            applied = header;
        }
    }

    SetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

'''
ui = replace_function(
    ui,
    "int ExpandFilterEditForHeader(HWND hwnd)\n{",
    "int FilterEditHeaderOffset(HWND hwnd)\n{",
    new_expand,
    "Filter Edit DPI-safe expansion")

ui = replace_once(
    ui,
    '''        case WM_THEMECHANGED:\n            ConfigureModernFilterEditControls(hwnd);\n            InvalidateRect(hwnd, NULL, TRUE);\n            break;''',
    '''        case WM_THEMECHANGED:\n            ConfigureModernFilterEditControls(hwnd);\n            LayoutModernFilterEditAcceptance(hwnd);\n            InvalidateRect(hwnd, NULL, TRUE);\n            break;''',
    "Filter Edit theme relayout")

ui = replace_once(
    ui,
    '''    ExpandFilterEditForHeader(hwnd);\n    ConfigureModernFilterEditControls(hwnd);\n    InvalidateRect(hwnd, NULL, TRUE);''',
    '''    ExpandFilterEditForHeader(hwnd);\n    ConfigureModernFilterEditControls(hwnd);\n    LayoutModernFilterEditAcceptance(hwnd);\n    InvalidateRect(hwnd, NULL, TRUE);''',
    "Filter Edit initial relayout")

ui = replace_once(
    ui,
    '''    if (message == WM_ERASEBKGND)\n        return 1;\n\n    if (message == WM_GETMINMAXINFO)''',
    '''    if (message == WM_ERASEBKGND)\n        return 1;\n\n    if (message == WM_PAINT)\n    {\n        PAINTSTRUCT ps = {};\n        HDC hdc = BeginPaint(hwnd, &ps);\n        PaintModernWorkspace(hwnd, hdc, &ps.rcPaint);\n        EndPaint(hwnd, &ps);\n        return 0;\n    }\n\n    if (message == WM_GETMINMAXINFO)''',
    "main WM_PAINT interception")

ui = replace_once(
    ui,
    '''                if (target == 5 && g_settingsFlyout)\n                {\n                    CloseSettingsFlyout();\n                    g_pressedTarget = -1;\n                    DrawModernWorkspace(hwnd);\n                    return 0;\n                }''',
    '''                if (target == 5 && g_settingsFlyout)\n                {\n                    CloseSettingsFlyout();\n                    g_pressedTarget = -1;\n                    InvalidateModernShell(hwnd);\n                    return 0;\n                }''',
    "bounded shell press close")
ui = replace_once(
    ui,
    '''                g_pressedTarget = target;\n                SetCapture(hwnd);\n                DrawModernWorkspace(hwnd);\n                return 0;''',
    '''                g_pressedTarget = target;\n                SetCapture(hwnd);\n                InvalidateModernShell(hwnd);\n                return 0;''',
    "bounded shell press")
ui = replace_once(
    ui,
    '''        if (GetCapture() == hwnd) ReleaseCapture();\n        DrawModernWorkspace(hwnd);\n        if (target == pressed)''',
    '''        if (GetCapture() == hwnd) ReleaseCapture();\n        InvalidateModernShell(hwnd);\n        if (target == pressed)''',
    "bounded shell release")
ui = replace_once(
    ui,
    '''        if (hover != g_hoverTarget)\n        {\n            g_hoverTarget = hover;\n            DrawModernWorkspace(hwnd);\n        }''',
    '''        if (hover != g_hoverTarget)\n        {\n            g_hoverTarget = hover;\n            InvalidateModernShell(hwnd);\n        }''',
    "bounded shell hover")
ui = replace_once(
    ui,
    '''    if (message == WM_MOUSELEAVE)\n    {\n        g_hoverTarget = -1;\n        DrawModernWorkspace(hwnd);\n    }''',
    '''    if (message == WM_MOUSELEAVE)\n    {\n        g_hoverTarget = -1;\n        InvalidateModernShell(hwnd);\n    }''',
    "bounded shell leave")

ui = replace_once(
    ui,
    '''        case WM_PAINT:\n        case WM_NOTIFY:\n            DrawModernWorkspace(hwnd);\n            break;\n\n        case WM_TIMER:\n            // RX quality changes at one-second cadence, while the original\n            // signal indicator state changes on the 100 ms decoder timer.\n            if (wParam == kLegacySecondTimer || wParam == kLegacyDecodeTimer)\n                DrawModernWorkspace(hwnd);\n            break;''',
    '''        case WM_NOTIFY:\n            DrawModernWorkspace(hwnd);\n            break;\n\n        case WM_TIMER:\n            // The 100 ms decoder timer already updates PDW's original\n            // si_index signal state. Repaint only the receive header here;\n            // no decoder I/O is performed by the modern presentation layer.\n            if (wParam == kLegacyDecodeTimer)\n                InvalidateModernRx(hwnd);\n            else if (wParam == kLegacySecondTimer)\n            {\n                InvalidateModernRx(hwnd);\n                InvalidateModernStatus(hwnd);\n            }\n            break;''',
    "bounded timer invalidation")

ui_path.write_text(ui, encoding="utf-8", newline="")
theme_path.write_text(theme, encoding="utf-8", newline="")
print("Applied bounded UI stability, live RX presentation, muted Light palette, and Filter acceptance fixes.")
