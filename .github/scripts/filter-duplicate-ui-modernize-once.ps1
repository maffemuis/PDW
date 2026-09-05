$ErrorActionPreference = 'Stop'

$path = 'utils/windows11_ui.cpp'
$text = [System.IO.File]::ReadAllText($path)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) {
        throw "Expected source fragment not found: $label"
    }
    $script:text = $script:text.Replace($old, $new)
}

Replace-Exact @'
const UINT_PTR kFilterFindWindowSubclassId = 0x50445735;
'@ @'
const UINT_PTR kFilterFindWindowSubclassId = 0x50445735;
const UINT_PTR kFilterDuplicateWindowSubclassId = 0x50445736;
'@ 'duplicate filter subclass id'

$anchor = @'
BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

$insert = @'
bool IsFilterDuplicateDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE) != NULL &&
           GetDlgItem(hwnd, IDC_PROGRESS1) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERDUP_PCT) != NULL;
}

void ResizeModernFilterDuplicateDialog(HWND hwnd)
{
    const int width = ScaleForDpi(hwnd, 620);
    const int height = ScaleForDpi(hwnd, 280);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfo(monitor, &info)) return;

    const int workW = info.rcWork.right - info.rcWork.left;
    const int workH = info.rcWork.bottom - info.rcWork.top;
    const int actualW = min(width, workW);
    const int actualH = min(height, workH);
    const int x = info.rcWork.left + (workW - actualW) / 2;
    const int y = info.rcWork.top + (workH - actualH) / 2;

    SetWindowPos(hwnd, NULL, x, y, actualW, actualH,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void LayoutModernFilterDuplicateDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int listTop = header + ScaleForDpi(hwnd, 18);
    const int listHeight = ScaleForDpi(hwnd, 88);
    const int progressTop = listTop + listHeight + ScaleForDpi(hwnd, 18);
    const int buttonHeight = ScaleForDpi(hwnd, 36);
    const int buttonWidth = ScaleForDpi(hwnd, 112);
    const int buttonGap = ScaleForDpi(hwnd, 10);
    const int bottom = client.bottom - margin;

    HWND results = GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE);
    if (results)
        MoveWindow(results, margin, listTop,
                   max(1, client.right - 2 * margin), listHeight, TRUE);

    HWND progress = GetDlgItem(hwnd, IDC_PROGRESS1);
    if (progress)
        MoveWindow(progress, margin, progressTop,
                   max(1, client.right - 2 * margin - ScaleForDpi(hwnd, 62)),
                   ScaleForDpi(hwnd, 14), TRUE);

    HWND pct = GetDlgItem(hwnd, IDC_FILTERDUP_PCT);
    if (pct)
        MoveWindow(pct,
                   client.right - margin - ScaleForDpi(hwnd, 52),
                   progressTop - ScaleForDpi(hwnd, 4),
                   ScaleForDpi(hwnd, 52), ScaleForDpi(hwnd, 24), TRUE);

    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close)
        MoveWindow(close,
                   client.right - margin - buttonWidth,
                   bottom - buttonHeight,
                   buttonWidth, buttonHeight, TRUE);

    HWND find = GetDlgItem(hwnd, IDOK);
    if (find)
        MoveWindow(find,
                   client.right - margin - 2 * buttonWidth - buttonGap,
                   bottom - buttonHeight,
                   buttonWidth, buttonHeight, TRUE);
}

void ConfigureModernFilterDuplicateControls(HWND hwnd)
{
    HWND results = GetDlgItem(hwnd, IDC_FILTERFIND_DUPLICATE);
    if (results)
    {
        LONG_PTR style = GetWindowLongPtr(results, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(results, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(results, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(results, GWL_EXSTYLE, exStyle);
        SetWindowTheme(results, L"Explorer", NULL);
        SendMessage(results, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(results, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    HWND progress = GetDlgItem(hwnd, IDC_PROGRESS1);
    if (progress)
    {
        SetWindowTheme(progress, L"Explorer", NULL);
        SendMessage(progress, PBM_SETBKCOLOR, 0,
                    static_cast<LPARAM>(RGB(230, 235, 241)));
        SendMessage(progress, PBM_SETBARCOLOR, 0,
                    static_cast<LPARAM>(RGB(0, 103, 192)));
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
    if (find) SetWindowTextW(find, L"Find duplicates");
    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close) SetWindowTextW(close, L"Close");
}

void PaintModernFilterDuplicateDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int listTop = header + ScaleForDpi(hwnd, 18);
    const int listHeight = ScaleForDpi(hwnd, 88);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Check duplicate filters", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc, L"Find filters with the same type, address and message text.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             RGB(216, 224, 233));

    RECT resultsCard = {
        margin - 1,
        listTop - 1,
        client.right - margin + 1,
        listTop + listHeight + 1
    };
    FillRoundedRect(hdc, resultsCard, RGB(255, 255, 255), RGB(206, 216, 227),
                    ScaleForDpi(hwnd, 8));
}

LRESULT CALLBACK FilterDuplicateWindowSubclassProc(HWND hwnd, UINT message,
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
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(40, 48, 58));
            return reinterpret_cast<LRESULT>(GetDialogSurfaceBrush());
        }

        case WM_CTLCOLORLISTBOX:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH listBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(listBrush);
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

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                                 subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterDuplicateDialog(HWND hwnd)
{
    if (!IsFilterDuplicateDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterDuplicateWindowSubclassProc,
                      kFilterDuplicateWindowSubclassId, 0);
    ResizeModernFilterDuplicateDialog(hwnd);
    ConfigureModernFilterDuplicateControls(hwnd);
    LayoutModernFilterDuplicateDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

Replace-Exact $anchor $insert 'duplicate filter modern surface insertion'

Replace-Exact @'
    if (IsFilterFindDialog(hwnd)) EnableModernFilterFindDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ @'
    if (IsFilterFindDialog(hwnd)) EnableModernFilterFindDialog(hwnd);
    if (IsFilterDuplicateDialog(hwnd)) EnableModernFilterDuplicateDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ 'duplicate filter activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }
git diff -- $path

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/filter-duplicate-ui-modernize-once.yml .github/scripts/filter-duplicate-ui-modernize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Modernize duplicate filter dialog presentation'
git push origin HEAD:modernization/windows11-ui
