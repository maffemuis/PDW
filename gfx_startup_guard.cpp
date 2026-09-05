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

void SetMessageItemPositionsWidth()
{
    if (cxChar == 0) return;
    LegacySetMessageItemPositionsWidth();
}