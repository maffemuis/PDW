from pathlib import Path
import re


def read(path):
    return Path(path).read_text(encoding="utf-8-sig")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8", newline="")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly 1 match, got {count}")
    return text.replace(old, new, 1)


def replace_all_checked(text, old, new, label, minimum=1):
    count = text.count(old)
    if count < minimum:
        raise RuntimeError(f"{label}: expected at least {minimum} matches, got {count}")
    return text.replace(old, new)


ui_path = "utils/windows11_ui.cpp"
pane_path = "utils/windows11_pane_skin.cpp"
sig_path = "sigind.cpp"
pdw_path = "PDW.cpp"
finish_path = "Rsrc.nl.finish.rc"

ui = read(ui_path)
pane = read(pane_path)
sig = read(sig_path)
pdw = read(pdw_path)
finish = read(finish_path)

# ---------------------------------------------------------------------------
# Main shell palette: retain the Windows 11 light language without pure-white
# surfaces everywhere.
# ---------------------------------------------------------------------------
for old, new, label in [
    ("    const COLORREF normalBg = RGB(250, 250, 250);", "    const COLORREF normalBg = RGB(246, 249, 252);", "button background"),
    ("    const COLORREF shellBg = RGB(247, 250, 253);", "    const COLORREF shellBg = RGB(238, 244, 249);", "navigation background"),
    ("    const COLORREF capsuleBg = RGB(249, 251, 253);", "    const COLORREF capsuleBg = RGB(244, 248, 251);", "navigation capsule"),
    ("    const COLORREF rowBg = RGB(252, 253, 254);", "    const COLORREF rowBg = RGB(245, 249, 252);", "command strip"),
    ("    const COLORREF background = RGB(249, 251, 253);", "    const COLORREF background = RGB(239, 245, 250);", "column header"),
    ("    const COLORREF cardBg = RGB(255, 255, 255);", "    const COLORREF cardBg = RGB(248, 251, 253);", "card background"),
    ("    const COLORREF titleBg = RGB(245, 249, 253);", "    const COLORREF titleBg = RGB(236, 243, 249);", "card title background"),
    ("    const COLORREF bg = RGB(249, 251, 253);", "    const COLORREF bg = RGB(239, 245, 250);", "status background"),
    ("    const COLORREF workspace = RGB(241, 246, 251);", "    const COLORREF workspace = RGB(231, 238, 245);", "workspace background"),
]:
    ui = replace_once(ui, old, new, label)

# Live signal state and decode-quality percentage are deliberately separate.
ui = replace_once(
    ui,
    "extern double dRX_Quality;\nextern bool bPauseFlag;",
    "extern double dRX_Quality;\nextern int si_index;\nextern bool bPauseFlag;",
    "live signal state declaration",
)

