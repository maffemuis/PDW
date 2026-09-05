from pathlib import Path


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    return text.replace(old, new, 1)


path = Path("PDW.cpp")
raw = path.read_bytes()
text = raw.decode("latin-1")
nl = "\r\n" if "\r\n" in text else "\n"

def lines(value):
    return value.replace("\n", nl)

text = replace_once(
    text,
    "int type=0, monitor_only=0, reject=0, match_exact=0, label_en=0, smtp=0;",
    "int type=0, filter_type=0, monitor_only=0, reject=0, match_exact=0, label_en=0, smtp=0;",
    "filter type mixed flag",
)

text = replace_once(
    text,
    lines("""\t\t\tfor (i=0; i<6; i++)\t\t// show filter types
\t\t\t{
\t\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_ADDSTRING, 0, (LPARAM) (LPSTR) types[i]);
\t\t\t}
"""),
    lines("""\t\t\tfor (i=0; i<6; i++)\t\t// show filter types
\t\t\t{
\t\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_ADDSTRING, 0, (LPARAM) (LPSTR) types[i]);
\t\t\t}
\t\t\tif (multiple_edit)
\t\t\t{
\t\t\t\tSendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_ADDSTRING, 0, (LPARAM) (LPSTR) "Don't change");
\t\t\t}
"""),
    "multi-edit type dont-change item",
)

text = replace_once(
    text,
    "\t\t\t\t\tif (strcmp(Profile.filters[index].capcode, filter.capcode) != 0) capcode=1;",
    lines("""\t\t\t\t\tif (Profile.filters[index].type != filter.type) filter_type=1;
\t\t\t\t\tif (strcmp(Profile.filters[index].capcode, filter.capcode) != 0) capcode=1;"""),
    "detect mixed filter type",
)

text = replace_once(
    text,
    "\t\tSendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_SETCURSEL, (WPARAM) filter.type-1, 0L);",
    lines("""\t\tSendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_SETCURSEL,
\t\t\t(WPARAM) ((multiple_edit && filter_type) ? 6 : filter.type-1), 0L);"""),
    "type initial selection",
)

text = replace_once(
    text,
    "\t\ttype\t\t  = (FILTER_TYPE) (SendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_GETCURSEL, 0, 0L)+1);",
    lines("""\t\t{
\t\t\tint type_selection = SendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_GETCURSEL, 0, 0L);
\t\t\ttype = (multiple_edit && type_selection == 6) ? filter.type : (FILTER_TYPE) (type_selection+1);
\t\t}"""),
    "type dont-change ui state",
)

text = replace_once(
    text,
    lines("""\t\t\tif (HIWORD(wParam) == CBN_SELENDOK)
\t\t\t{
\t\t\t\tfilter.type = (FILTER_TYPE) (SendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_GETCURSEL, 0, 0L)+1);

\t\t\t\tif (filter.type == TEXT_FILTER)  // if TEXT filter"""),
    lines("""\t\t\tif (HIWORD(wParam) == CBN_SELENDOK)
\t\t\t{
\t\t\t\tint type_selection = SendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_GETCURSEL, 0, 0L);
\t\t\t\tif (multiple_edit && type_selection == 6)
\t\t\t\t{
\t\t\t\t\tSendMessage(hDlg, WM_WININICHANGE, initializing, 0L);
\t\t\t\t\tbreak;
\t\t\t\t}
\t\t\t\tfilter.type = (FILTER_TYPE) (type_selection+1);

\t\t\t\tif (filter.type == TEXT_FILTER)  // if TEXT filter"""),
    "type explicit override handler",
)

text = replace_once(
    text,
    lines("""\t\tif (IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY) != BST_INDETERMINATE)
\t\t{
\t\t\t EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), (!reject && captxt));
\t\t}
\t\telse EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), false);"""),
    lines("""\t\tif (IsDlgButtonChecked(hDlg, IDC_FILTER_MONITOR_ONLY) != BST_INDETERMINATE)
\t\t{
\t\t\t EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), (!reject && captxt));
\t\t}
\t\telse EnableWindow(GetDlgItem(hDlg, IDC_FILTERAUDIO), multiple_edit ? true : false);"""),
    "mixed monitor audio override usability",
)

text = replace_once(
    text,
    lines("""\t\t\t\tif (!filter.reject)
\t\t\t\t{"""),
    lines("""\t\t\t\tint type_selection = SendDlgItemMessage(hDlg, IDC_FILTERTYPE, CB_GETCURSEL, 0, 0L);
\t\t\t\tif (!(multiple_edit && type_selection == 6))
\t\t\t\t{
\t\t\t\t\tProfile.filters[index].type = (FILTER_TYPE) (type_selection+1);
\t\t\t\t}

\t\t\t\tif (!filter.reject)
\t\t\t\t{"""),
    "persist explicit filter type",
)

