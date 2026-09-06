$ErrorActionPreference = 'Stop'

function Replace-Exact([string]$Path, [string]$Old, [string]$New, [string]$Label) {
    $text = Get-Content -LiteralPath $Path -Raw
    if (-not $text.Contains($Old)) {
        throw "Pattern not found for $Label in $Path"
    }
    $text = $text.Replace($Old, $New)
    Set-Content -LiteralPath $Path -Value $text -Encoding utf8NoBOM
}

$ui = 'utils/windows11_ui.cpp'
$pane = 'utils/windows11_pane_skin.cpp'

# Softer shell: reduce the all-white appearance while keeping the light Windows 11 language.
Replace-Exact $ui '    const COLORREF normalBg = RGB(250, 250, 250);' '    const COLORREF normalBg = RGB(246, 249, 252);' 'button background'
Replace-Exact $ui '    const COLORREF shellBg = RGB(247, 250, 253);' '    const COLORREF shellBg = RGB(238, 244, 249);' 'navigation background'
Replace-Exact $ui '    const COLORREF capsuleBg = RGB(249, 251, 253);' '    const COLORREF capsuleBg = RGB(244, 248, 251);' 'navigation capsule'
Replace-Exact $ui '    const COLORREF rowBg = RGB(252, 253, 254);' '    const COLORREF rowBg = RGB(245, 249, 252);' 'command strip'
Replace-Exact $ui '    const COLORREF background = RGB(249, 251, 253);' '    const COLORREF background = RGB(239, 245, 250);' 'column header'
Replace-Exact $ui '    const COLORREF cardBg = RGB(255, 255, 255);' '    const COLORREF cardBg = RGB(248, 251, 253);' 'card background'
Replace-Exact $ui '    const COLORREF titleBg = RGB(245, 249, 253);' '    const COLORREF titleBg = RGB(236, 243, 249);' 'card title background'
Replace-Exact $ui '    const COLORREF bg = RGB(249, 251, 253);' '    const COLORREF bg = RGB(239, 245, 250);' 'status background'
Replace-Exact $ui '    const COLORREF workspace = RGB(241, 246, 251);' '    const COLORREF workspace = RGB(231, 238, 245);' 'workspace background'

# Restore an actual reception meter and percentage from PDW''s existing dRX_Quality signal.
$oldRx = @'
    if (withRx)
    {
        const bool active = !bPauseFlag && dRX_Quality > 0.0;
        const COLORREF dot = active ? RGB(20, 170, 62) : RGB(154, 160, 168);
        const int dotSize = ScaleForDpi(hwnd, 10);
        const int dotRight = card.right - ScaleForDpi(hwnd, 14);
        const int cy = titleRect.top + (titleHeight / 2);
        HBRUSH dotBrush = CreateSolidBrush(dot);
        HPEN dotPen = CreatePen(PS_SOLID, 1, dot);
        HGDIOBJ oldBrush = SelectObject(hdc, dotBrush);
        HGDIOBJ oldPen = SelectObject(hdc, dotPen);
        Ellipse(hdc, dotRight - dotSize, cy - dotSize / 2,
                dotRight, cy + dotSize / 2);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(dotPen);
        DeleteObject(dotBrush);

        SetTextColor(hdc, titleFg);
        oldFont = SelectObject(hdc, GetHeaderFont());
        RECT rxText = { dotRight - ScaleForDpi(hwnd, 70), titleRect.top,
                        dotRight - ScaleForDpi(hwnd, 16), titleRect.bottom };
        DrawTextW(hdc, L"RX-Q", -1, &rxText,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }
'@
$newRx = @'
    if (withRx)
    {
        double quality = dRX_Quality;
        if (quality < 0.0) quality = 0.0;
        if (quality > 100.0) quality = 100.0;

        const bool active = !bPauseFlag && quality > 0.0;
        const bool weak = active && quality < 90.0;
        const COLORREF meterOn = weak ? RGB(196, 120, 0) : RGB(20, 170, 62);
        const COLORREF meterOff = RGB(184, 194, 204);
        const int cy = titleRect.top + (titleHeight / 2);
        const int meterLeft = card.right - ScaleForDpi(hwnd, 150);
        const int meterBottom = cy + ScaleForDpi(hwnd, 7);
        const int barWidth = ScaleForDpi(hwnd, 4);
        const int barGap = ScaleForDpi(hwnd, 3);

        // Five ascending bars: a modern equivalent of PDW''s old reception indicator.
        for (int i = 0; i < 5; ++i)
        {
            const int barHeight = ScaleForDpi(hwnd, 4 + i * 2);
            RECT bar = {
                meterLeft + i * (barWidth + barGap),
                meterBottom - barHeight,
                meterLeft + i * (barWidth + barGap) + barWidth,
                meterBottom
            };
            const bool filled = active && quality >= static_cast<double>((i + 1) * 20);
            HBRUSH barBrush = CreateSolidBrush(filled ? meterOn : meterOff);
            FillRect(hdc, &bar, barBrush);
            DeleteObject(barBrush);
        }

        wchar_t qualityText[32] = {};
        if (quality > 0.0)
            swprintf(qualityText, ARRAYSIZE(qualityText), L"%.1f%%", quality);
        else
            lstrcpyW(qualityText, L"--.-%");

        SetTextColor(hdc, active ? (weak ? RGB(145, 86, 0) : titleFg) : RGB(104, 116, 128));
        oldFont = SelectObject(hdc, GetHeaderFont());
        RECT rxText = {
            meterLeft + ScaleForDpi(hwnd, 42), titleRect.top,
            card.right - ScaleForDpi(hwnd, 14), titleRect.bottom
        };
        DrawTextW(hdc, qualityText, -1, &rxText,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }
'@
Replace-Exact $ui $oldRx $newRx 'RX quality meter'

# Draw the full modern shell into a memory bitmap first, then blit once.
$oldWorkspace = @'
void DrawModernWorkspace(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HDC hdc = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_CLIPCHILDREN);
    if (!hdc) hdc = GetDC(hwnd);
    if (!hdc) return;

    const COLORREF workspace = RGB(231, 238, 245);
    HBRUSH background = CreateSolidBrush(workspace);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    DrawTopNavigation(hdc, hwnd, client);
    DrawCommandStrip(hdc, hwnd, client);
    DrawCard(hdc, hwnd, g_pane1Card, g_pane1Body, L"Monitored messages", true);
    DrawCard(hdc, hwnd, g_pane2Card, g_pane2Body, L"Filtered messages", false);
    DrawStatusBar(hdc, hwnd);

    ReleaseDC(hwnd, hdc);
}
'@
$newWorkspace = @'
void DrawModernWorkspace(HWND hwnd)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0) return;

    HDC target = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_CLIPCHILDREN);
    if (!target) target = GetDC(hwnd);
    if (!target) return;

    HDC buffer = CreateCompatibleDC(target);
    HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
    HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;
    HDC hdc = (buffer && bitmap) ? buffer : target;

    const COLORREF workspace = RGB(231, 238, 245);
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
        BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(buffer);
    }
    else if (buffer)
    {
        DeleteDC(buffer);
    }

    ReleaseDC(hwnd, target);
}
'@
Replace-Exact $ui $oldWorkspace $newWorkspace 'buffered main workspace'

