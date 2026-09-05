// Startup guard for legacy GDI title-bar layout.
//
// Windows 11 theming can cause a paint while the main window is still inside
// WM_CREATE. At that point PDW has not measured its configured font yet, so
// cxChar/cyChar are still zero. The legacy layout divides by cxChar. Keep the
// original Gfx.cpp implementation intact and only enter it after those metrics
// are valid.

#ifndef STRICT
#define STRICT 1
#endif

#include <windows.h>
#include <stdio.h>

#include "headers\PDW.h"
#include "headers\gfx.h"
#include "headers\initapp.h"

void LegacyDrawTitleBarGfx(HWND hwnd);
void LegacySetMessageItemPositionsWidth();

void DrawTitleBarGfx(HWND hwnd)
{
    if (!hwnd || cxChar == 0 || cyChar == 0) return;
    LegacyDrawTitleBarGfx(hwnd);
}

void SetMessageItemPositionsWidth()
{
    if (cxChar == 0) return;
    LegacySetMessageItemPositionsWidth();
}
