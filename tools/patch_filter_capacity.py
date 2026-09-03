from pathlib import Path


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


header_path = Path("Headers/pdw.h")
header = header_path.read_text(encoding="latin-1")
header = replace_once(
    header,
    "#define FILTER_LABEL_LEN    256\t// was 70, see issue #21",
    "#define FILTER_LABEL_LEN    1024\t// expanded modern label capacity",
    "FILTER_LABEL_LEN",
)
header = replace_once(
    header,
    "#define FILTER_FILE_LEN     128\t// PH: was 256",
    "#define FILTER_FILE_LEN     128\t// PH: was 256\n#define FILTER_SOUND_COUNT  100\t// Sound0.wav through Sound99.wav",
    "FILTER_SOUND_COUNT insertion",
)
header_path.write_text(header, encoding="latin-1")

pdw_path = Path("PDW.cpp")
pdw = pdw_path.read_text(encoding="latin-1")
old_wave_decl = (
    '\tchar *wave_names[11]  = {"Default","Sound-0","Sound-1","Sound-2","Sound-3","Sound-4",\n'
    '\t\t\t\t\t\t\t\t\t   "Sound-5","Sound-6","Sound-7","Sound-8","Sound-9"};'
)
pdw = replace_once(pdw, old_wave_decl, "\tchar wave_name[32];", "wave display table")
pdw = replace_once(
    pdw,
    '\t\t\tstrcat(temp_str, filter.wave_number == -1 ? "NoSound" : wave_names[filter.wave_number]);',
    '''\t\t\tif (filter.wave_number == -1)
\t\t\t{
\t\t\t\tstrcat(temp_str, "NoSound");
\t\t\t}
\t\t\telse if (filter.wave_number == 0)
\t\t\t{
\t\t\t\tstrcat(temp_str, "Default");
\t\t\t}
\t\t\telse
\t\t\t{
\t\t\t\tsprintf(wave_name, "Sound-%i", filter.wave_number-1);
\t\t\t\tstrcat(temp_str, wave_name);
\t\t\t}''',
    "dynamic wave display",
)
pdw = replace_once(
    pdw,
    "\t\t\t\tfor (i=0; i<10; i++)",
    "\t\t\t\tfor (i=0; i<FILTER_SOUND_COUNT; i++)",
    "initial sound combo count",
)
pdw = replace_once(
    pdw,
    "\t\t\t\tfor (i=0; i<11; i++)",
    "\t\t\t\tfor (i=0; i<FILTER_SOUND_COUNT; i++)",
    "monitor-toggle sound combo count",
)
pdw = replace_once(
    pdw,
    "(Profile.filters[index].monitor_only ? 2 : 12)",
    "(Profile.filters[index].monitor_only ? 2 : FILTER_SOUND_COUNT + 2)",
    "save audio sentinel",
)
pdw = replace_once(
    pdw,
    "(WPARAM) filter.monitor_only ? 2 : 12",
    "(WPARAM) (filter.monitor_only ? 2 : FILTER_SOUND_COUNT + 2)",
    "initial audio sentinel",
)
pdw = replace_once(
    pdw,
    "(WPARAM) monitor_only ? 2 : 12",
    "(WPARAM) (monitor_only ? 2 : FILTER_SOUND_COUNT + 2)",
    "monitor-toggle audio sentinel",
)
pdw = replace_once(
    pdw,
    "void WriteFilters(PPROFILE pProfile, int backup)\n{\n\tchar szLine[256];",
    "void WriteFilters(PPROFILE pProfile, int backup)\n{\n\tchar szLine[MAX_STR_LEN];",
    "filter serialization buffer",
)

label_start = pdw.index("\t\t\t\t\t\t\tcase FILTER_LABEL:\t\t\t// Get label")
label_end = pdw.index("\t\t\t\t\t\t\tcase FILTER_TEXT:\t\t\t// Get text", label_start)
old_label_block = pdw[label_start:label_end]
if "strncpy(filter.label" not in old_label_block:
    raise SystemExit("label parser block did not contain expected legacy copy")