# Prevent the legacy background erase from flashing through before the modern shell repaint.
$oldMainStart = @'
{
    if (message == WM_GETMINMAXINFO)
'@
$newMainStart = @'
{
    if (message == WM_ERASEBKGND)
        return 1;

    if (message == WM_GETMINMAXINFO)
'@
Replace-Exact $ui $oldMainStart $newMainStart 'main WM_ERASEBKGND'

# Softer message surfaces and buffered pane painting.
Replace-Exact $pane '    HBRUSH base = CreateSolidBrush(RGB(255, 255, 255));' '    HBRUSH base = CreateSolidBrush(RGB(247, 250, 252));' 'pane base background'
Replace-Exact $pane '            HBRUSH alternate = CreateSolidBrush(RGB(250, 252, 254));' '            HBRUSH alternate = CreateSolidBrush(RGB(240, 246, 250));' 'pane alternate background'

$oldPanePaint = @'
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawPaneRows(hwnd, hdc, pane);
            EndPaint(hwnd, &ps);
            return 0;
        }
'@
$newPanePaint = @'
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC target = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int width = client.right - client.left;
            const int height = client.bottom - client.top;

            HDC buffer = (width > 0 && height > 0) ? CreateCompatibleDC(target) : NULL;
            HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;
            HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;

            if (buffer && bitmap)
            {
                DrawPaneRows(hwnd, buffer, pane);
                BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
                SelectObject(buffer, oldBitmap);
                DeleteObject(bitmap);
                DeleteDC(buffer);
            }
            else
            {
                if (buffer) DeleteDC(buffer);
                DrawPaneRows(hwnd, target, pane);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
'@
Replace-Exact $pane $oldPanePaint $newPanePaint 'buffered pane paint'

$oldInteraction = @'
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            InvalidateRect(hwnd, NULL, FALSE);
            return result;
'@
$newInteraction = @'
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            // Plain pointer movement has no hover presentation in a message pane.
            // Repaint while selecting, or for events that actually change pane state.
            if (message != WM_MOUSEMOVE || selecting != 0)
                InvalidateRect(hwnd, NULL, FALSE);
            return result;
'@
Replace-Exact $pane $oldInteraction $newInteraction 'bounded pane invalidation'

Write-Host 'Main UI stability, softer surfaces and RX quality meter patch applied.'