old_rx = r'''    if (withRx)
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
'''
new_rx = r'''    if (withRx)
    {
        // si_index is PDW's original live signal-indicator state (0..20).
        // It follows the raw receive transitions and therefore moves while
        // listening and swings on a received signal. dRX_Quality is a separate
        // decode-quality metric and is shown only as the percentage.
        int signal = si_index;
        if (signal < 0) signal = 0;
        if (signal > 20) signal = 20;

        double quality = dRX_Quality;
        if (quality < 0.0) quality = 0.0;
        if (quality > 100.0) quality = 100.0;

        const int cy = titleRect.top + (titleHeight / 2);
        const int meterWidth = ScaleForDpi(hwnd, 104);
        const int meterHeight = ScaleForDpi(hwnd, 10);
        const int meterRight = card.right - ScaleForDpi(hwnd, 76);
        RECT meter = {
            meterRight - meterWidth,
            cy - meterHeight / 2,
            meterRight,
            cy + meterHeight / 2
        };
        FillRoundedRect(hdc, meter, RGB(219, 228, 236), RGB(203, 214, 224),
                        ScaleForDpi(hwnd, 8));

        const int innerPad = ScaleForDpi(hwnd, 2);
        RECT inner = {
            meter.left + innerPad,
            meter.top + innerPad,
            meter.right - innerPad,
            meter.bottom - innerPad
        };
        const int innerWidth = inner.right - inner.left;
        const int liveWidth = (innerWidth * signal) / 20;
        if (liveWidth > 0)
        {
            RECT live = inner;
            live.right = live.left + liveWidth;
            const COLORREF signalColor = signal >= 15
                ? RGB(20, 170, 62)
                : (signal >= 8 ? RGB(0, 120, 212) : RGB(86, 145, 198));
            FillRoundedRect(hdc, live, signalColor, signalColor,
                            ScaleForDpi(hwnd, 6));
        }

        // A small marker makes low-level/noise movement visible even when only
        // one or two of the old signal steps are active.
        const int markerX = inner.left + (innerWidth * signal) / 20;
        HPEN markerPen = CreatePen(PS_SOLID, ScaleForDpi(hwnd, 2), RGB(28, 45, 62));
        HGDIOBJ oldPen = SelectObject(hdc, markerPen);
        MoveToEx(hdc, markerX, meter.top - ScaleForDpi(hwnd, 2), NULL);
        LineTo(hdc, markerX, meter.bottom + ScaleForDpi(hwnd, 2));
        SelectObject(hdc, oldPen);
        DeleteObject(markerPen);

        wchar_t qualityText[32] = {};
        if (quality > 0.0)
            swprintf(qualityText, ARRAYSIZE(qualityText), L"%.1f%%", quality);
        else
            lstrcpyW(qualityText, L"--.-%");

        SetTextColor(hdc, quality > 0.0 && quality < 90.0
                     ? RGB(145, 86, 0) : titleFg);
        oldFont = SelectObject(hdc, GetHeaderFont());
        RECT rxText = {
            meter.right + ScaleForDpi(hwnd, 8), titleRect.top,
            card.right - ScaleForDpi(hwnd, 14), titleRect.bottom
        };
        DrawTextW(hdc, qualityText, -1, &rxText,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, oldFont);
    }
'''
ui = replace_once(ui, old_rx, new_rx, "live RX meter")

# Buffered shell paint: draw once off-screen and blit to eliminate shell jitter.
old_workspace = r'''void DrawModernWorkspace(HWND hwnd)
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
'''
new_workspace = r'''void DrawModernWorkspace(HWND hwnd)
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
'''
ui = replace_once(ui, old_workspace, new_workspace, "buffered main workspace")

ui = replace_once(
    ui,
    "{\n    if (message == WM_GETMINMAXINFO)\n    {",
    "{\n    if (message == WM_ERASEBKGND)\n        return 1;\n\n    if (message == WM_GETMINMAXINFO)\n    {",
    "main erase suppression",
)

ui = replace_once(
    ui,
    "        case WM_TIMER:\n            if (wParam == kLegacySecondTimer) DrawModernWorkspace(hwnd);\n            break;",
    "        case WM_TIMER:\n            // RX quality changes at one-second cadence, while the original\n            // signal indicator state changes on the 100 ms decoder timer.\n            if (wParam == kLegacySecondTimer || wParam == PDW_TIMER)\n                DrawModernWorkspace(hwnd);\n            break;",
    "live meter timer refresh",
)

# ---------------------------------------------------------------------------
# Filter visual acceptance: one header layer, Dutch wording and a light list.
# ---------------------------------------------------------------------------
ui = replace_once(
    ui,
    'L"Manage address and message matching rules."',
    'L"Beheer adressen en regels voor berichtovereenkomsten."',
    "Dutch filter subtitle",
)

# Configure the SysListView32 itself, not only its surrounding dialog.
needle = '''        SetWindowTheme(list, L"Explorer", NULL);\n        SendMessage(list, WM_SETFONT, reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);'''
replacement = '''        SetWindowTheme(list, L"Explorer", NULL);\n        SendMessage(list, WM_SETFONT, reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);\n        ListView_SetBkColor(list, RGB(247, 250, 252));\n        ListView_SetTextBkColor(list, RGB(247, 250, 252));\n        ListView_SetTextColor(list, RGB(32, 40, 48));'''
ui = replace_once(ui, needle, replacement, "light filter list")

