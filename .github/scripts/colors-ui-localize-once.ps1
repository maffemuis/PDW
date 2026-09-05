$ErrorActionPreference = 'Stop'

$path = 'utils/windows11_ui.cpp'
$text = [System.IO.File]::ReadAllText($path)

function Replace-Exact([string]$old, [string]$new, [string]$label) {
    if (-not $script:text.Contains($old)) { throw "Expected source fragment not found: $label" }
    $script:text = $script:text.Replace($old, $new)
}

Replace-Exact @'
const UINT_PTR kStatisticsWindowSubclassId = 0x5044573F;
'@ @'
const UINT_PTR kStatisticsWindowSubclassId = 0x5044573F;
const UINT_PTR kColorsWindowSubclassId = 0x50445740;
'@ 'colors subclass id'

$anchor = @'
BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

$insert = @'
bool IsColorsDialog(HWND hwnd)
{
    if (!hwnd) return false;
    wchar_t className[32] = {};
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) <= 0 ||
        lstrcmpW(className, L"#32770") != 0)
        return false;

    return GetDlgItem(hwnd, IDC_COLORBACKGND) != NULL &&
           GetDlgItem(hwnd, IDC_COLORCAPCODE) != NULL &&
           GetDlgItem(hwnd, IDC_COLORTIMESTAMP) != NULL &&
           GetDlgItem(hwnd, IDC_COLORNUMERIC) != NULL &&
           GetDlgItem(hwnd, IDC_COLORDEFAULT) != NULL;
}

void SetColorsDialogText(HWND hwnd)
{
    SetWindowTextW(hwnd, L"Kleuren");
    struct ItemText { int id; const wchar_t* text; };
    const ItemText items[] = {
        { IDC_COLORBACKGND, L"Achtergrond" },
        { IDC_COLORCAPCODE, L"Capcode" },
        { IDC_COLORFLEXPHASE, L"FLEX-fase" },
        { IDC_COLORTIMESTAMP, L"Tijdstempel" },
        { IDC_COLORBITERRORS, L"Bitfouten" },
        { IDC_COLORNUMERIC, L"Numeriek" },
        { IDC_COLORALPHANUM, L"Alfanumeriek" },
        { IDC_COLORFLEXBIN, L"FLEX binair" },
        { IDC_COLORFILTMATCH, L"Filtertreffer" },
        { IDC_COLORFILTERLABEL, L"Filterlabel" },
        { IDC_COLORDEFAULT, L"Standaardkleuren" },
        { IDC_COLORWIN, L"Windows-kleuren" },
        { IDC_COLORINSTRUCTIONS, L"Klik op een kleurvak om de kleur aan te passen." },
        { IDOK, L"OK" },
        { IDCANCEL, L"Annuleren" }
    };
    for (int i = 0; i < static_cast<int>(ARRAYSIZE(items)); ++i)
    {
        HWND child = GetDlgItem(hwnd, items[i].id);
        if (child) SetWindowTextW(child, items[i].text);
    }
}

void ConfigureModernColorsControls(HWND hwnd)
{
    SetColorsDialogText(hwnd);
    for (HWND child = GetWindow(hwnd, GW_CHILD);
         child;
         child = GetWindow(child, GW_HWNDNEXT))
    {
        SendMessage(child, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetDialogFont()), TRUE);
        wchar_t className[32] = {};
        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0)
            continue;
        if (lstrcmpiW(className, L"Button") == 0)
            SetWindowTheme(child, L"Explorer", NULL);
    }
}

int ExpandColorsForHeader(HWND hwnd)
{
    HANDLE existing = GetPropW(hwnd, L"PDW.Colors.HeaderOffset");
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
    SetPropW(hwnd, L"PDW.Colors.HeaderOffset",
             reinterpret_cast<HANDLE>(static_cast<INT_PTR>(applied + 1)));
    return applied;
}

int ColorsHeaderOffset(HWND hwnd)
{
    HANDLE value = GetPropW(hwnd, L"PDW.Colors.HeaderOffset");
    if (!value) return 0;
    const INT_PTR stored = reinterpret_cast<INT_PTR>(value);
    return stored > 0 ? static_cast<int>(stored - 1) : 0;
}

void PaintModernColorsDialog(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    FillRect(hdc, &client, GetDialogSurfaceBrush());
    const int header = ColorsHeaderOffset(hwnd);
    if (header <= 0) return;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(24, 39, 58));
    HGDIOBJ oldFont = SelectObject(hdc, GetTitleFont());
    RECT title = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 8),
                   client.right - ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31) };
    DrawTextW(hdc, L"Kleuren", -1, &title,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, oldFont);

    SetTextColor(hdc, RGB(91, 103, 116));
    oldFont = SelectObject(hdc, GetDialogFont());
    RECT subtitle = { ScaleForDpi(hwnd, 14), ScaleForDpi(hwnd, 31),
                      client.right - ScaleForDpi(hwnd, 14), header - ScaleForDpi(hwnd, 5) };
    DrawTextW(hdc, L"Pas de weergavekleuren van gedecodeerde berichten aan.", -1,
              &subtitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);
    DrawLine(hdc, ScaleForDpi(hwnd, 12), header - 1,
             client.right - ScaleForDpi(hwnd, 12), header - 1,
             RGB(216, 224, 233));
}

LRESULT CALLBACK ColorsWindowSubclassProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId,
                                          DWORD_PTR referenceData)
{
    switch (message)
    {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintModernColorsDialog(hwnd, hdc);
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
        case WM_NCDESTROY:
            RemovePropW(hwnd, L"PDW.Colors.HeaderOffset");
            RemoveWindowSubclass(hwnd, ColorsWindowSubclassProc, subclassId);
            break;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void EnableModernColorsDialog(HWND hwnd)
{
    if (!IsColorsDialog(hwnd)) return;
    SetWindowSubclass(hwnd, ColorsWindowSubclassProc,
                      kColorsWindowSubclassId, 0);
    ExpandColorsForHeader(hwnd);
    ConfigureModernColorsControls(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

BOOL CALLBACK StyleDialogChild(HWND child, LPARAM fontParam)
'@

Replace-Exact $anchor $insert 'colors modern surface insertion'

Replace-Exact @'
    if (IsCustomAudioDialog(hwnd)) EnableModernCustomAudioDialog(hwnd);
    if (IsStatisticsDialog(hwnd)) EnableModernStatisticsDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ @'
    if (IsCustomAudioDialog(hwnd)) EnableModernCustomAudioDialog(hwnd);
    if (IsStatisticsDialog(hwnd)) EnableModernStatisticsDialog(hwnd);
    if (IsColorsDialog(hwnd)) EnableModernColorsDialog(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
'@ 'colors activation'

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

git diff --check
if ($LASTEXITCODE -ne 0) { throw 'git diff --check failed' }

git config user.name 'github-actions[bot]'
git config user.email '41898282+github-actions[bot]@users.noreply.github.com'
git rm .github/workflows/colors-ui-localize-once.yml .github/scripts/colors-ui-localize-once.ps1
git add $path
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw 'cached diff check failed' }
git commit -m 'Modernize and localize Colors dialog presentation'
git push origin HEAD:modernization/windows11-ui
