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
const UINT_PTR kFilterOptionsWindowSubclassId = 0x50445734;
'@ @'
const UINT_PTR kFilterOptionsWindowSubclassId = 0x50445734;
const UINT_PTR kFilterFindWindowSubclassId = 0x50445735;
'@ 'Filter Find subclass id'

$anchor = @'
BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

$insert = @'
bool IsFilterFindDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERFIND) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERFIND_HITS) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERFIND_CASE) != NULL;
}

BOOL CALLBACK HideLegacyFilterFindLabels(HWND child, LPARAM)
{
    wchar_t className[32] = {};
    if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpiW(className, L"Static") != 0)
        return TRUE;

    wchar_t label[64] = {};
    GetWindowTextW(child, label, ARRAYSIZE(label));
    if (lstrcmpiW(label, L"Find :") == 0 || lstrcmpiW(label, L"Hits :") == 0)
        ShowWindow(child, SW_HIDE);
    return TRUE;
}

void LayoutModernFilterFindDialog(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int editHeight = ScaleForDpi(hwnd, 30);
    const int rowTop = header + ScaleForDpi(hwnd, 28);

    HWND edit = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (edit)
        MoveWindow(edit, margin, rowTop,
                   max(1, client.right - 2 * margin), editHeight, TRUE);

    HWND hits = GetDlgItem(hwnd, IDC_FILTERFIND_HITS);
    if (hits)
        MoveWindow(hits, margin + ScaleForDpi(hwnd, 58),
                   rowTop + ScaleForDpi(hwnd, 45),
                   ScaleForDpi(hwnd, 80), ScaleForDpi(hwnd, 22), TRUE);

    HWND caseSensitive = GetDlgItem(hwnd, IDC_FILTERFIND_CASE);
    if (caseSensitive)
        MoveWindow(caseSensitive,
                   margin + ScaleForDpi(hwnd, 155),
                   rowTop + ScaleForDpi(hwnd, 44),
                   ScaleForDpi(hwnd, 125), ScaleForDpi(hwnd, 24), TRUE);

    HWND close = GetDlgItem(hwnd, IDCANCEL);
    if (close)
        MoveWindow(close,
                   client.right - margin - ScaleForDpi(hwnd, 92),
                   client.bottom - margin - ScaleForDpi(hwnd, 36),
                   ScaleForDpi(hwnd, 92), ScaleForDpi(hwnd, 36), TRUE);
}

void ConfigureModernFilterFindControls(HWND hwnd)
{
    EnumChildWindows(hwnd, HideLegacyFilterFindLabels, 0);

    HWND edit = GetDlgItem(hwnd, IDC_FILTERFIND);
    if (edit)
    {
        LONG_PTR style = GetWindowLongPtr(edit, GWL_STYLE);
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(edit, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(edit, GWL_EXSTYLE);
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE);
        SetWindowLongPtr(edit, GWL_EXSTYLE, exStyle);
        SetWindowTheme(edit, L"Explorer", NULL);
        SendMessage(edit, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        SetWindowPos(edit, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
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
    }
}

void ResizeModernFilterFindDialog(HWND hwnd)
{
    const int width = ScaleForDpi(hwnd, 430);
    const int height = ScaleForDpi(hwnd, 220);

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

void PaintModernFilterFindDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int margin = ScaleForDpi(hwnd, 18);
    const int header = ScaleForDpi(hwnd, 62);
    const int editTop = header + ScaleForDpi(hwnd, 28);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { margin, ScaleForDpi(hwnd, 10),
                   client.right - margin, ScaleForDpi(hwnd, 34) };
    DrawTextW(hdc, L"Find filter", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { margin, ScaleForDpi(hwnd, 34),
                      client.right - margin, header - ScaleForDpi(hwnd, 4) };
    DrawTextW(hdc, L"Search address, message text or label.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, margin, header - 1, client.right - margin, header - 1,
             RGB(216, 224, 233));

    SetTextColor(hdc, RGB(45, 56, 68));
    oldFont = SelectObject(hdc, GetHeaderFont());
    RECT findLabel = { margin, header + ScaleForDpi(hwnd, 5),
                       client.right - margin, editTop - ScaleForDpi(hwnd, 3) };
    DrawTextW(hdc, L"Search", -1, &findLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    RECT hitsLabel = { margin,
                       editTop + ScaleForDpi(hwnd, 44),
                       margin + ScaleForDpi(hwnd, 55),
                       editTop + ScaleForDpi(hwnd, 68) };
    DrawTextW(hdc, L"Hits", -1, &hitsLabel,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    RECT editCard = { margin - 1, editTop - 1,
                      client.right - margin + 1,
                      editTop + ScaleForDpi(hwnd, 31) };
    FillRoundedRect(hdc, editCard, RGB(255, 255, 255), RGB(196, 208, 220),
                    ScaleForDpi(hwnd, 8));
}

LRESULT CALLBACK FilterFindWindowSubclassProc(HWND hwnd, UINT message,
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
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(40, 48, 58));
            return reinterpret_cast<LRESULT>(GetDialogSurfaceBrush());
        }

        case WM_CTLCOLOREDIT:
        {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
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

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, FilterFindWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterFindDialog(HWND hwnd)
{
    if (!IsFilterFindDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterFindWindowSubclassProc,
                      kFilterFindWindowSubclassId, 0);
    ResizeModernFilterFindDialog(hwnd);
    ConfigureModernFilterFindControls(hwnd);
    LayoutModernFilterFindDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

Replace-Exact $anchor $insert 'Filter Find modern surface insertion'

Replace-Exact @'
    if (IsFilterOptionsDialog(hwnd)) EnableModernFilterOptionsDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ @'
    if (IsFilterOptionsDialog(hwnd)) EnableModernFilterOptionsDialog(hwnd);
    if (IsFilterFindDialog(hwnd)) EnableModernFilterFindDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ 'Filter Find activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }
git diff -- $path

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/filter-find-ui-modernize-once.yml .github/scripts/filter-find-ui-modernize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Modernize Find Filter dialog presentation'
git push origin HEAD:modernization/windows11-ui
