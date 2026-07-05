#pragma once

#include <map>
#include <optional>
#include <string>

namespace nightjar {

// Parse a flat JSON object, {"key":"str", "key2":123, "key3":true}, into a
// key→raw-string map (string values unquoted, numbers/bools kept verbatim).
// Deliberately minimal: the RuleCompiler's LLM output is GBNF-constrained to a
// flat object, so nested values are neither produced nor needed. Returns
// nullopt on malformed input.
std::optional<std::map<std::string, std::string>> parse_flat_json_object(const std::string& text);

}  // namespace nightjar
