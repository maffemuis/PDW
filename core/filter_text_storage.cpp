#include "filter_text_storage.h"

namespace pdw {

LegacyFilterTextParseResult ExtractLegacyQuotedFilterText(const std::string& line,
                                                           std::size_t opening_quote,
                                                           std::size_t max_length) {
    LegacyFilterTextParseResult result;
    if (opening_quote >= line.size() || line[opening_quote] != '"') return result;

    const std::size_t close = line.find('"', opening_quote + 1);
    if (close == std::string::npos) return result;

    const std::size_t length = close - opening_quote - 1;
    if (length > max_length) return result;

    result.ok = true;
    result.text = line.substr(opening_quote + 1, length);
    result.closing_quote = close;
    return result;
}

} // namespace pdw
