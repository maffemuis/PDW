#include "filter_multi_edit.h"

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
