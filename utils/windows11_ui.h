#ifndef PDW_WINDOWS11_UI_H
#define PDW_WINDOWS11_UI_H

#include <windows.h>

namespace pdw {

// Applies Windows 11 non-client styling to the main PDW window while
// preserving the existing Win32 client-area rendering and decoder UI.
void ApplyWindows11MainWindowStyle(HWND hwnd);

// Installs a thread-local hook that modernizes legacy resource dialogs after
// WM_INITDIALOG, without changing any existing dialog procedures or IDs.
void InstallWindows11DialogStyling();

// Applies modern non-client styling, system UI fonts and themed common
// controls to a legacy dialog without changing its command/control IDs.
void ApplyWindows11DialogStyle(HWND hwnd);

// Applies the modern Explorer visual style to an existing common control.
void ApplyWindows11ControlStyle(HWND hwnd);

} // namespace pdw

#endif
