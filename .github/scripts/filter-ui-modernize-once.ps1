$ErrorActionPreference = 'Stop'

$path = 'utils/windows11_ui.cpp'
$text = [System.IO.File]::ReadAllText($path)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) {
        throw "Expected source fragment not found: $label"
    }
    $script:text = $script:text.Replace($old, $new)
}

function Replace-RegexOnce([string]$pattern, [string]$replacement, [string]$label) {
    $regex = [regex]::new($pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)
    $matches = $regex.Matches($script:text)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one source fragment for $label; found $($matches.Count)"
    }
    $script:text = $regex.Replace($script:text, $replacement, 1)
}

Replace-RegexOnce 'HFONT g_iconFont = NULL;\r?\n' "HFONT g_iconFont = NULL;`r`nHFONT g_filterListFont = NULL;`r`n" 'filter list font global'

$oldIcon = @'
HFONT GetIconFont()
{
    if (g_iconFont) return g_iconFont;
    g_iconFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    return g_iconFont;
}
'@
$newIcon = @'
HFONT GetIconFont()
{
    if (g_iconFont) return g_iconFont;
    g_iconFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    return g_iconFont;
}

HFONT GetFilterListFont()
{
    if (g_filterListFont) return g_filterListFont;
    g_filterListFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   FIXED_PITCH | FF_MODERN, L"Consolas");
    return g_filterListFont ? g_filterListFont : GetDialogFont();
}
'@
Replace-Exact $oldIcon $newIcon 'filter list font factory'