# Hide the legacy resource subtitle in the filter dialog; the modern header
# draws the canonical heading/subheading. This prevents both layers overlapping.
filter_config_marker = "void ConfigureModernFilterControls(HWND hwnd)\n{\n"
if filter_config_marker not in ui:
    raise RuntimeError("filter control configuration marker missing")
filter_hide = '''void ConfigureModernFilterControls(HWND hwnd)\n{\n    for (HWND child = GetWindow(hwnd, GW_CHILD);\n         child;\n         child = GetWindow(child, GW_HWNDNEXT))\n    {\n        wchar_t className[32] = {};\n        if (GetClassNameW(child, className, ARRAYSIZE(className)) <= 0 ||\n            lstrcmpiW(className, L"Static") != 0)\n            continue;\n        RECT rect = {};\n        GetWindowRect(child, &rect);\n        MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<POINT*>(&rect), 2);\n        if (rect.top < ScaleForDpi(hwnd, 70))\n            ShowWindow(child, SW_HIDE);\n    }\n'''
ui = replace_once(ui, filter_config_marker, filter_hide, "hide duplicate filter resource header")

# Interface dialog modern header must use the same Dutch presentation language.
ui = replace_once(ui, 'L"Interface setup"', 'L"Interface-instellingen"', "Dutch interface title")
ui = replace_once(
    ui,
    'L"Configure serial input and soundcard capture without changing decoder behavior."',
    'L"Configureer seriële invoer en geluidskaartopname zonder het decodergedrag te wijzigen."',
    "Dutch interface subtitle",
)

# ---------------------------------------------------------------------------
# Message panes: softer surfaces and buffered paint. Ordinary pointer motion
# does not repaint the entire pane unless a selection is actually being made.
# ---------------------------------------------------------------------------
pane = replace_once(pane, "    HBRUSH base = CreateSolidBrush(RGB(255, 255, 255));", "    HBRUSH base = CreateSolidBrush(RGB(247, 250, 252));", "pane base")
pane = replace_once(pane, "            HBRUSH alternate = CreateSolidBrush(RGB(250, 252, 254));", "            HBRUSH alternate = CreateSolidBrush(RGB(240, 246, 250));", "pane alternate")

old_paint = '''        case WM_PAINT:\n        {\n            PAINTSTRUCT ps = {};\n            HDC hdc = BeginPaint(hwnd, &ps);\n            DrawPaneRows(hwnd, hdc, pane);\n            EndPaint(hwnd, &ps);\n            return 0;\n        }\n'''
new_paint = '''        case WM_PAINT:\n        {\n            PAINTSTRUCT ps = {};\n            HDC target = BeginPaint(hwnd, &ps);\n            RECT client = {};\n            GetClientRect(hwnd, &client);\n            const int width = client.right - client.left;\n            const int height = client.bottom - client.top;\n\n            HDC buffer = (width > 0 && height > 0) ? CreateCompatibleDC(target) : NULL;\n            HBITMAP bitmap = buffer ? CreateCompatibleBitmap(target, width, height) : NULL;\n            HGDIOBJ oldBitmap = (buffer && bitmap) ? SelectObject(buffer, bitmap) : NULL;\n\n            if (buffer && bitmap)\n            {\n                DrawPaneRows(hwnd, buffer, pane);\n                BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);\n                SelectObject(buffer, oldBitmap);\n                DeleteObject(bitmap);\n                DeleteDC(buffer);\n            }\n            else\n            {\n                if (buffer) DeleteDC(buffer);\n                DrawPaneRows(hwnd, target, pane);\n            }\n\n            EndPaint(hwnd, &ps);\n            return 0;\n        }\n'''
pane = replace_once(pane, old_paint, new_paint, "buffered pane paint")
pane = replace_once(
    pane,
    "            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);\n            InvalidateRect(hwnd, NULL, FALSE);\n            return result;",
    "            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);\n            if (message != WM_MOUSEMOVE || selecting != 0)\n                InvalidateRect(hwnd, NULL, FALSE);\n            return result;",
    "bounded mouse invalidation",
)

# ---------------------------------------------------------------------------
# Preserve PDW's original live signal model in modern chrome. Previously the
# state itself only moved while the legacy bitmap rectangle existed.
# ---------------------------------------------------------------------------
start = sig.find("void UpdateSigInd(int direction_flg)\n{")
end = sig.find("\n\n\n// Draw signal indicator needle.", start)
if start < 0 or end < 0:
    raise RuntimeError("UpdateSigInd function bounds not found")
