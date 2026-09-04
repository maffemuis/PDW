#!/usr/bin/env python3
from pathlib import Path

PATH = Path("PDW.cpp")
raw = PATH.read_bytes()
text = raw.decode("utf-8")
nl = "\r\n" if "\r\n" in text else "\n"


def replace_once(old: str, new: str, label: str):
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one occurrence, found {count}")
    text = text.replace(old, new, 1)


include_anchor = '#include "core\\filter_text_storage.h"' + nl
include_line = '#include "core\\filter_text_match.h"' + nl
if include_line not in text:
    replace_once(include_anchor, include_anchor + include_line, "filter matcher include")

old_caret = nl.join([
    "\t\t\t\tif ((strchr(filter.text, '^') - filter.text) > 0)",
    "\t\t\t\t{",
    "\t\t\t\t\tMessageBox(hDlg, \"The character ^ can only be used\\nat the beginning of the line\",\"PDW Filter Text\", MB_ICONERROR);",
    "\t\t\t\t\tSetFocus(GetDlgItem(hDlg, IDC_FILTERTEXT));",
    "\t\t\t\t\tpos = strchr(filter.text, '^') - filter.text;",
    "\t\t\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERTEXT, EM_SETSEL, pos, pos+1);",
    "\t\t\t\t\treturn (FALSE);",
    "\t\t\t\t}",
])
new_caret = nl.join([
    "\t\t\t\tconst bool exact_message = IsDlgButtonChecked(hDlg, IDC_FILTERMATCHEXACT) == BST_CHECKED;",
    "\t\t\t\tconst size_t invalid_caret = pdw::FindInvalidTextFilterCaret(filter.text, exact_message);",
    "\t\t\t\tif (invalid_caret != std::string::npos)",
    "\t\t\t\t{",
    "\t\t\t\t\tMessageBox(hDlg, \"The character ^ can only be used at the beginning of each ; alternative\",\"PDW Filter Text\", MB_ICONERROR);",
    "\t\t\t\t\tSetFocus(GetDlgItem(hDlg, IDC_FILTERTEXT));",
    "\t\t\t\t\tpos = (int)invalid_caret;",
    "\t\t\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERTEXT, EM_SETSEL, pos, pos+1);",
    "\t\t\t\t\treturn (FALSE);",
    "\t\t\t\t}",
])
if "FindInvalidTextFilterCaret(filter.text" not in text:
    replace_once(old_caret, new_caret, "caret validation")

old_capcode = "\t\t\t\tif (strncmp(temp_cap, \"Don't cha\", 9))\t// If not \"Don't cha(nge)\""
new_capcode = "\t\t\t\tif (!multiple_edit && strncmp(temp_cap, \"Don't cha\", 9))\t// Capcode is immutable during multi-edit"
if new_capcode not in text:
    replace_once(old_capcode, new_capcode, "multi-edit capcode preservation")

if "if ((strchr(filter.text, '^') - filter.text) > 0)" in text:
    raise SystemExit("unsafe legacy caret validation remains")
if "if (strncmp(temp_cap, \"Don't cha\", 9))\t// If not \"Don't cha(nge)\"" in text:
    raise SystemExit("multi-edit capcode write remains")

PATH.write_bytes(text.encode("utf-8"))
print("filter editor UI patch: OK")
