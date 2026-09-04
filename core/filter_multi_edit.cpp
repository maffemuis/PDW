#include "filter_multi_edit.h"

namespace pdw {

bool IsValidFilterMultiEditCapcode(const std::string& capcode) {
    return !capcode.empty() && capcode.size() <= kFilterCapcodeMaxLength;
}

void ApplyFilterMultiEdit(std::vector<FilterEditRow>& rows,
                          const std::vector<std::size_t>& selected_indices,
                          const FilterMultiEdit& edit) {
    const bool apply_capcode = edit.change_capcode && IsValidFilterMultiEditCapcode(edit.capcode);

    for (std::size_t i = 0; i < selected_indices.size(); ++i) {
        const std::size_t index = selected_indices[i];
        if (index >= rows.size()) continue;

        FilterEditRow& row = rows[index];

        // Mixed/default multi-edit keeps every row's existing capcode. A capcode
        // changes only after an explicit, bounded override request.
        if (apply_capcode) row.capcode = edit.capcode;
        if (edit.change_text) row.text = edit.text;
        if (edit.change_label) row.label = edit.label;
        if (edit.change_match_exact_msg) row.match_exact_msg = edit.match_exact_msg;
    }
}

} // namespace pdw
