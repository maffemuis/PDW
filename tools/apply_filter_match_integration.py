#!/usr/bin/env python3
from pathlib import Path
import re

PATH = Path("Misc.cpp")
raw = PATH.read_bytes()
text = raw.decode("latin-1")
nl = "\r\n" if "\r\n" in text else "\n"


def replace_once(old, new, label):
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected 1 occurrence, found {count}")
    text = text.replace(old, new, 1)


include_anchor = '#include "utils\\smtp.h"' + nl
include_line = '#include "core\\filter_text_match.h"' + nl
if include_line not in text:
    replace_once(include_anchor, include_anchor + include_line, "matcher include")

replace_once(
    "int iTextPositions[10], iTextLengths[10];",
    "#define FILTER_MATCH_SPAN_MAX 64" + nl + "int iTextPositions[FILTER_MATCH_SPAN_MAX], iTextLengths[FILTER_MATCH_SPAN_MAX];",
    "match span arrays",
)

# Permit a match at message position zero; length is the sentinel, not position.
old = "for (k=0; k<10 && iTextPositions[k]; k++)"
new = "for (k=0; k<FILTER_MATCH_SPAN_MAX && iTextLengths[k]; k++)"
replace_once(old, new, "highlight loop")

replace_once(
    "for (i=0; i<10 && iTextLengths[i]; i++)",
    "for (i=0; i<FILTER_MATCH_SPAN_MAX && iTextLengths[i]; i++)",
    "span reset loop 1",
)
replace_once(
    "for (i=0; i<10; i++)" + nl + "\t\t\t\t\t\t\t\t{\n".replace("\n", nl),
    "for (i=0; i<FILTER_MATCH_SPAN_MAX; i++)" + nl + "\t\t\t\t\t\t\t\t{" + nl,
    "span reset loop 2",
)

start_marker = "\t\t// Is there (also) text that must be matched?" + nl
end_marker = "\t\tif (Profile.filters[iFilter].type == TEXT_FILTER || (iAddrMatch == iFilter))" + nl
start = text.find(start_marker)
end = text.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("text matching block markers not found")

new_block = nl.join([
    "\t\t// Is there (also) text that must be matched?",
    "\t\tif (Profile.filters[iFilter].type == TEXT_FILTER || (iAddrMatch == iFilter) && Profile.filters[iFilter].text[0])",
    "\t\t{",
    "\t\t\tpdw::TextFilterExpression expression = pdw::ParseTextFilterExpression(",
    "\t\t\t\tProfile.filters[iFilter].text, Profile.filters[iFilter].match_exact_msg != 0);",
    "\t\t\tpdw::TextMatchResult match = pdw::FindTextFilterExpression(",
    "\t\t\t\texpression, Current_MSG[MSG_MESSAGE], FILTER_MATCH_SPAN_MAX);",
    "",
    "\t\t\tfor (i=0; i<FILTER_MATCH_SPAN_MAX; i++)",
    "\t\t\t{",
    "\t\t\t\tiTextPositions[i]=0;",
    "\t\t\t\tiTextLengths[i]=0;",
    "\t\t\t}",
    "",
    "\t\t\tif (match.matched)",
    "\t\t\t{",
    "\t\t\t\tfor (i=0; i<(int)match.spans.size() && i<FILTER_MATCH_SPAN_MAX; i++)",
    "\t\t\t\t{",
    "\t\t\t\t\tiTextPositions[i]=(int)match.spans[i].position;",
    "\t\t\t\t\tiTextLengths[i]=(int)match.spans[i].length;",
    "\t\t\t\t}",
    "\t\t\t\tiTextMatch = iFilter;",
    "\t\t\t\tiTextLength = match.spans.empty() ? 0 : (int)match.spans[0].length;",
    "\t\t\t}",
    "",
]) + nl
text = text[:start] + new_block + text[end:]

# The old matcher and its ten-term cap must be gone.
for forbidden in [
    "while (Profile.filters[iFilter].text[i] != 0 && l < 10)",
    "int iTextPositions[10], iTextLengths[10];",
    "k<10 && iTextPositions[k]",
]:
    if forbidden in text:
        raise SystemExit(f"legacy matcher fragment still present: {forbidden}")

PATH.write_bytes(text.encode("latin-1"))
print("legacy filter matcher integration: OK")