new_update = r'''void UpdateSigInd(int direction_flg)
{
    si_old_index = si_index;

    if (direction_flg) // Move indicator right
    {
        si_low_hover = 0;
        si_index ? si_index += 2 : si_index++;

        if (si_index > MAX_SI_POS)
        {
            si_hi_hover++;
            si_index = MAX_SI_POS;
            return;
        }
    }
    else // Move indicator left
    {
        if (si_low_hover)
        {
            // Preserve the old low-end needle animation when the bitmap UI is
            // actually active. The state machine itself stays active for the
            // modern meter even though legacy drawing is suppressed.
            if (old_rect_flg && si_low_hover == 1)
                show_sigind(1, 0);
            if (si_low_hover > 1)
            {
                si_low_hover = 0;
                if (old_rect_flg) show_sigind(0, 1);
            }
        }
        si_hi_hover = 0;
        si_index--;

        if (si_index < 0)
        {
            si_low_hover++;
            si_index = 0;
            return;
        }
    }

    if (old_rect_flg)
        show_sigind(si_index, si_old_index);
}'''
sig = sig[:start] + new_update + sig[end:]

# ---------------------------------------------------------------------------
# Dutch runtime strings that override already-Dutch resources.
# ---------------------------------------------------------------------------
pdw = replace_once(pdw, 'SetWindowText(hDlg, (LPSTR) "PDW Add Filter");', 'SetWindowText(hDlg, (LPSTR) "PDW - Filter toevoegen");', "add-filter runtime title")
pdw = replace_once(pdw, 'SetWindowText(hDlg, (LPSTR) multiple_edit ? "PDW (multiple) Edit Filter" : "PDW Edit Filter");', 'SetWindowText(hDlg, (LPSTR) multiple_edit ? "PDW - Meerdere filters bewerken" : "PDW - Filter bewerken");', "edit-filter runtime title")
pdw = replace_all_checked(pdw, '"No sound"', '"Geen geluid"', "audio no-sound label")
pdw = replace_all_checked(pdw, '"Default"', '"Standaard"', "visible default labels")
pdw = replace_all_checked(pdw, 'Enter the capcode/riccode to filter. Use ? as wildcard.', 'Voer de capcode/riccode in. Gebruik ? als jokerteken.', "filter help capcode")

# The resource group caption and checkbox occupied effectively the same line.
finish = replace_once(
    finish,
    '    CONTROL         "Afzonderlijk filterbestand inschakelen",IDC_SEPFILTERFILEEN,"Button",BS_AUTOCHECKBOX | WS_DISABLED | WS_TABSTOP,28,304,184,12',
    '    CONTROL         "Afzonderlijk filterbestand inschakelen",IDC_SEPFILTERFILEEN,"Button",BS_AUTOCHECKBOX | WS_DISABLED | WS_TABSTOP,28,316,184,12',
    "separate-filter checkbox spacing",
)
finish = replace_once(
    finish,
    '    PUSHBUTTON      "Bladeren...",IDC_SEPFILTERFILEBROWSE1,28,324,62,18,WS_DISABLED',
    '    PUSHBUTTON      "Bladeren...",IDC_SEPFILTERFILEBROWSE1,28,336,62,18,WS_DISABLED',
    "separate-filter browse spacing",
)
finish = replace_once(
    finish,
    '    EDITTEXT        IDC_SEPFILTERFILE1,96,324,306,18,ES_AUTOHSCROLL | ES_READONLY | NOT WS_TABSTOP',
    '    EDITTEXT        IDC_SEPFILTERFILE1,96,336,306,18,ES_AUTOHSCROLL | ES_READONLY | NOT WS_TABSTOP',
    "separate-filter edit spacing",
)

write(ui_path, ui)
write(pane_path, pane)
write(sig_path, sig)
write(pdw_path, pdw)
write(finish_path, finish)
print("Applied buffered UI, live signal meter, softer surfaces and Dutch visual acceptance fixes.")
