#pragma once

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