path.write_bytes(text.encode("latin-1"))

Path("core/filter_multi_edit.h").write_text("""#pragma once

#include <array>
#include <string>
#include <vector>

namespace pdw {

struct FilterEditRow {
    int type = 0;
    std::string capcode;
    std::string text;
    std::string label;
    int match_exact_msg = 0;
    int wave_number = -1;
    int label_color = 0;
    int label_enabled = 0;
    int monitor_only = 0;
    int cmd_enabled = 0;
    int smtp = 0;
    int sep_filterfile_en = 0;
    std::array<std::string, 3> sep_filterfile{};
};

struct FilterMultiEdit {
    bool change_type = false;
    int type = 0;
    bool change_capcode = false;
    std::string capcode;
    bool change_text = false;
    std::string text;
    bool change_label = false;
    std::string label;
    bool change_match_exact_msg = false;
    int match_exact_msg = 0;
    bool change_wave_number = false;
    int wave_number = -1;
    bool change_label_color = false;
    int label_color = 0;
    bool change_label_enabled = false;
    int label_enabled = 0;
    bool change_monitor_only = false;
    int monitor_only = 0;
    bool change_cmd_enabled = false;
    int cmd_enabled = 0;
    bool change_smtp = false;
    int smtp = 0;
    bool change_sep_filterfile_en = false;
    int sep_filterfile_en = 0;
    std::array<bool, 3> change_sep_filterfile{};
    std::array<std::string, 3> sep_filterfile{};
};

void ApplyFilterMultiEdit(std::vector<FilterEditRow>& rows,
                          const std::vector<std::size_t>& selected_indices,
                          const FilterMultiEdit& edit);

} // namespace pdw
""", encoding="utf-8", newline="\n")

Path("core/filter_multi_edit.cpp").write_text("""#include \"filter_multi_edit.h\"

namespace pdw {

void ApplyFilterMultiEdit(std::vector<FilterEditRow>& rows,
                          const std::vector<std::size_t>& selected_indices,
                          const FilterMultiEdit& edit) {
    for (std::size_t i = 0; i < selected_indices.size(); ++i) {
        const std::size_t index = selected_indices[i];
        if (index >= rows.size()) continue;

        FilterEditRow& row = rows[index];
        if (edit.change_type) row.type = edit.type;
        if (edit.change_capcode) row.capcode = edit.capcode;
        if (edit.change_text) row.text = edit.text;
        if (edit.change_label) row.label = edit.label;
        if (edit.change_match_exact_msg) row.match_exact_msg = edit.match_exact_msg;
        if (edit.change_wave_number) row.wave_number = edit.wave_number;
        if (edit.change_label_color) row.label_color = edit.label_color;
        if (edit.change_label_enabled) row.label_enabled = edit.label_enabled;
        if (edit.change_monitor_only) row.monitor_only = edit.monitor_only;
        if (edit.change_cmd_enabled) row.cmd_enabled = edit.cmd_enabled;
        if (edit.change_smtp) row.smtp = edit.smtp;
        if (edit.change_sep_filterfile_en) row.sep_filterfile_en = edit.sep_filterfile_en;
        for (std::size_t j = 0; j < row.sep_filterfile.size(); ++j) {
            if (edit.change_sep_filterfile[j]) row.sep_filterfile[j] = edit.sep_filterfile[j];
        }
    }
}

} // namespace pdw
""", encoding="utf-8", newline="\n")

