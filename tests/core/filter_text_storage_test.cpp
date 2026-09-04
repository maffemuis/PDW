#include "filter_text_storage.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        std::exit(1);
    }
}

} // namespace

int main() {
    using pdw::ExtractLegacyQuotedFilterText;
    using pdw::kFilterTextMax;

    const auto legacy = ExtractLegacyQuotedFilterText("4=\"BRANDWEER TEST\",5=0", 2);
    expect(legacy.ok && legacy.text == "BRANDWEER TEST", "legacy quoted filter text parses unchanged");

    const std::string old_limit(40, 'A');
    const auto old_roundtrip = ExtractLegacyQuotedFilterText("4=\"" + old_limit + "\"", 2);
    expect(old_roundtrip.ok && old_roundtrip.text == old_limit, "legacy 40-char text remains compatible");

    const std::string max_text(kFilterTextMax, 'B');
    const auto max_ok = ExtractLegacyQuotedFilterText("4=\"" + max_text + "\"", 2);
    expect(max_ok.ok && max_ok.text.size() == kFilterTextMax, "new bounded maximum is accepted exactly");

    const std::string too_long(kFilterTextMax + 1, 'C');
    const auto rejected = ExtractLegacyQuotedFilterText("4=\"" + too_long + "\"", 2);
    expect(!rejected.ok, "overlong filter text fails closed instead of truncating");

    const auto empty = ExtractLegacyQuotedFilterText("4=\"\"", 2);
    expect(empty.ok && empty.text.empty(), "empty quoted text remains parseable");

    const auto syntax = ExtractLegacyQuotedFilterText("4=\"^BRAND;PELOTON&REGIO;\"", 2);
    expect(syntax.ok && syntax.text == "^BRAND;PELOTON&REGIO;", "filter operators survive save/restart text parsing");

    const auto mixed_case = ExtractLegacyQuotedFilterText("4=\"BrAnDwEeR P2000\"", 2);
    expect(mixed_case.ok && mixed_case.text == "BrAnDwEeR P2000", "mixed case is preserved byte-for-byte");

    expect(!ExtractLegacyQuotedFilterText("4=BRAND\"", 2).ok, "missing opening quote rejected");
    expect(!ExtractLegacyQuotedFilterText("4=\"BRAND", 2).ok, "missing closing quote rejected");

    std::cout << "filter_text_storage: OK" << std::endl;
    return 0;
}
