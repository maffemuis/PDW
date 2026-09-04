#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace pdw {

enum class TextMatchMode {
    Contains,
    StartsWith,
    Exact,
    OrderedAnd
};

struct TextAlternative {
    std::vector<std::string> terms;
    TextMatchMode mode = TextMatchMode::Contains;
};

struct TextFilterExpression {
    std::vector<TextAlternative> alternatives; // ';' means OR
};

struct TextMatchSpan {
    std::size_t position = 0;
    std::size_t length = 0;
};

struct TextMatchResult {
    bool matched = false;
    std::vector<TextMatchSpan> spans;
};

TextFilterExpression ParseTextFilterExpression(const std::string& expression, bool exact_message);
TextMatchResult FindTextFilterExpression(const TextFilterExpression& expression, const std::string& message,
                                         std::size_t max_spans = 128);
bool MatchTextFilterExpression(const TextFilterExpression& expression, const std::string& message);

// Returns std::string::npos when every '^' is valid. In non-exact mode '^'
// may only be the first non-whitespace character of a ';' alternative.
// Exact-message mode keeps '^' literal, preserving legacy precedence.
std::size_t FindInvalidTextFilterCaret(const std::string& expression, bool exact_message);

} // namespace pdw
