$ErrorActionPreference = 'Stop'

$path = 'utils/windows11_ui.cpp'
$text = [System.IO.File]::ReadAllText($path)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) { throw "Expected source fragment not found: $label" }
    $script:text = $script:text.Replace($old, $new)
}

Replace-Exact @'
const UINT_PTR kInterfaceSetupWindowSubclassId = 0x5044573C;
'@ @'
const UINT_PTR kInterfaceSetupWindowSubclassId = 0x5044573C;
const UINT_PTR kLogfileWindowSubclassId = 0x5044573D;
'@ 'logfile subclass id'

$anchor = @'
BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

$insert = @'
bool IsLogfileDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_LOGFILEEN) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGFILEDATE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGBROWSE) != NULL &&
           GetDlgItem(hwnd, IDC_LOGCOLUMN + 1) != NULL &&
           GetDlgItem(hwnd, IDC_LOGCOLUMN + 7) != NULL;
}

void FlattenModernLogfileField(HWND child)
{
    if (!child) return;
    LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_BORDER);
    SetWindowLongPtr(child, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(child, GWL_EXSTYLE);
    exStyle &= ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(child, GWL_EXSTYLE, exStyle);

    SetWindowTheme(child, L"Explorer", NULL);
    SendMessage(child, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
}

void ConfigureModernLogfileControls(HWND hwnd)
{
    FlattenModernLogfileField(GetDlgItem(hwnd, IDC_LOGFILE));

    const int actionIds[] = { IDC_LOGBROWSE, IDOK, IDCANCEL };
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
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||
            lstrcmpiW(className, L"Button") != 0)
            continue;

        const LONG_PTR style = GetWindowLongPtr(child, GWL_STYLE);
        if ((style & 0x0F) == BS_GROUPBOX)
            SetWindowSubclass(child, ModernGroupBoxSubclassProc,
                              kModernGroupBoxSubclassId, 0);
        else
        {
            const int id = GetDlgCtrlID(child);
            if (id != IDC_LOGBROWSE && id != IDOK && id != IDCANCEL)
                SetWindowTheme(child, L"Explorer", NULL);
        }
    }
}

int ExpandLogfileForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Logfile.HeaderOffset");
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

    SetPropW(hwnd, L"PDW.Logfile.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int LogfileHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Logfile.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernLogfileDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());

    const int header = LogfileHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, L"Logfile", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc,
              L"Choose the logfile name and the message columns written to disk.",
              -1, &subtitle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK LogfileWindowSubclassProc(HWND hwnd, UINT message,
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
            PaintModernLogfileDialog(hwnd, hdc);
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
            SetBkMode(hdc, OPAQUE);
            SetTextColor(hdc, RGB(32, 32, 32));
            SetBkColor(hdc, RGB(255, 255, 255));
            static HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(editBrush);
        }
        case WM_DRAWITEM:
        {
            const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            if (item && item->CtlType == ODT_BUTTON &&
                (item->CtlID == IDC_LOGBROWSE || item->CtlID == IDOK ||
                 item->CtlID == IDCANCEL))
            {
                DrawModernFilterButton(item);
                return TRUE;
            }
            break;
        }
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Logfile.HeaderOffset");
            RemoveWindowSubclass(hwnd, LogfileWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernLogfileDialog(HWND hwnd)
{
    if (!IsLogfileDialog(hwnd)) return;
    SetWindowSubclass(hwnd, LogfileWindowSubclassProc,
                      kLogfileWindowSubclassId, 0);
    ExpandLogfileForHeader(hwnd);
    ConfigureModernLogfileControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

Replace-Exact $anchor $insert 'logfile modern surface insertion'

Replace-Exact @'
    if (IsSystemTrayDialog(hwnd)) EnableModernSystemTrayDialog(hwnd);
    if (IsInterfaceSetupDialog(hwnd)) EnableModernInterfaceSetupDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ @'
    if (IsSystemTrayDialog(hwnd)) EnableModernSystemTrayDialog(hwnd);
    if (IsInterfaceSetupDialog(hwnd)) EnableModernInterfaceSetupDialog(hwnd);
    if (IsLogfileDialog(hwnd)) EnableModernLogfileDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ 'logfile activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/logfile-ui-modernize-once.yml .github/scripts/logfile-ui-modernize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Modernize Logfile dialog presentation'
git push origin HEAD:modernization/windows11-ui
