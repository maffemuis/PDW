from pathlib import Path

path = Path("PDW.cpp")
text = path.read_text(encoding="latin-1")
old = '''\tchar temp_cap[FILTER_CAPCODE_LEN+1]="",
\t\t temp[MAX_PATH],
\t\t tmp_text[FILTER_TEXT_LEN+1],'''
new = '''\tchar temp_cap[FILTER_CAPCODE_LEN+1]="",
\t\t temp[MAX_STR_LEN],
\t\t tmp_text[FILTER_TEXT_LEN+1],'''
count = text.count(old)
if count != 1:
    raise SystemExit(f"expected exactly one FilterEdit temporary buffer block, found {count}")
path.write_text(text.replace(old, new, 1), encoding="latin-1")
print("FilterEdit display buffer widened to MAX_STR_LEN")
