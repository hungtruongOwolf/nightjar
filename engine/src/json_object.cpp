#include "nightjar/json_object.h"

#include <cctype>

namespace nightjar {
namespace {

void skip_ws(const std::string& s, size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

// Read a JSON string literal starting at s[i]=='"'. Advances i past the close
// quote. Handles \" and \\ escapes. Returns false on unterminated string.
bool read_string(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        const char c = s[i++];
        if (c == '\\' && i < s.size()) {
            const char e = s[i++];
            out.push_back(e == 'n' ? '\n' : e);  // minimal escape handling
        } else if (c == '"') {
            return true;
        } else {
            out.push_back(c);
        }
    }
    return false;  // unterminated
}

// Read a bare value (number / true / false / null): a run of non-delimiter,
// non-space characters. Stopping at whitespace lets the caller detect a missing
// comma (a following token that isn't ',' or '}').
std::string read_bare(const std::string& s, size_t& i) {
    const size_t start = i;
    while (i < s.size() && s[i] != ',' && s[i] != '}' &&
           !std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return s.substr(start, i - start);
}

}  // namespace

std::optional<std::map<std::string, std::string>> parse_flat_json_object(const std::string& text) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    skip_ws(text, i);
    if (i >= text.size() || text[i] != '{') return std::nullopt;
    ++i;
    skip_ws(text, i);
    if (i < text.size() && text[i] == '}') return out;  // empty object

    for (;;) {
        skip_ws(text, i);
        std::string key;
        if (!read_string(text, i, key)) return std::nullopt;
        skip_ws(text, i);
        if (i >= text.size() || text[i] != ':') return std::nullopt;
        ++i;
        skip_ws(text, i);

        std::string value;
        if (i < text.size() && text[i] == '"') {
            if (!read_string(text, i, value)) return std::nullopt;
        } else {
            value = read_bare(text, i);
            if (value.empty()) return std::nullopt;
        }
        out[key] = value;

        skip_ws(text, i);
        if (i >= text.size()) return std::nullopt;
        if (text[i] == ',') {
            ++i;
            continue;
        }
        if (text[i] == '}') return out;
        return std::nullopt;  // unexpected char
    }
}

}  // namespace nightjar
