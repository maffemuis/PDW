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
    using pdw::FindTextFilterExpression;
    using pdw::MatchTextFilterExpression;
    using pdw::ParseTextFilterExpression;

    expect(MatchTextFilterExpression(ParseTextFilterExpression("GROTE BRAND", false), "P 1 GROTE BRAND INDUSTRIE"),
           "legacy single-text contains match");
    expect(!MatchTextFilterExpression(ParseTextFilterExpression("GROTE BRAND", false), "P 1 MIDDELBRAND"),
           "legacy single-text non-match");

    const auto contains_result = FindTextFilterExpression(ParseTextFilterExpression("GROTE BRAND", false),
                                                           "P 1 GROTE BRAND INDUSTRIE");
    expect(contains_result.matched && contains_result.spans.size() == 1,
           "contains match exposes one highlight span");
    expect(contains_result.spans[0].position == 4 && contains_result.spans[0].length == 11,
           "contains span preserves message coordinates");

    const auto or_filter = ParseTextFilterExpression("GROTE BRAND;ZEER GROTE BRAND;PELOTON", false);
    expect(MatchTextFilterExpression(or_filter, "P 1 ZEER GROTE BRAND BEDRIJF"), "semicolon OR alternative");
    expect(MatchTextFilterExpression(or_filter, "PELOTON TER PLAATSE"), "last OR alternative");
    expect(!MatchTextFilterExpression(or_filter, "MIDDELBRAND"), "OR non-match");

    // Existing PDW '&' filters use strstr from the previous hit onward:
    // matching is case-sensitive and terms must occur in order.
    const auto and_filter = ParseTextFilterExpression("BRAND&INDUSTRIE;GRIP 1&REGIO", false);
    expect(MatchTextFilterExpression(and_filter, "BRAND IN INDUSTRIEGEBIED"), "legacy ampersand ordered match");
    expect(MatchTextFilterExpression(and_filter, "GRIP 1 OPSCHALING REGIO"), "legacy ampersand second OR alternative");
    expect(!MatchTextFilterExpression(and_filter, "INDUSTRIE VOOR BRAND"), "legacy ampersand preserves term order");
    expect(!MatchTextFilterExpression(and_filter, "brand IN INDUSTRIEGEBIED"), "legacy ampersand remains case sensitive");
    expect(!MatchTextFilterExpression(and_filter, "BRAND IN industriegebied"), "legacy ampersand remains case sensitive for later terms");

    const auto and_result = FindTextFilterExpression(and_filter, "BRAND IN INDUSTRIEGEBIED");
    expect(and_result.matched && and_result.spans.size() == 2,
           "ampersand match exposes all ordered highlight spans");
    expect(and_result.spans[0].position == 0 && and_result.spans[1].position == 9,
           "ampersand span coordinates stay ordered");

    // More than ten '&' terms used to exceed the legacy ten-entry highlight arrays.
    // Matching remains deterministic while stored spans are explicitly bounded.
    const auto many_terms = ParseTextFilterExpression("A&B&C&D&E&F&G&H&I&J&K&L", false);
    const auto many_result = FindTextFilterExpression(many_terms, "A B C D E F G H I J K L", 8);
    expect(many_result.matched, "more than ten ampersand terms still match");
    expect(many_result.spans.size() == 8, "highlight span collection obeys explicit bound");

    // Legacy precedence is exact -> '^' -> '&'. Therefore '&' is literal when
    // exact-message mode is active or when the filter starts with '^'.
    const auto starts_literal_amp = ParseTextFilterExpression("^BRAND&INDUSTRIE", false);
    expect(MatchTextFilterExpression(starts_literal_amp, "brand&industrie vervolg"), "legacy start anchor keeps ampersand literal");
    expect(!MatchTextFilterExpression(starts_literal_amp, "BRAND INDUSTRIE"), "start anchor must not activate ampersand semantics");

    const auto exact_literal_amp = ParseTextFilterExpression("BRAND&INDUSTRIE", true);
    expect(MatchTextFilterExpression(exact_literal_amp, "brand&industrie"), "legacy exact mode keeps ampersand literal");
    expect(!MatchTextFilterExpression(exact_literal_amp, "BRAND INDUSTRIE"), "exact mode does not activate ampersand semantics");

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

    const auto spaced_or = ParseTextFilterExpression(" GROTE BRAND ; PELOTON ", false);
    expect(MatchTextFilterExpression(spaced_or, "P 1 GROTE BRAND WONING"), "semicolon alternatives trim surrounding whitespace");
    expect(MatchTextFilterExpression(spaced_or, "PELOTON TER PLAATSE"), "trimmed second semicolon alternative");

    // Without ';', whitespace remains part of the legacy expression. Likewise,
    // '&' terms and text after '^' must not be silently trimmed.
    const auto legacy_spaced = ParseTextFilterExpression(" GROTE BRAND ", false);
    expect(MatchTextFilterExpression(legacy_spaced, "P 1 GROTE BRAND WONING"),
           "legacy single expression preserves surrounding spaces when present in message");
    expect(!MatchTextFilterExpression(legacy_spaced, "P 1 GROTE BRAND-WONING"),
           "legacy single expression does not trim surrounding spaces");

    const auto amp_spaced = ParseTextFilterExpression("BRAND &INDUSTRIE", false);
    expect(MatchTextFilterExpression(amp_spaced, "BRAND INDUSTRIE"),
           "legacy ampersand preserves trailing space in first term");
    expect(!MatchTextFilterExpression(amp_spaced, "BRAND-X INDUSTRIE"),
           "legacy ampersand does not trim term whitespace");

    const auto starts_spaced = ParseTextFilterExpression("^ BRAND", false);
    expect(MatchTextFilterExpression(starts_spaced, " brand melding"),
           "legacy start anchor preserves space immediately after caret");
    expect(!MatchTextFilterExpression(starts_spaced, "BRAND MELDING"),
           "legacy start anchor does not trim after caret");

    std::string long_term(1024, 'A');
    const auto bounded = ParseTextFilterExpression(long_term + ";BRAND", false);
    expect(MatchTextFilterExpression(bounded, long_term), "long bounded alternative remains matchable");
    expect(MatchTextFilterExpression(bounded, "BRAND"), "long alternative does not break following alternative");

    std::cout << "filter_text_match: OK" << std::endl;
    return 0;
}
