#pragma once

#include <string>
#include <vector>

namespace pdw {

struct FilterEditRow {
    std::string capcode;
    std::string text;
    std::string label;
    int match_exact_msg = 0;
};

struct FilterMultiEdit {
    bool change_text = false;
    std::string text;
    bool change_label = false;
    std::string label;
    bool change_match_exact_msg = false;
    int match_exact_msg = 0;
};

void ApplyFilterMultiEdit(std::vector<FilterEditRow>& rows,
                          const std::vector<std::size_t>& selected_indices,
                          const FilterMultiEdit& edit);

} // namespace pdw
