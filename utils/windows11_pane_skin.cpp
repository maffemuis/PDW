#include "windows11_ui.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <string.h>

#include "..\\Headers\\pdw.h"
#include "..\\Headers\\gfx.h"
#include "..\\Headers\\initapp.h"

extern unsigned int iSelectionStartCol;
extern unsigned int iSelectionStartRow;
extern unsigned int iSelectionEndCol;
extern unsigned int iSelectionEndRow;

namespace {

const UINT_PTR kPane1SubclassId = 0x50445741;
const UINT_PTR kPane2SubclassId = 0x50445742;

COLORREF EnsureLightSurfaceContrast(COLORREF color)
{
    const int red = GetRValue(color);
    const int green = GetGValue(color);
    const int blue = GetBValue(color);
    const int luminance = (red * 299 + green * 587 + blue * 114) / 1000;

    // Legacy profiles were often tuned for a black monitor background. Keep
    // configured semantic colors where possible, but prevent near-white text
    // from disappearing on the new light workspace.
    if (luminance > 218)
        return RGB(45, 52, 61);
    return color;
}

COLORREF MessageColor(BYTE color)
{
    switch (color)
    {
        case COLOR_ADDRESS:       return EnsureLightSurfaceContrast(Profile.color_address);
        case COLOR_TIMESTAMP:     return EnsureLightSurfaceContrast(Profile.color_timestamp);
        case COLOR_MODETYPEBIT:   return EnsureLightSurfaceContrast(Profile.color_modetypebit);
        case COLOR_MESSAGE:       return EnsureLightSurfaceContrast(Profile.color_message);
        case COLOR_NUMERIC:       return EnsureLightSurfaceContrast(Profile.color_numeric);
        case COLOR_MISC:          return EnsureLightSurfaceContrast(Profile.color_misc);
        case COLOR_BITERRORS:     return EnsureLightSurfaceContrast(Profile.color_biterrors);
        case COLOR_FILTERMATCH:   return EnsureLightSurfaceContrast(Profile.color_filtermatch);
        case COLOR_INSTRUCTIONS:  return EnsureLightSurfaceContrast(Profile.color_instructions);
        case COLOR_AC_MESSAGE_NR: return EnsureLightSurfaceContrast(Profile.color_ac_message_nr);
        case COLOR_AC_DBI:        return EnsureLightSurfaceContrast(Profile.color_ac_dbi);
        case COLOR_MB_SENDER:     return EnsureLightSurfaceContrast(Profile.color_mb_sender);
        default:
            if (color >= COLOR_FILTERLABEL && color <= COLOR_FILTERLABEL + 16)
                return EnsureLightSurfaceContrast(Profile.color_filterlabel[color - COLOR_FILTERLABEL]);
            return RGB(35, 42, 51);
    }
}

bool SelectionActiveForPane(PaneStruct* pane)
{
    return pane && select_pane == pane && select_on != 0 &&
           (selected != 0 || selecting != 0);
}

void DrawModernSelection(HDC hdc, PaneStruct* pane, int row,
                         int cellWidth, int cellHeight, int clientRight)
{
    if (!SelectionActiveForPane(pane)) return;

    const unsigned int minRow = iSelectionStartRow < iSelectionEndRow
        ? iSelectionStartRow : iSelectionEndRow;
    const unsigned int maxRow = iSelectionStartRow > iSelectionEndRow
        ? iSelectionStartRow : iSelectionEndRow;
    if (static_cast<unsigned int>(row) < minRow || static_cast<unsigned int>(row) > maxRow)
        return;

    const unsigned int minCol = iSelectionStartCol < iSelectionEndCol
        ? iSelectionStartCol : iSelectionEndCol;
    const unsigned int maxCol = iSelectionStartCol > iSelectionEndCol
        ? iSelectionStartCol : iSelectionEndCol;

    int left = static_cast<int>(minCol) * cellWidth;
    int right = static_cast<int>(maxCol + 1) * cellWidth;
    if (left < 0) left = 0;
    if (right > clientRight) right = clientRight;
    if (right <= left) return;

    RECT selection = {
        left,
        row * cellHeight,
        right,
        (row + 1) * cellHeight
    };
    HBRUSH brush = CreateSolidBrush(RGB(214, 232, 250));
    FillRect(hdc, &selection, brush);
    DeleteObject(brush);
}

void DrawPaneRows(HWND hwnd, HDC hdc, PaneStruct* pane)
{
    RECT client = {};
    GetClientRect(hwnd, &client);

    HBRUSH base = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &client, base);
    DeleteObject(base);

    if (!pane || !pane->buff_char || !pane->buff_color || cxChar == 0 || cyChar == 0)
        return;

    // Reuse PDW's configured message font. This keeps the existing Font
    // setting fully functional while only replacing the surrounding visual
    // presentation and paint path.
    HGDIOBJ oldFont = hfont ? SelectObject(hdc, hfont) : NULL;
    SetBkMode(hdc, TRANSPARENT);