Path("tests/core/filter_multi_edit_test.cpp").write_text("""#include \"filter_multi_edit.h\"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << \"FAILED: \" << message << std::endl;
        std::exit(1);
    }
}

bool same_row(const pdw::FilterEditRow& a, const pdw::FilterEditRow& b) {
    return a.type == b.type && a.capcode == b.capcode && a.text == b.text &&
           a.label == b.label && a.match_exact_msg == b.match_exact_msg &&
           a.wave_number == b.wave_number && a.label_color == b.label_color &&
           a.label_enabled == b.label_enabled && a.monitor_only == b.monitor_only &&
           a.cmd_enabled == b.cmd_enabled && a.smtp == b.smtp &&
           a.sep_filterfile_en == b.sep_filterfile_en && a.sep_filterfile == b.sep_filterfile;
}
} // namespace

int main() {
    std::vector<pdw::FilterEditRow> rows;
    std::vector<std::size_t> selected;
    for (std::size_t i = 0; i < 25; ++i) {
        pdw::FilterEditRow row;
        row.type = (i % 2) ? 1 : 2;
        row.capcode = \"CAP\" + std::to_string(i);
        row.text = \"OLD\";
        row.label = \"OLDLABEL\";
        row.match_exact_msg = 0;
        row.wave_number = static_cast<int>(i % 4) - 1;
        row.label_color = static_cast<int>(i % 17);
        row.label_enabled = static_cast<int>(i % 2);
        row.monitor_only = static_cast<int>((i + 1) % 2);
        row.cmd_enabled = static_cast<int>(i % 2);
        row.smtp = static_cast<int>((i + 1) % 2);
        row.sep_filterfile_en = static_cast<int>(i % 2);
        row.sep_filterfile = {\"one\" + std::to_string(i) + \".txt\", \"two\" + std::to_string(i) + \".txt\", \"three\" + std::to_string(i) + \".txt\"};
        rows.push_back(row);
        selected.push_back(i);
    }

    const std::vector<pdw::FilterEditRow> baseline = rows;
    pdw::FilterMultiEdit no_change;
    pdw::ApplyFilterMultiEdit(rows, selected, no_change);
    for (std::size_t i = 0; i < rows.size(); ++i)
        expect(same_row(rows[i], baseline[i]), \"default multi-edit must preserve complete row\");

    pdw::FilterMultiEdit edit;
    edit.change_type = true; edit.type = 2;
    edit.change_capcode = true; edit.capcode = \"1234567\";
    edit.change_text = true; edit.text = \"GROTE BRAND;PELOTON\";
    edit.change_label = true; edit.label = \"P2000\";
    edit.change_match_exact_msg = true; edit.match_exact_msg = 1;
    edit.change_wave_number = true; edit.wave_number = 3;
    edit.change_label_color = true; edit.label_color = 5;
    edit.change_label_enabled = true; edit.label_enabled = 1;
    edit.change_monitor_only = true; edit.monitor_only = 0;
    edit.change_cmd_enabled = true; edit.cmd_enabled = 1;
    edit.change_smtp = true; edit.smtp = 1;
    edit.change_sep_filterfile_en = true; edit.sep_filterfile_en = 1;
    edit.change_sep_filterfile = {true, false, true};
    edit.sep_filterfile = {\"override-1.txt\", \"unused.txt\", \"override-3.txt\"};
    pdw::ApplyFilterMultiEdit(rows, selected, edit);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        expect(rows[i].type == 2, \"filter type override\");
        expect(rows[i].capcode == \"1234567\", \"capcode override\");
        expect(rows[i].text == \"GROTE BRAND;PELOTON\", \"text override\");
        expect(rows[i].label == \"P2000\", \"label override\");
        expect(rows[i].match_exact_msg == 1, \"exact override\");
        expect(rows[i].wave_number == 3, \"audio override\");
        expect(rows[i].label_color == 5, \"color override\");
        expect(rows[i].label_enabled == 1, \"show-label override\");
        expect(rows[i].monitor_only == 0, \"monitor-only override\");
        expect(rows[i].cmd_enabled == 1, \"command override\");
        expect(rows[i].smtp == 1, \"send-email override\");
        expect(rows[i].sep_filterfile_en == 1, \"separate-file enable override\");
        expect(rows[i].sep_filterfile[0] == \"override-1.txt\", \"separate-file slot 1 override\");
        expect(rows[i].sep_filterfile[1] == baseline[i].sep_filterfile[1], \"separate-file slot 2 preserved\");
        expect(rows[i].sep_filterfile[2] == \"override-3.txt\", \"separate-file slot 3 override\");
    }

    selected.push_back(9999);
    const std::size_t size_before = rows.size();
    pdw::ApplyFilterMultiEdit(rows, selected, no_change);
    expect(rows.size() == size_before, \"out-of-range selection is bounds-safe\");

    pdw::FilterMultiEdit text_only;
    text_only.change_text = true;
    text_only.text = \"FOLLOWUP\";
    std::vector<std::size_t> one(1, 7);
    const pdw::FilterEditRow before_sparse = rows[7];
    pdw::ApplyFilterMultiEdit(rows, one, text_only);
    pdw::FilterEditRow expected_sparse = before_sparse;
    expected_sparse.text = \"FOLLOWUP\";
    expect(same_row(rows[7], expected_sparse), \"sparse override preserves non-selected fields\");

    std::cout << \"filter_multi_edit: OK\" << std::endl;
    return 0;
}
""", encoding="utf-8", newline="\n")
