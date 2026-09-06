from pathlib import Path
import runpy

# The main helper intentionally uses exact-match guards. One Filter ListView
# block evolved after that helper was authored, so adapt that single known
# block here before executing the remaining bounded patch.
ui_path = Path("utils/windows11_ui.cpp")
ui_text = ui_path.read_text(encoding="utf-8-sig")
old_list = '''        SendMessage(list, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);
        if (!Profile.FilterWindowColors)
        {
            SendMessage(list, LVM_SETBKCOLOR, 0, RGB(255, 255, 255));
            SendMessage(list, LVM_SETTEXTBKCOLOR, 0, CLR_NONE);
        }'''
new_list = '''        SendMessage(list, WM_SETFONT,
                    reinterpret_cast<WPARAM>(GetFilterListFont()), TRUE);
        ListView_SetBkColor(list, RGB(247, 250, 252));
        ListView_SetTextBkColor(list, RGB(247, 250, 252));
        ListView_SetTextColor(list, RGB(32, 40, 48));'''
if ui_text.count(old_list) != 1:
    raise RuntimeError(f"current filter list block: expected 1 match, got {ui_text.count(old_list)}")
ui_path.write_text(ui_text.replace(old_list, new_list, 1), encoding="utf-8", newline="")

# Owner-drawn filter rows otherwise keep filling with the legacy profile's
# black monitor background. Keep label colors/selection semantics but use the
# modern light list surface until the central Dark/Light theme layer lands.
pdw_path = Path("PDW.cpp")
pdw_text = pdw_path.read_bytes().decode("cp1252")
old_owner_bg = "FillRect(lpdis->hDC, &rect, hb = CreateSolidBrush(Profile.color_background));"
new_owner_bg = "FillRect(lpdis->hDC, &rect, hb = CreateSolidBrush(RGB(247, 250, 252)));"
if pdw_text.count(old_owner_bg) != 1:
    raise RuntimeError(f"filter owner-draw background: expected 1 match, got {pdw_text.count(old_owner_bg)}")
pdw_path.write_bytes(pdw_text.replace(old_owner_bg, new_owner_bg, 1).encode("cp1252"))

# Skip only the now-preapplied stale ListView exact-match line in the main
# helper. All other guards remain active.
helper_path = Path(".github/scripts/apply_main_ui_stability.py")
helper_text = helper_path.read_text(encoding="utf-8")
stale_call = 'ui = replace_once(ui, needle, replacement, "light filter list")'
if helper_text.count(stale_call) != 1:
    raise RuntimeError("stale filter-list helper call was not unique")
helper_path.write_text(
    helper_text.replace(stale_call, '# Filter ListView block is preapplied by run_main_ui_stability.py', 1),
    encoding="utf-8",
    newline="",
)

_original_read_text = Path.read_text
_original_write_text = Path.write_text


def compatible_read_text(self, *args, **kwargs):
    try:
        return _original_read_text(self, *args, **kwargs)
    except UnicodeDecodeError:
        retry = dict(kwargs)
        retry["encoding"] = "cp1252"
        return _original_read_text(self, *args, **retry)


def compatible_write_text(self, data, *args, **kwargs):
    options = dict(kwargs)
    # PDW.cpp is still a legacy Windows-1252 source file. Preserve that byte
    # encoding so localization changes do not rewrite unrelated historical text.
    if self.name.lower() == "pdw.cpp":
        options["encoding"] = "cp1252"
    return _original_write_text(self, data, *args, **options)


Path.read_text = compatible_read_text
Path.write_text = compatible_write_text
runpy.run_path(".github/scripts/apply_main_ui_stability.py", run_name="__main__")