    const int cellWidth = static_cast<int>(cxChar);
    const int cellHeight = static_cast<int>(cyChar);
    const int visibleRows = (client.bottom + cellHeight - 1) / cellHeight;
    const int firstColumn = pane->iHscrollPos > 0 ? pane->iHscrollPos : 0;
    int visibleColumns = (client.right + cellWidth - 1) / cellWidth + 1;
    if (visibleColumns > LINE_SIZE) visibleColumns = LINE_SIZE;

    for (int row = 0; row < visibleRows; ++row)
    {
        const int lineNumber = pane->iVscrollPos + row;
        if (lineNumber < 0 || static_cast<unsigned int>(lineNumber) >= pane->buff_lines)
            break;

        RECT rowRect = { 0, row * cellHeight, client.right, (row + 1) * cellHeight };
        if ((row & 1) != 0)
        {
            HBRUSH alternate = CreateSolidBrush(RGB(250, 252, 254));
            FillRect(hdc, &rowRect, alternate);
            DeleteObject(alternate);
        }

        DrawModernSelection(hdc, pane, row, cellWidth, cellHeight, client.right);

        const int offset = lineNumber * (LINE_SIZE + 1);
        int column = firstColumn;
        const int lastColumn = firstColumn + visibleColumns < LINE_SIZE
            ? firstColumn + visibleColumns
            : LINE_SIZE;

        while (column < lastColumn)
        {
            const char current = pane->buff_char[offset + column];
            if (current == '\0') break;

            const BYTE color = pane->buff_color[offset + column];
            const int runStart = column;
            char run[LINE_SIZE + 1] = {};
            int runLength = 0;

            while (column < lastColumn && runLength < LINE_SIZE)
            {
                const char value = pane->buff_char[offset + column];
                if (value == '\0' || pane->buff_color[offset + column] != color)
                    break;
                run[runLength++] = static_cast<unsigned char>(value) < 32 ? ' ' : value;
                ++column;
            }

            if (runLength == 0)
            {
                ++column;
                continue;
            }

            SetTextColor(hdc, MessageColor(color));
            const int x = (runStart - firstColumn) * cellWidth;
            const int y = row * cellHeight;
            TextOutA(hdc, x, y, run, runLength);
        }

        HPEN separator = CreatePen(PS_SOLID, 1, RGB(238, 242, 246));
        HGDIOBJ oldPen = SelectObject(hdc, separator);
        MoveToEx(hdc, 0, rowRect.bottom - 1, NULL);
        LineTo(hdc, client.right, rowRect.bottom - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(separator);
    }

    if (oldFont) SelectObject(hdc, oldFont);
}

LRESULT CALLBACK ModernPaneSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR subclassId,
                                        DWORD_PTR referenceData)
{
    PaneStruct* pane = reinterpret_cast<PaneStruct*>(referenceData);

    switch (message)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            DrawPaneRows(hwnd, hdc, pane);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_PRINTCLIENT:
            DrawPaneRows(hwnd, reinterpret_cast<HDC>(wParam), pane);
            return 0;

        case WM_VSCROLL:
        case WM_HSCROLL:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN:
        case WM_SIZE:
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        {
            // Let the original pane procedure own all interaction semantics.
            // We only repaint its result so the old XOR/legacy visual never
            // becomes the persistent presentation layer.
            const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
            InvalidateRect(hwnd, NULL, FALSE);
            return result;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, ModernPaneSubclassProc, subclassId);
            break;
    }

    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void AttachModernPane(PaneStruct* pane, UINT_PTR subclassId)
{
    if (!pane || !pane->hWnd || !IsWindow(pane->hWnd)) return;

    DWORD_PTR existing = 0;
    if (!GetWindowSubclass(pane->hWnd, ModernPaneSubclassProc, subclassId, &existing))
    {
        SetWindowSubclass(
            pane->hWnd,
            ModernPaneSubclassProc,
            subclassId,
            reinterpret_cast<DWORD_PTR>(pane));
    }

    // Keep the existing pane HWND for scrolling, selection and command
    // behavior, but remove its legacy etched border. The standard scrollbars
    // remain native controls and therefore retain their existing behavior.
    LONG_PTR style = GetWindowLongPtr(pane->hWnd, GWL_STYLE);
    if ((style & WS_BORDER) != 0)
    {
        style &= ~static_cast<LONG_PTR>(WS_BORDER);
        SetWindowLongPtr(pane->hWnd, GWL_STYLE, style);
        SetWindowPos(pane->hWnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    }

    SetWindowTheme(pane->hWnd, L"Explorer", NULL);
    InvalidateRect(pane->hWnd, NULL, FALSE);
}

} // namespace

namespace pdw {

void EnsureWindows11PaneStyle()
{
    if (!IsWindows11ChromeEnabled()) return;
    AttachModernPane(&Pane1, kPane1SubclassId);
    AttachModernPane(&Pane2, kPane2SubclassId);
}

} // namespace pdw
