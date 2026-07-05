#pragma once

#include <optional>
#include <string>

#include "nightjar/rule_compiler.h"  // TextInferFn
#include "nightjar/temporal_rule.h"

namespace nightjar {

// Few-shot prompt + GBNF grammar for one focused sub-question.
struct CompilerAssets {
    std::string subject_prompt, subject_grammar;  // -> person/vehicle/animal/package
    std::string event_prompt, event_grammar;      // -> appears/stays/disappears
    std::string dwell_prompt, dwell_grammar;       // -> seconds (for "stays")
};

// Compiles English into a TemporalRule by DECOMPOSITION: instead of asking the
// small model to emit a whole nested rule at once (measured unreliable — see
// the negative-result experiment), it asks a few focused CLASSIFICATION
// questions the model does reliably (each few-shot + GBNF-constrained), and
// assembles the answers into the rule with deterministic code. The confirmation
// screen (design F1) remains the safety net for the rare miss.
class DecomposedRuleCompiler {
public:
    DecomposedRuleCompiler(CompilerAssets assets, TextInferFn infer);

    std::optional<TemporalRule> compile(const std::string& english, const std::string& id) const;

private:
    std::string ask(const std::string& prompt_tpl, const std::string& grammar,
                    const std::string& rule) const;

    CompilerAssets assets_;
    TextInferFn infer_;
};

}  // namespace nightjar
