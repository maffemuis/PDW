#pragma once

#include <string>
#include <vector>

namespace pdw {

enum class TextMatchMode {
    Contains,
    StartsWith,
    Exact
};

struct TextAlternative {
    std::vector<std::string> terms; // '&' means all terms must match within this alternative
    TextMatchMode mode = TextMatchMode::Contains;
};

struct TextFilterExpression {
    std::vector<TextAlternative> alternatives; // ';' means OR
};

TextFilterExpression ParseTextFilterExpression(const std::string& expression, bool exact_message);
bool MatchTextFilterExpression(const TextFilterExpression& expression, const std::string& message);

} // namespace pdw
