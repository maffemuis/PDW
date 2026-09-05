#ifndef PDW_WINDOWS11_UI_H
#define PDW_WINDOWS11_UI_H

#include <windows.h>

// windowsx.h provides these helpers, but it also defines a SelectFont macro
// that collides with PDW's legacy SelectFont function. Keep the dependency
// surface small and provide only the signed coordinate helpers we need.
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace pdw {

// Applies Windows 11 non-client styling to the main PDW window while
// preserving the existing decoder panes and command IDs.
void ApplyWindows11MainWindowStyle(HWND hwnd);

// Installs a thread-local hook that modernizes legacy resource dialogs after
// WM_INITDIALOG, without changing any existing dialog procedures or IDs.
void InstallWindows11DialogStyling();

// Applies modern non-client styling, system UI fonts and themed common
// controls to a legacy dialog without changing its command/control IDs.
void ApplyWindows11DialogStyle(HWND hwnd);

// Applies the modern Explorer visual style to an existing common control.
void ApplyWindows11ControlStyle(HWND hwnd);

// True after the modern shell has been enabled. Legacy drawing helpers use
// this to avoid painting obsolete chrome over the Windows 11 command surface.
bool IsWindows11ChromeEnabled();

} // namespace pdw

#endif
