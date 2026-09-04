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

} // namespace

int main() {
    std::vector<pdw::FilterEditRow> rows;
    std::vector<std::size_t> selected;

    for (std::size_t i = 0; i < 25; ++i) {
        pdw::FilterEditRow row;
        row.capcode = "CAP" + std::to_string(i);
        row.text = "OLD";
        row.label = "OLDLABEL";
        row.match_exact_msg = 0;
        rows.push_back(row);
        selected.push_back(i);
    }

    pdw::FilterMultiEdit edit;
    edit.change_text = true;
    edit.text = "GROTE BRAND;PELOTON";
    edit.change_label = true;
    edit.label = "P2000";
    edit.change_match_exact_msg = true;
    edit.match_exact_msg = 1;

    pdw::ApplyFilterMultiEdit(rows, selected, edit);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        expect(rows[i].capcode == ("CAP" + std::to_string(i)), "default multi-edit preserves every mixed capcode");
        expect(rows[i].text == "GROTE BRAND;PELOTON", "multi-edit updates common text");
        expect(rows[i].label == "P2000", "multi-edit updates common label");
        expect(rows[i].match_exact_msg == 1, "multi-edit updates exact flag");
    }

    // Explicit capcode override intentionally applies one bounded value to all selected rows.
    pdw::FilterMultiEdit capcode_override;
    capcode_override.change_capcode = true;
    capcode_override.capcode = "1234567";
    pdw::ApplyFilterMultiEdit(rows, selected, capcode_override);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        expect(rows[i].capcode == "1234567", "explicit capcode override applies to every selected row");
    }

    // Same-capcode selections remain editable via the same explicit override path.
    std::vector<std::size_t> same_selected;
    same_selected.push_back(0);
    same_selected.push_back(1);
    rows[0].capcode = "7654321";
    rows[1].capcode = "7654321";
    capcode_override.capcode = "1111111";
    pdw::ApplyFilterMultiEdit(rows, same_selected, capcode_override);
    expect(rows[0].capcode == "1111111" && rows[1].capcode == "1111111", "equal capcodes are explicitly editable");

    // Invalid/oversized explicit values fail closed and preserve existing rows.
    expect(pdw::IsValidFilterMultiEditCapcode("123456789"), "9-character capcode fits legacy storage bound");
    expect(!pdw::IsValidFilterMultiEditCapcode(""), "empty explicit capcode is rejected");
    expect(!pdw::IsValidFilterMultiEditCapcode("1234567890"), "oversized capcode is rejected");
    const std::string before0 = rows[0].capcode;
    const std::string before1 = rows[1].capcode;
    capcode_override.capcode = "1234567890";
    pdw::ApplyFilterMultiEdit(rows, same_selected, capcode_override);
    expect(rows[0].capcode == before0 && rows[1].capcode == before1, "invalid capcode override preserves rows fail-closed");

    // Out-of-range selections are ignored rather than indexing past the row vector.
    selected.push_back(9999);
    pdw::ApplyFilterMultiEdit(rows, selected, edit);
    expect(rows.size() == 25, "out-of-range selection is bounds-safe");

    // Selective edits leave untouched fields intact.
    pdw::FilterMultiEdit text_only;
    text_only.change_text = true;
    text_only.text = "BRAND";
    std::vector<std::size_t> one(1, 7);
    const std::string original_label = rows[7].label;
    const std::string original_capcode = rows[7].capcode;
    pdw::ApplyFilterMultiEdit(rows, one, text_only);
    expect(rows[7].text == "BRAND", "single selected row text updated");
    expect(rows[7].label == original_label, "unchanged label preserved");
    expect(rows[7].capcode == original_capcode, "single edit preserves capcode without override");

    std::cout << "filter_multi_edit: OK" << std::endl;
    return 0;
}
