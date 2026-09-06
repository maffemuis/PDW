from pathlib import Path

path = Path("utils/windows11_ui.cpp")
text = path.read_text(encoding="utf-8")

old_const = "const WPARAM kLegacySecondTimer = 103;\nconst int kSettingsPopupCommand = 50001;"
new_const = "// Legacy timer IDs are private to PDW.cpp; mirror only the two main-window\n// timers the modern shell observes. PDW_TIMER=101, SECOND_TIMER=103.\nconst WPARAM kLegacyDecodeTimer = 101;\nconst WPARAM kLegacySecondTimer = 103;\nconst int kSettingsPopupCommand = 50001;"
if text.count(old_const) != 1:
    raise RuntimeError(f"timer constant anchor: expected 1, got {text.count(old_const)}")
text = text.replace(old_const, new_const, 1)

old_refresh = "if (wParam == kLegacySecondTimer || wParam == PDW_TIMER)\n                DrawModernWorkspace(hwnd);"
new_refresh = "if (wParam == kLegacySecondTimer || wParam == kLegacyDecodeTimer)\n                DrawModernWorkspace(hwnd);"
if text.count(old_refresh) != 1:
    raise RuntimeError(f"timer refresh anchor: expected 1, got {text.count(old_refresh)}")
text = text.replace(old_refresh, new_refresh, 1)

path.write_text(text, encoding="utf-8", newline="")
print("Replaced out-of-scope PDW_TIMER with explicit legacy decode timer ID 101.")
