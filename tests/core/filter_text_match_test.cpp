#include "filter_text_match.h"

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
    using pdw::MatchTextFilterExpression;
    using pdw::ParseTextFilterExpression;

    expect(MatchTextFilterExpression(ParseTextFilterExpression("GROTE BRAND", false), "P 1 GROTE BRAND INDUSTRIE"),
           "legacy single-text contains match");
    expect(!MatchTextFilterExpression(ParseTextFilterExpression("GROTE BRAND", false), "P 1 MIDDELBRAND"),
           "legacy single-text non-match");

    const auto or_filter = ParseTextFilterExpression("GROTE BRAND;ZEER GROTE BRAND;PELOTON", false);
    expect(MatchTextFilterExpression(or_filter, "P 1 ZEER GROTE BRAND BEDRIJF"), "semicolon OR alternative");
    expect(MatchTextFilterExpression(or_filter, "PELOTON TER PLAATSE"), "last OR alternative");
    expect(!MatchTextFilterExpression(or_filter, "MIDDELBRAND"), "OR non-match");

    const auto and_filter = ParseTextFilterExpression("BRAND&INDUSTRIE;GRIP 1&REGIO", false);
    expect(MatchTextFilterExpression(and_filter, "INDUSTRIEBRAND IN LOODS"), "legacy ampersand AND within alternative");
    expect(MatchTextFilterExpression(and_filter, "REGIO OPSCHALING GRIP 1"), "AND second OR alternative");
    expect(!MatchTextFilterExpression(and_filter, "BRAND WONING"), "AND requires every term");

    const auto starts = ParseTextFilterExpression("^GROTE BRAND;^GRIP 1", false);
    expect(MatchTextFilterExpression(starts, "grote brand woning"), "start-of-message is case insensitive");
    expect(!MatchTextFilterExpression(starts, "P 1 GROTE BRAND WONING"), "start-of-message rejects interior match");

    const auto exact = ParseTextFilterExpression("GROTE BRAND;GRIP 1", true);
    expect(MatchTextFilterExpression(exact, "grote brand"), "exact per alternative");
    expect(MatchTextFilterExpression(exact, "GRIP 1"), "exact second alternative");
    expect(!MatchTextFilterExpression(exact, "P 1 GROTE BRAND"), "exact rejects surrounding text");

    const auto empties = ParseTextFilterExpression(";GROTE BRAND;;PELOTON;", false);
    expect(empties.alternatives.size() == 2, "empty and trailing semicolon tokens ignored");
    expect(MatchTextFilterExpression(empties, "PELOTON"), "trailing semicolon keeps valid alternatives");

    std::string long_term(1024, 'A');
    const auto bounded = ParseTextFilterExpression(long_term + ";BRAND", false);
    expect(MatchTextFilterExpression(bounded, long_term), "long bounded alternative remains matchable");
    expect(MatchTextFilterExpression(bounded, "BRAND"), "long alternative does not break following alternative");

    std::cout << "filter_text_match: OK" << std::endl;
    return 0;
}
