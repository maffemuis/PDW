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
const UINT_PTR kMainWindowSubclassId = 0x50445711;
const UINT_PTR kFilterWindowSubclassId = 0x50445731;
'@ @'
const UINT_PTR kMainWindowSubclassId = 0x50445711;
const UINT_PTR kFilterWindowSubclassId = 0x50445731;
const UINT_PTR kFilterEditWindowSubclassId = 0x50445732;
const UINT_PTR kModernGroupBoxSubclassId = 0x50445733;
'@ 'Filter Edit subclass ids'

Replace-Exact @'
HHOOK g_dialogHook = NULL;
bool g_chromeEnabled = false;
'@ @'
HHOOK g_dialogHook = NULL;
HBRUSH g_dialogSurfaceBrush = NULL;
bool g_chromeEnabled = false;
'@ 'dialog surface brush global'

Replace-Exact @'
HFONT GetFilterListFont()
{
    if (g_filterListFont) return g_filterListFont;
    g_filterListFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   FIXED_PITCH | FF_MODERN, L"Consolas");
    return g_filterListFont ? g_filterListFont : GetDialogFont();
}

void ApplyRoundedCorners(HWND hwnd)
'@ @'
HFONT GetFilterListFont()
{
    if (g_filterListFont) return g_filterListFont;
    g_filterListFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   FIXED_PITCH | FF_MODERN, L"Consolas");
    return g_filterListFont ? g_filterListFont : GetDialogFont();
}

HBRUSH GetDialogSurfaceBrush()
{
    if (!g_dialogSurfaceBrush)
        g_dialogSurfaceBrush = CreateSolidBrush(RGB(246, 249, 252));
    return g_dialogSurfaceBrush;
}

void ApplyRoundedCorners(HWND hwnd)
'@ 'dialog surface brush factory'

$anchor = @'
BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

$insert = @'
bool IsFilterEditDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_FILTERTYPE) != NULL &&
           GetDlgItem(hwnd, IDC_FILTERCAPCODE) != NULL &&
           GetDlgItem(hwnd, IDC_FILTER_APPLY) != NULL;
}

bool IsModernFilterEditButton(UINT controlId)
{
    switch (controlId)
    {
        case IDOK:
        case IDCANCEL:
        case IDC_FILTER_APPLY:
        case IDC_FILTER_PREVIOUS:
        case IDC_FILTER_NEXT:
        case IDC_FILTERRESET:
        case IDC_SEPFILTERFILEBROWSE1:
        case IDC_SEPFILTERFILEBROWSE2:
        case IDC_SEPFILTERFILEBROWSE3:
            return true;
        default:
            return false;
    }
}

LRESULT CALLBACK ModernGroupBoxSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
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

            const COLORREF card = RGB(255, 255, 255);
            const COLORREF border = RGB(212, 221, 231);
            const COLORREF title = RGB(45, 56, 68);
            FillRoundedRect(hdc, client, card, border, ScaleForDpi(hwnd, 10));

            wchar_t label[96] = {};
            GetWindowTextW(hwnd, label, ARRAYSIZE(label));
            if (label[0])
            {
                RECT text = client;
                text.left += ScaleForDpi(hwnd, 12);
                text.right -= ScaleForDpi(hwnd, 12);
                text.top += ScaleForDpi(hwnd, 4);
                text.bottom = text.top + ScaleForDpi(hwnd, 22);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, title);
                HGDIOBJ oldFont = SelectObject(hdc, GetHeaderFont());
                DrawTextW(hdc, label, -1, &text,
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

void ShiftFilterEditChildren(HWND hwnd, int deltaY)
{
    if (deltaY <= 0) return;

    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        RECT rect = {};
        if (!GetWindowRect(child, &rect)) continue;
        MapWindowPoints(NULL, hwnd, reinterpret_cast<POINT*>(&rect), 2);
        SetWindowPos(child, NULL,
                     rect.left, rect.top + deltaY,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void ConfigureModernFilterEditControls(HWND hwnd)
{
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
        if (lstrcmpiW(className, L"Button") != 0)
            continue;

        const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
        if ((style & 0x0F) == BS_GROUPBOX)
            SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                              kModernGroupBoxSubclassId, 0);
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

        SetWindowPos(help, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }
}

int ExpandFilterEditForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
    if (existing)
    {
        const INT_PTR stored = reinterpret_cast<INT_PTR>(existing);
        return stored > 0 ? static_cast<int>(stored - 1) : 0;
    }

    RECT window = {};
    GetWindowRect(hwnd, &window);
    const int currentHeight = window.bottom - window.top;
    const int header = ScaleForDpi(hwnd, 58);

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    int applied = 0;

    if (GetMonitorInfo(monitor, &info))
    {
        const int workHeight = info.rcWork.bottom - info.rcWork.top;
        if (currentHeight + header <= workHeight)
        {
            int y = window.top - header / 2;
            if (y < info.rcWork.top) y = info.rcWork.top;
            if (y + currentHeight + header > info.rcWork.bottom)
                y = info.rcWork.bottom - currentHeight - header;

            SetWindowPos(hwnd, NULL, window.left, y,
                         window.right - window.left, currentHeight + header,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            ShiftFilterEditChildren(hwnd, header);
            applied = header;
        }
    }

    SetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int FilterEditHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernFilterEditDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HBRUSH bg = GetDialogSurfaceBrush();
    FillRect(hdc, &client, bg);

    const int header = FilterEditHeaderOffset(hwnd);
    if (header <= 0) return;

    wchar_t caption[128] = {};
    GetWindowTextW(hwnd, caption, ARRAYSIZE(caption));
    const wchar_t* title =
        wcsstr(caption, L"Add") ? L"Add filter" :
        (wcsstr(caption, L"multiple") ? L"Edit selected filters" : L"Edit filter");

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT titleRect = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
        client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31)
    };
    DrawTextW(hdc, title, -1, &titleRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = {
        ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
        client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5)
    };
    DrawTextW(hdc, L"Matching, notification and output settings.", -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK FilterEditWindowSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
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
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(40, 48, 58));
            return reinterpret_cast<LRESULT>(GetDialogSurfaceBrush());
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

        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.FilterEdit.HeaderOffset");
            RemoveWindowSubclass(hwnd, FilterEditWindowSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernFilterEditDialog(HWND hwnd)
{
    if (!IsFilterEditDialog(hwnd)) return;

    SetWindowSubclass(hwnd, FilterEditWindowSubclassProc,
                      kFilterEditWindowSubclassId, 0);
    ExpandFilterEditForHeader(hwnd);
    ConfigureModernFilterEditControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

Replace-Exact $anchor $insert 'Filter Edit modern surface insertion'

Replace-Exact @'
    if (IsFilterDialog(hwnd)) EnableResizableFilterDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ @'
    if (IsFilterDialog(hwnd)) EnableResizableFilterDialog(hwnd);
    if (IsFilterEditDialog(hwnd)) EnableModernFilterEditDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ 'Filter Edit activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }
git diff -- $path

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/filter-edit-ui-modernize-once.yml .github/scripts/filter-edit-ui-modernize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Modernize Add Edit Filter dialog presentation'
git push origin HEAD:modernization/windows11-ui
