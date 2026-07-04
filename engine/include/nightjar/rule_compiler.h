#pragma once

#include <functional>
#include <optional>
#include <string>

#include "nightjar/rule.h"

namespace nightjar {

// The LLM text step, injected so the compiler's prompt-building and JSON→Rule
// parsing are testable without a model. Given the built prompt and the GBNF
// grammar, returns the model's (constrained) JSON string.
using TextInferFn = std::function<std::string(const std::string& prompt, const std::string& grammar)>;

// Compiles one English rule into a Rule — ONCE, at rule creation (design doc
// §5.5). Output is GBNF-constrained flat JSON (subject/zone/start/end/
// cooldown_s), parsed and validated into a Rule. The confirmation screen is
// the first-class safety net: a small model may be wrong, so compile() failing
// or producing an odd result is surfaced to the user, never silently trusted.
// Actions (ntfy topic etc.) are attached separately by app config — the English
// describes what to watch, not the delivery channel.
class RuleCompiler {
public:
    RuleCompiler(std::string prompt_template, std::string grammar, TextInferFn infer);

    // Returns nullopt if the model output can't be parsed/validated into a Rule.
    std::optional<Rule> compile(const std::string& english, const std::string& id) const;

    // Parse a compiler-JSON string into a Rule (exposed for testing / offline use).
    static std::optional<Rule> parse(const std::string& json, const std::string& id,
                                     const std::string& raw_text);

private:
    std::string prompt_template_;  // contains the {RULE_TEXT} placeholder
    std::string grammar_;
    TextInferFn infer_;
};

}  // namespace nightjar
