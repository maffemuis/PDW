#include "filter_multi_edit.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
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
        row.capcode = "CAP" + std::to_string(i);
        row.text = "OLD";
        row.label = "OLDLABEL";
        row.match_exact_msg = 0;
        row.wave_number = static_cast<int>(i % 4) - 1;
        row.label_color = static_cast<int>(i % 17);
        row.label_enabled = static_cast<int>(i % 2);
        row.monitor_only = static_cast<int>((i + 1) % 2);
        row.cmd_enabled = static_cast<int>(i % 2);
        row.smtp = static_cast<int>((i + 1) % 2);
        row.sep_filterfile_en = static_cast<int>(i % 2);
        row.sep_filterfile = {"one" + std::to_string(i) + ".txt", "two" + std::to_string(i) + ".txt", "three" + std::to_string(i) + ".txt"};
        rows.push_back(row);
        selected.push_back(i);
    }

    const std::vector<pdw::FilterEditRow> baseline = rows;
    pdw::FilterMultiEdit no_change;
    pdw::ApplyFilterMultiEdit(rows, selected, no_change);
    for (std::size_t i = 0; i < rows.size(); ++i)
        expect(same_row(rows[i], baseline[i]), "default multi-edit must preserve complete row");

    pdw::FilterMultiEdit edit;
    edit.change_type = true; edit.type = 2;
    edit.change_capcode = true; edit.capcode = "1234567";
    edit.change_text = true; edit.text = "GROTE BRAND;PELOTON";
    edit.change_label = true; edit.label = "P2000";
    edit.change_match_exact_msg = true; edit.match_exact_msg = 1;
    edit.change_wave_number = true; edit.wave_number = 3;
    edit.change_label_color = true; edit.label_color = 5;
    edit.change_label_enabled = true; edit.label_enabled = 1;
    edit.change_monitor_only = true; edit.monitor_only = 0;
    edit.change_cmd_enabled = true; edit.cmd_enabled = 1;
    edit.change_smtp = true; edit.smtp = 1;
    edit.change_sep_filterfile_en = true; edit.sep_filterfile_en = 1;
    edit.change_sep_filterfile = {true, false, true};
    edit.sep_filterfile = {"override-1.txt", "unused.txt", "override-3.txt"};
    pdw::ApplyFilterMultiEdit(rows, selected, edit);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        expect(rows[i].type == 2, "filter type override");
        expect(rows[i].capcode == "1234567", "capcode override");
        expect(rows[i].text == "GROTE BRAND;PELOTON", "text override");
        expect(rows[i].label == "P2000", "label override");
        expect(rows[i].match_exact_msg == 1, "exact override");
        expect(rows[i].wave_number == 3, "audio override");
        expect(rows[i].label_color == 5, "color override");
        expect(rows[i].label_enabled == 1, "show-label override");
        expect(rows[i].monitor_only == 0, "monitor-only override");
        expect(rows[i].cmd_enabled == 1, "command override");
        expect(rows[i].smtp == 1, "send-email override");
        expect(rows[i].sep_filterfile_en == 1, "separate-file enable override");
        expect(rows[i].sep_filterfile[0] == "override-1.txt", "separate-file slot 1 override");
        expect(rows[i].sep_filterfile[1] == baseline[i].sep_filterfile[1], "separate-file slot 2 preserved");
        expect(rows[i].sep_filterfile[2] == "override-3.txt", "separate-file slot 3 override");
    }

    selected.push_back(9999);
    const std::size_t size_before = rows.size();
    pdw::ApplyFilterMultiEdit(rows, selected, no_change);
    expect(rows.size() == size_before, "out-of-range selection is bounds-safe");

    pdw::FilterMultiEdit text_only;
    text_only.change_text = true;
    text_only.text = "FOLLOWUP";
    std::vector<std::size_t> one(1, 7);
    const pdw::FilterEditRow before_sparse = rows[7];
    pdw::ApplyFilterMultiEdit(rows, one, text_only);
    pdw::FilterEditRow expected_sparse = before_sparse;
    expected_sparse.text = "FOLLOWUP";
    expect(same_row(rows[7], expected_sparse), "sparse override preserves non-selected fields");

    std::cout << "filter_multi_edit: OK" << std::endl;
    return 0;
}
