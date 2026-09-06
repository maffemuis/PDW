// Startup guard for legacy GDI title-bar layout.
//
// The decoder/message panes stay intact as functional windows, but the
// Windows 11 shell owns all visible pane chrome and message painting. Legacy
// title bars therefore must not paint behind or through the modern workspace.

#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <stdio.h>

#include "headers\PDW.h"
#include "headers\gfx.h"
#include "headers\initapp.h"
#include "utils\windows11_ui.h"

void LegacyDrawTitleBarGfx(HWND hwnd);
void LegacyDrawPaneLabels(HWND hwnd, int pane);
void LegacySetMessageItemPositionsWidth();

void DrawTitleBarGfx(HWND hwnd)
{
    if (!hwnd || cxChar == 0 || cyChar == 0) return;
    if (pdw::IsWindows11ChromeEnabled())
    {
        pdw::EnsureWindows11PaneStyle();
        return;
    }
    LegacyDrawTitleBarGfx(hwnd);
}

void DrawPaneLabels(HWND hwnd, int pane)
{
    if (!hwnd || cxChar == 0 || cyChar == 0) return;
    if (pdw::IsWindows11ChromeEnabled())
    {
        // The modern shell owns pane headers, the live si_index meter and the
        // separate dRX_Quality percentage. Suppress every legacy pane-label
        // paint, including the old sigind bitmap path reached by SECOND_TIMER.
        pdw::EnsureWindows11PaneStyle();
        return;
    }
    LegacyDrawPaneLabels(hwnd, pane);
}

void SetMessageItemPositionsWidth()
{
    if (cxChar == 0) return;
    LegacySetMessageItemPositionsWidth();
    if (pdw::IsWindows11ChromeEnabled())
        pdw::EnsureWindows11PaneStyle();
}