new_label_block = '''\t\t\t\t\t\t\tcase FILTER_LABEL:\t\t\t// Get label

\t\t\t\t\t\t\tif (szLine[pos+1] != '\"')
\t\t\t\t\t\t\t{
\t\t\t\t\t\t\t\tconst char *label_start = &szLine[pos+1];
\t\t\t\t\t\t\t\tconst char *label_end = strchr(label_start, '\"');
\t\t\t\t\t\t\t\tif (!label_end)
\t\t\t\t\t\t\t\t{
\t\t\t\t\t\t\t\t\tbError = true;
\t\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t\t}

\t\t\t\t\t\t\t\tconst size_t label_len = static_cast<size_t>(label_end - label_start);
\t\t\t\t\t\t\t\tif (label_len > FILTER_LABEL_LEN)
\t\t\t\t\t\t\t\t{
\t\t\t\t\t\t\t\t\tbError = true;
\t\t\t\t\t\t\t\t\tbreak;
\t\t\t\t\t\t\t\t}

\t\t\t\t\t\t\t\tmemcpy(filter.label, label_start, label_len);
\t\t\t\t\t\t\t\tfilter.label[label_len] = 0;
\t\t\t\t\t\t\t\tpos = static_cast<int>(label_end - szLine);
\t\t\t\t\t\t\t}
\t\t\t\t\t\t\telse filter.label[0] = 0;

\t\t\t\t\t\t\tbreak;

'''
pdw = pdw[:label_start] + new_label_block + pdw[label_end:]
pdw_path.write_text(pdw, encoding="latin-1")

misc_path = Path("Misc.cpp")
misc = misc_path.read_text(encoding="latin-1")
misc = replace_once(
    misc,
    "#define FILTER_PARAM_LEN\t500",
    "#define FILTER_PARAM_LEN\t(MAX_STR_LEN - 1)",
    "command parameter capacity",
)
display_marker = '''void display_show_strV2(PaneStruct *pane, char strin[])
{
\tfor (int x=0; ((strin[x] != 0) && (x < 256)); x++)
\t{
\t\tbuild_show_line(pane, strin[x], 0);
\t}
}
'''
display_replacement = display_marker + '''
// Filter labels can be substantially longer than legacy pane fields. Keep the
// wider limit local to labels so normal legacy field rendering stays unchanged.
void display_show_filter_label(PaneStruct *pane, const char strin[])
{
\tfor (int x=0; ((strin[x] != 0) && (x < FILTER_LABEL_LEN)); x++)
\t{
\t\tbuild_show_line(pane, strin[x], 0);
\t}
}
'''
misc = replace_once(misc, display_marker, display_replacement, "label display helper")
for pane, expected in (("Pane1", 2), ("Pane2", 1)):
    old = f"display_show_strV2(&{pane}, szCurrentLabel[0])"
    count = misc.count(old)
    if count != expected:
        raise SystemExit(f"{pane} label display calls: expected {expected}, found {count}")
    misc = misc.replace(old, f"display_show_filter_label(&{pane}, szCurrentLabel[0])")

misc = replace_once(
    misc,
    "if ((i > 254) || (arg_pos > FILTER_PARAM_LEN)) break;",
    "if ((i > 254) || (arg_pos >= FILTER_PARAM_LEN)) break;",
    "command outer bound",
)
misc = replace_once(
    misc,
    "for (pos=0; Current_MSG[arg][pos] != 0; pos++, arg_pos++)",
    "for (pos=0; Current_MSG[arg][pos] != 0 && arg_pos < FILTER_PARAM_LEN; pos++, arg_pos++)",
    "command message expansion bound",
)
misc = replace_once(
    misc,
    "while (szLabel[pos] != 0)",
    "while (szLabel[pos] != 0 && arg_pos < FILTER_PARAM_LEN)",
    "command label expansion bound",
)
if misc.count("while (tmp[pos] != 0)") != 2:
    raise SystemExit("command cycle/frame loops changed")
misc = misc.replace("while (tmp[pos] != 0)", "while (tmp[pos] != 0 && arg_pos < FILTER_PARAM_LEN)")
old_command = '''\tstrcpy(szCommandFile, Profile.filter_cmd);

\tif (param_str[0])
\t{
\t\tstrcat(szCommandFile, " ");
\t\tstrcat(szCommandFile, param_str);
\t}
\tif (strlen(szCommandFile) > MAX_STR_LEN) szCommandFile[MAX_STR_LEN] = 0;'''
new_command = '''\t_snprintf(szCommandFile, sizeof(szCommandFile)-1, "%s%s%s",
\t\tProfile.filter_cmd, param_str[0] ? " " : "", param_str);
\tszCommandFile[sizeof(szCommandFile)-1] = '\\0';'''
misc = replace_once(misc, old_command, new_command, "command line bounded construction")
misc_path.write_text(misc, encoding="latin-1")

assert "FILTER_LABEL_LEN    1024" in header
assert "FILTER_SOUND_COUNT  100" in header
assert "wave_names[11]" not in pdw
assert "i<10; i++" not in pdw
assert "i<11; i++" not in pdw
assert "? 2 : 12" not in pdw
assert "char szLine[256];" not in pdw[pdw.index("void WriteFilters"):]
print("filter capacity patch applied")
