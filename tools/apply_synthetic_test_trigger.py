from pathlib import Path

FILES = {
    Path("Headers/Resource.h"): [
        (
            b"#define IDM_FILTERCOMMANDFILE      245\r\n",
            b"#define IDM_FILTERCOMMANDFILE      245\r\n#define IDM_TEST_MESSAGE            246\r\n",
        ),
    ],
    Path("Rsrc.rc"): [
        (
            b'        MENUITEM "Filter Command File",         IDM_FILTERCOMMANDFILE\r\n',
            b'        MENUITEM "Filter Command File",         IDM_FILTERCOMMANDFILE\r\n        MENUITEM SEPARATOR\r\n        MENUITEM "Send TEST/SYNTHETIC message...", IDM_TEST_MESSAGE\r\n',
        ),
    ],
    Path("PDW.cpp"): [
        (
            b'#include "utils\\smtp.h"\r\n',
            b'#include "utils\\smtp.h"\r\n#include "utils\\synthetic_injection.h"\r\n',
        ),
        (
            b'\t\t\t\tcase IDM_DEBUG:\r\n',
            b'\t\t\t\tcase IDM_TEST_MESSAGE:\r\n\t\t\t\t\tif (MessageBox(hWnd, "Inject a clearly marked TEST/SYNTHETIC message through the normal filter/action pipeline?\\n\\nCapcode: 1234567\\nText: PDW TEST MESSAGE", "PDW Synthetic Test", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)\r\n\t\t\t\t\t{\r\n\t\t\t\t\t\tif (!pdw::InjectDefaultSyntheticTestMessage())\r\n\t\t\t\t\t\t{\r\n\t\t\t\t\t\t\tMessageBox(hWnd, "Synthetic test injection was rejected safely.", "PDW Synthetic Test", MB_OK | MB_ICONERROR);\r\n\t\t\t\t\t\t}\r\n\t\t\t\t\t}\r\n\t\t\t\t\tbreak;\r\n\r\n\t\t\t\tcase IDM_DEBUG:\r\n',
        ),
    ],
}

for path, replacements in FILES.items():
    data = path.read_bytes()
    original = data
    for old, new in replacements:
        count = data.count(old)
        if count != 1:
            raise SystemExit(f"{path}: expected exactly one anchor, found {count}")
        data = data.replace(old, new, 1)
    if data == original:
        raise SystemExit(f"{path}: patch made no change")
    path.write_bytes(data)

print("synthetic test trigger patch applied")
