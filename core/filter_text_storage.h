#pragma once

#include <cstddef>
#include <string>

namespace pdw {

static const std::size_t kFilterTextMax = 255;

struct LegacyFilterTextParseResult {
    bool ok = false;
    std::string text;
    std::size_t closing_quote = std::string::npos;
};

LegacyFilterTextParseResult ExtractLegacyQuotedFilterText(const std::string& line,
                                                           std::size_t opening_quote,
                                                           std::size_t max_length = kFilterTextMax);

} // namespace pdw
