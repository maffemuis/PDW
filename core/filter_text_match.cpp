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

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool alternative_matches(const TextAlternative& alt, const std::string& message_upper) {
    if (alt.terms.empty()) return false;

    if (alt.mode == TextMatchMode::Exact) {
        if (alt.terms.size() != 1) return false;
        return message_upper == alt.terms.front();
    }

    for (std::size_t i = 0; i < alt.terms.size(); ++i) {
        const std::string& term = alt.terms[i];
        if (term.empty()) return false;

        if (alt.mode == TextMatchMode::StartsWith && i == 0) {
            if (!starts_with(message_upper, term)) return false;
        } else if (message_upper.find(term) == std::string::npos) {
            return false;
        }
    }
    return true;
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
            alt.mode = exact_message ? TextMatchMode::Exact : TextMatchMode::Contains;

            if (!exact_message && alternative_text[0] == '^') {
                alt.mode = TextMatchMode::StartsWith;
                alternative_text = trim(alternative_text.substr(1));
            }

            std::string::size_type term_start = 0;
            while (term_start <= alternative_text.size()) {
                const std::string::size_type term_end = alternative_text.find('&', term_start);
                const std::string term = trim(alternative_text.substr(
                    term_start,
                    term_end == std::string::npos ? std::string::npos : term_end - term_start));
                if (!term.empty()) alt.terms.push_back(upper(term));
                if (term_end == std::string::npos) break;
                term_start = term_end + 1;
            }

            if (!alt.terms.empty()) parsed.alternatives.push_back(alt);
        }

        if (alt_end == std::string::npos) break;
        alt_start = alt_end + 1;
    }

    return parsed;
}

bool MatchTextFilterExpression(const TextFilterExpression& expression, const std::string& message) {
    const std::string message_upper = upper(message);
    for (std::size_t i = 0; i < expression.alternatives.size(); ++i) {
        if (alternative_matches(expression.alternatives[i], message_upper)) return true;
    }
    return false;
}

} // namespace pdw
