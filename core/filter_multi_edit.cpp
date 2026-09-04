#include "filter_multi_edit.h"

#include <algorithm>

namespace pdw {

void ApplyFilterMultiEdit(std::vector<FilterEditRow>& rows,
                          const std::vector<std::size_t>& selected_indices,
                          const FilterMultiEdit& edit) {
    for (std::size_t i = 0; i < selected_indices.size(); ++i) {
        const std::size_t index = selected_indices[i];
        if (index >= rows.size()) continue;

        FilterEditRow& row = rows[index];
        const std::string original_capcode = row.capcode;

        if (edit.change_text) row.text = edit.text;
        if (edit.change_label) row.label = edit.label;
        if (edit.change_match_exact_msg) row.match_exact_msg = edit.match_exact_msg;

        // Multi-edit must never collapse distinct capcodes into one shared value.
        row.capcode = original_capcode;
    }
}

} // namespace pdw
