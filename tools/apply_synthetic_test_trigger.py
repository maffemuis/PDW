from pathlib import Path


def replace_once(data: bytes, old_lf: bytes, new_lf: bytes, path: Path) -> bytes:
    variants = [
        (old_lf.replace(b"\n", b"\r\n"), new_lf.replace(b"\n", b"\r\n")),
        (old_lf, new_lf),
    ]
    matches = [(old, new) for old, new in variants if data.count(old) == 1]
    if len(matches) != 1:
        counts = [data.count(old) for old, _ in variants]
        raise SystemExit(f"{path}: expected one newline-specific anchor, counts={counts}")
    old, new = matches[0]
    return data.replace(old, new, 1)


FILES = {
    Path("Headers/Resource.h"): [
        (
            b"#define IDM_FILTERCOMMANDFILE      245\n",
            b"#define IDM_FILTERCOMMANDFILE      245\n#define IDM_TEST_MESSAGE            246\n",
        ),
    ],
    Path("Rsrc.rc"): [
        (
            b'        MENUITEM "Filter Command File",         IDM_FILTERCOMMANDFILE\n',
            b'        MENUITEM "Filter Command File",         IDM_FILTERCOMMANDFILE\n        MENUITEM SEPARATOR\n        MENUITEM "Send TEST/SYNTHETIC message...", IDM_TEST_MESSAGE\n',
        ),
    ],
    Path("PDW.cpp"): [
        (
            b'#include "utils\\smtp.h"\n',
            b'#include "utils\\smtp.h"\n#include "utils\\synthetic_injection.h"\n',
        ),
        (
            b'\t\t\t\tcase IDM_DEBUG:\n',
            b'\t\t\t\tcase IDM_TEST_MESSAGE:\n\t\t\t\t\tif (MessageBox(hWnd, "Inject a clearly marked TEST/SYNTHETIC message through the normal filter/action pipeline?\\n\\nCapcode: 1234567\\nText: PDW TEST MESSAGE", "PDW Synthetic Test", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES)\n\t\t\t\t\t{\n\t\t\t\t\t\tif (!pdw::InjectDefaultSyntheticTestMessage())\n\t\t\t\t\t\t{\n\t\t\t\t\t\t\tMessageBox(hWnd, "Synthetic test injection was rejected safely.", "PDW Synthetic Test", MB_OK | MB_ICONERROR);\n\t\t\t\t\t\t}\n\t\t\t\t\t}\n\t\t\t\t\tbreak;\n\n\t\t\t\tcase IDM_DEBUG:\n',
        ),
    ],
}

for path, replacements in FILES.items():
    data = path.read_bytes()
    original = data
    for old, new in replacements:
        data = replace_once(data, old, new, path)
    if data == original:
        raise SystemExit(f"{path}: patch made no change")
    path.write_bytes(data)

print("synthetic test trigger patch applied")
