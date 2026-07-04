#pragma once

#include <string>

#include "nightjar/rule_compiler.h"  // TextInferFn

namespace nightjar {

// Text-only llama.cpp generation for the RuleCompiler. Loads a GGUF once and
// generates GBNF-constrained output. The design uses the local model text-only
// here (no image); if the 20-rule gate fails, the C6 escalation swaps in a
// stronger model at setup — this class doesn't care which GGUF it loads.
class LlamaTextModel {
public:
    explicit LlamaTextModel(const std::string& model_path);
    ~LlamaTextModel();

    LlamaTextModel(const LlamaTextModel&) = delete;
    LlamaTextModel& operator=(const LlamaTextModel&) = delete;

    bool ok() const { return ok_; }
    const std::string& error() const { return error_; }

    // Generate up to max_tokens under the given GBNF grammar (greedy).
    std::string generate(const std::string& prompt, const std::string& grammar, int max_tokens = 64);

    // Adapt to the RuleCompiler's injected-function seam.
    TextInferFn as_infer() {
        return [this](const std::string& prompt, const std::string& grammar) {
            return generate(prompt, grammar);
        };
    }

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool ok_ = false;
    std::string error_;
};

}  // namespace nightjar