$filterSurface = @'
void ConfigureModernFilterControls(HWND hwnd)
{
    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    if (list)
    {
        LONG_PTR style = GetWindowLongPtr(list, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(list, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(list, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(list, GWL_EXSTYLE, exStyle);

        SetWindowTheme(list, L"Explorer", NULL);
        ListView_SetExtendedListViewStyleEx(
            list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessage(list, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);
        if (!Profile.FilterWindowColors)
        {
            SendMessage(list, LVM_SETBKCOLOR, 0, RGB(255, 255, 255));
            SendMessage(list, LVM_SETTEXTBKCOLOR, 0, CLR_NONE);
        }
        SetWindowPos(list, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    const int ids[] = {
        IDC_FILTERADD, IDC_FILTEREDIT, IDC_FILTERDEL,
        IDC_FILTEROPTIONS, IDC_FILTERFIND, IDOK
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(ids)); ++i)
    {
        HWND button = GetDlgItem(hwnd, ids[i]);
        if (!button) continue;
        LONG_PTR style = GetWindowLongPtr(button, GWL_STYLE);
        style = (style & ~static_cast<LONG_PTR>(0x0F)) | BS_OWNERDRAW;
        SetWindowLongPtr(button, GWL_STYLE, style);
        SetWindowTheme(button, L"", L"");
        SendMessage(button, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    }
}

void LayoutFilterDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int cx = client.right;
    const int cy = client.bottom;
    if (cx <= 0 || cy <= 0) return;

    const int margin = ScaleForDpi(hwnd, 16);
    const int gap = ScaleForDpi(hwnd, 8);
    const int headerHeight = ScaleForDpi(hwnd, 76);
    const int footerHeight = ScaleForDpi(hwnd, 60);
    const int buttonHeight = ScaleForDpi(hwnd, 36);
    const int buttonWidth = ScaleForDpi(hwnd, 94);
    const int buttonY = cy - margin - buttonHeight;

    HWND list = GetDlgItem(hwnd, IDC_FILTERS);
    if (list)
    {
        const int listBottom = cy - footerHeight;
        MoveWindow(list, margin, headerHeight,
                   max(1, cx - 2 * margin),
                   max(1, listBottom - headerHeight), TRUE);
        ListView_SetColumnWidth(list, 0,
            max(100, cx - 2 * margin - ScaleForDpi(hwnd, 8)));
    }

    int x = margin;
    const int leftIds[] = { IDC_FILTERADD, IDC_FILTEREDIT, IDC_FILTERDEL };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(leftIds)); ++i)
    {
        HWND button = GetDlgItem(hwnd, leftIds[i]);
        if (button)
            MoveWindow(button, x, buttonY, buttonWidth, buttonHeight, TRUE);
        x += buttonWidth + gap;
    }

    x = cx - margin - buttonWidth;
    HWND ok = GetDlgItem(hwnd, IDOK);
    if (ok) MoveWindow(ok, x, buttonY, buttonWidth, buttonHeight, TRUE);
    x -= buttonWidth + gap;

    HWND find = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (find) MoveWindow(find, x, buttonY, buttonWidth, buttonHeight, TRUE);
    x -= buttonWidth + gap;

    HWND options = GetDlgItem(hwnd, IDC_FILTEROPTIONS);
    if (options) MoveWindow(options, x, buttonY, buttonWidth, buttonHeight, TRUE);
}

bool IsModernFilterButton(UINT controlId)
{
    switch (controlId)
    {
        case IDC_FILTERADD:
        case IDC_FILTEREDIT:
        case IDC_FILTERDEL:
        case IDC_FILTEROPTIONS:
        case IDC_FILTERFIND:
        case IDOK:
            return true;
        default:
            return false;
    }
}

void DrawModernFilterButton(const DRAWITEMSTRUCT* item)
{
    if (!item || !item->hwndItem) return;

    RECT rect = item->rcItem;
    HBRUSH clear = CreateSolidBrush(RGB(246, 249, 252));
    FillRect(item->hDC, &rect, clear);
    DeleteObject(clear);

    const bool enabled = (item->itemState & ODS_DISABLED) == 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool primary = item->CtlID == IDC_FILTERADD || item->CtlID == IDOK;

    COLORREF fill = RGB(255, 255, 255);
    COLORREF border = RGB(205, 215, 226);
    COLORREF text = RGB(35, 43, 52);
    if (!enabled)
    {
        fill = RGB(244, 246, 248);
        border = RGB(226, 231, 236);
        text = RGB(150, 157, 165);
    }
    else if (primary)
    {
        fill = pressed ? RGB(0, 95, 184) : RGB(0, 120, 212);
        border = fill;
        text = RGB(255, 255, 255);
    }
    else if (pressed)
    {
        fill = RGB(232, 240, 248);
        border = RGB(179, 201, 224);
    }

    FillRoundedRect(item->hDC, rect, fill, border,
                    ScaleForDpi(item->hwndItem, 10));

    wchar_t label[64] = {};
    GetWindowTextW(item->hwndItem, label, ARRAYSIZE(label));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text);
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

void PaintModernFilterDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const COLORREF background = RGB(246, 249, 252);
    const COLORREF foreground = RGB(24, 39, 58);
    const COLORREF secondary = RGB(91, 103, 116);
    const COLORREF divider = RGB(216, 224, 233);
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
    DrawTextW(hdc, L"Manage address and message matching rules.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    wchar_t countText[64] = {};
    swprintf(countText, ARRAYSIZE(countText), L"%u filters",
             static_cast<unsigned int>(Profile.filters.size()));
    RECT badge = { client.right - margin - ScaleForDpi(hwnd, 112),
                   ScaleForDpi(hwnd, 22), client.right - margin,
                   ScaleForDpi(hwnd, 52) };
    FillRoundedRect(hdc, badge, RGB(232, 242, 252), RGB(205, 224, 242),
                    ScaleForDpi(hwnd, 16));
    SetTextColor(hdc, RGB(0, 95, 160));
    oldFont = SelectObject(hdc, GetHeaderFont());
    DrawTextW(hdc, countText, -1, &badge,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, headerHeight - 1,
             client.right - margin, headerHeight - 1, divider);
    DrawLine(hdc, margin, client.bottom - footerHeight,
             client.right - margin, client.bottom - footerHeight, divider);
}

LRESULT CALLBACK FilterWindowSubclassProc
'@
Replace-RegexOnce 'void LayoutFilterDialog\(HWND hwnd\)\r?\n\{.*?\r?\n\}\r?\n\r?\nLRESULT CALLBACK FilterWindowSubclassProc' $filterSurface 'modern Filter surface'

$filterSwitchOld = @'
LRESULT CALLBACK FilterWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                          LPARAM lParam, UINT_PTR subclassId,
                                          DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_GETMINMAXINFO:
'@
$filterSwitchNew = @'
LRESULT CALLBACK FilterWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
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
            PaintModernFilterDialog(hwnd, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON && IsModernFilterButton(item->CtlID))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }

        case WM_GETMINMAXINFO:
'@
Replace-Exact $filterSwitchOld $filterSwitchNew 'Filter subclass painting hooks'

Replace-RegexOnce 'info->ptMinTrackSize\.x = ScaleForDpi\(hwnd, 560\);\r?\n\s*info->ptMinTrackSize\.y = ScaleForDpi\(hwnd, 360\);' "info->ptMinTrackSize.x = ScaleForDpi(hwnd, 720);`r`n            info->ptMinTrackSize.y = ScaleForDpi(hwnd, 480);" 'Filter minimum size'

$enablePattern = '(?s)(void EnableResizableFilterDialog\(HWND hwnd\).*?SetWindowPos\(hwnd, NULL, x, y, width, height,\r?\n\s*SWP_NOZORDER \| SWP_NOACTIVATE \| SWP_FRAMECHANGED\);\r?\n\s*\}\r?\n)\s*LayoutFilterDialog\(hwnd\);\r?\n\}'
$enableReplacement = '$1    ConfigureModernFilterControls(hwnd);' + "`r`n" + '    LayoutFilterDialog(hwnd);' + "`r`n" + '    InvalidateRect(hwnd, NULL, TRUE);' + "`r`n" + '}'
Replace-RegexOnce $enablePattern $enableReplacement 'Filter dialog activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }

git diff -- $path

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/filter-ui-modernize-once.yml .github/scripts/filter-ui-modernize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Replace legacy Filter dialog presentation'
git push origin HEAD:modernization/windows11-ui
