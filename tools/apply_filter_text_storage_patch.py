#!/usr/bin/env python3
from pathlib import Path


def read_legacy(path: str):
    raw = Path(path).read_bytes()
    text = raw.decode("latin-1")
    newline = "\r\n" if "\r\n" in text else "\n"
    return text, newline


def write_legacy(path: str, text: str):
    Path(path).write_bytes(text.encode("latin-1"))


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


# Expand the FILTER struct text field while keeping the limit explicit and bounded.
pdw_h, nl = read_legacy("Headers/pdw.h")
old_define = "#define FILTER_TEXT_LEN     40\t// PH: was 25"
new_define = "#define FILTER_TEXT_LEN     255\t// modern bounded filter text capacity"
if new_define not in pdw_h:
    pdw_h = replace_once(pdw_h, old_define, new_define, "FILTER_TEXT_LEN")
write_legacy("Headers/pdw.h", pdw_h)

# Wire the tested parser into legacy FILTERS.INI loading without transcoding the file.
pdw_cpp, nl = read_legacy("PDW.cpp")
include_anchor = '#include "utils\\smtp.h"' + nl
include_line = '#include "core\\filter_text_storage.h"' + nl
if include_line not in pdw_cpp:
    pdw_cpp = replace_once(
        pdw_cpp,
        include_anchor,
        include_anchor + include_line,
        "filter_text_storage include",
    )

old_block = nl.join([
    "\t\t\t\t\t\t\t\tstrncpy(filter.text, &szLine[pos+1], strlen(szLine));",
    "\t\t\t\t\t\t\t\tfilter.text[strchr(filter.text, '\"') - filter.text] = 0;",
    "\t\t\t\t\t\t\t\tpos += strlen(filter.text) + 1;\t// + 1 to start at last \"",
])
new_block = nl.join([
    "\t\t\t\t\t\t\t\tpdw::LegacyFilterTextParseResult parsed_text =",
    "\t\t\t\t\t\t\t\t\tpdw::ExtractLegacyQuotedFilterText(szLine, pos, FILTER_TEXT_LEN);",
    "\t\t\t\t\t\t\t\tif (!parsed_text.ok)",
    "\t\t\t\t\t\t\t\t{",
    "\t\t\t\t\t\t\t\t\tfclose(pFile);",
    "\t\t\t\t\t\t\t\t\treturn false;",
    "\t\t\t\t\t\t\t\t}",
    "\t\t\t\t\t\t\t\tstrcpy(filter.text, parsed_text.text.c_str());",
    "\t\t\t\t\t\t\t\tpos = parsed_text.closing_quote;",
])
if "ExtractLegacyQuotedFilterText(szLine, pos, FILTER_TEXT_LEN)" not in pdw_cpp:
    pdw_cpp = replace_once(pdw_cpp, old_block, new_block, "legacy FILTER_TEXT parser")

if "strncpy(filter.text, &szLine[pos+1], strlen(szLine))" in pdw_cpp:
    raise SystemExit("unsafe FILTER_TEXT copy still present")
if "FILTER_TEXT_LEN     255" not in pdw_h:
    raise SystemExit("FILTER_TEXT_LEN did not update")

write_legacy("PDW.cpp", pdw_cpp)
print("legacy filter-text storage patch: OK")
