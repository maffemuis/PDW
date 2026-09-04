#include "filter_text_match.h"

#include <algorithm>
#include <cctype>

namespace pdw {
namespace {

std::string trim(const std::string& input) {
    std::string::size_type first = 0;
    while (first < input.size() && std::isspace(static_cast<unsigned char>(input[first]))) ++first;
    std::string::size_type last = input.size();
    while (last > first && std::isspace(static_cast<unsigned char>(input[last - 1]))) --last;
    return input.substr(first, last - first);
}

std::string upper(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return result;
}

TextMatchResult match_alternative(const TextAlternative& alt, const std::string& message,
                                  std::size_t max_spans) {
    TextMatchResult result;
    if (alt.terms.empty() || max_spans == 0) return result;

    if (alt.mode == TextMatchMode::Exact) {
        const std::string message_upper = upper(message);
        const std::string term_upper = upper(alt.terms.front());
        if (message_upper == term_upper) {
            result.matched = true;
            result.spans.push_back({0, message.size()});
        }
        return result;
    }

    if (alt.mode == TextMatchMode::StartsWith) {
        const std::string message_upper = upper(message);
        const std::string term_upper = upper(alt.terms.front());
        if (message_upper.size() >= term_upper.size() &&
            message_upper.compare(0, term_upper.size(), term_upper) == 0) {
            result.matched = true;
            result.spans.push_back({0, alt.terms.front().size()});
        }
        return result;
    }

    if (alt.mode == TextMatchMode::OrderedAnd) {
        std::size_t search_from = 0;
        for (std::size_t i = 0; i < alt.terms.size(); ++i) {
            const std::string& term = alt.terms[i];
            if (term.empty()) return TextMatchResult();
            const std::size_t found = message.find(term, search_from); // legacy '&' is case-sensitive
            if (found == std::string::npos) return TextMatchResult();
            if (result.spans.size() < max_spans) result.spans.push_back({found, term.size()});
            search_from = found + term.size();
        }
        result.matched = true;
        return result;
    }

    const std::string message_upper = upper(message);
    const std::string term_upper = upper(alt.terms.front());
    const std::size_t found = message_upper.find(term_upper);
    if (found != std::string::npos) {
        result.matched = true;
        result.spans.push_back({found, alt.terms.front().size()});
    }
    return result;
}

} // namespace

TextFilterExpression ParseTextFilterExpression(const std::string& expression, bool exact_message) {
    TextFilterExpression parsed;
    std::string::size_type alt_start = 0;

    while (alt_start <= expression.size()) {
        const std::string::size_type alt_end = expression.find(';', alt_start);
        std::string alternative_text = trim(expression.substr(
            alt_start,
            alt_end == std::string::npos ? std::string::npos : alt_end - alt_start));

        if (!alternative_text.empty()) {
            TextAlternative alt;

            // Preserve legacy precedence independently for every ';' alternative:
            // exact -> '^' -> '&' -> normal contains.
            if (exact_message) {
                alt.mode = TextMatchMode::Exact;
                alt.terms.push_back(alternative_text);
            } else if (alternative_text[0] == '^') {
                alt.mode = TextMatchMode::StartsWith;
                alternative_text = trim(alternative_text.substr(1));
                if (!alternative_text.empty()) alt.terms.push_back(alternative_text);
            } else if (alternative_text.find('&') != std::string::npos) {
                alt.mode = TextMatchMode::OrderedAnd;
                std::string::size_type term_start = 0;
                while (term_start <= alternative_text.size()) {
                    const std::string::size_type term_end = alternative_text.find('&', term_start);
                    const std::string term = trim(alternative_text.substr(
                        term_start,
                        term_end == std::string::npos ? std::string::npos : term_end - term_start));
                    if (!term.empty()) alt.terms.push_back(term);
                    if (term_end == std::string::npos) break;
                    term_start = term_end + 1;
                }
            } else {
                alt.mode = TextMatchMode::Contains;
                alt.terms.push_back(alternative_text);
            }

            if (!alt.terms.empty()) parsed.alternatives.push_back(alt);
        }

        if (alt_end == std::string::npos) break;
        alt_start = alt_end + 1;
    }

    return parsed;
}

TextMatchResult FindTextFilterExpression(const TextFilterExpression& expression, const std::string& message,
                                         std::size_t max_spans) {
    for (std::size_t i = 0; i < expression.alternatives.size(); ++i) {
        TextMatchResult result = match_alternative(expression.alternatives[i], message, max_spans);
        if (result.matched) return result;
    }
    return TextMatchResult();
}

bool MatchTextFilterExpression(const TextFilterExpression& expression, const std::string& message) {
    return FindTextFilterExpression(expression, message).matched;
}

} // namespace pdw
