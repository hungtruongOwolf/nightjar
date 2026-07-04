#pragma once

#include <string>
#include <vector>

#include "nightjar/facts.h"
#include "nightjar/vlm_worker.h"

namespace nightjar {

struct MtmdConfig {
    std::string model_path;    // SmolVLM-500M-Instruct GGUF (INT4)
    std::string mmproj_path;   // mmproj vision encoder GGUF
    bool encoder_use_gpu = true;  // KT1: encoder on Metal ~34x faster than CPU
    int n_threads = 0;            // 0 => llama default
    // Which subjects to ask about. One focused y/n question per subject — KT1
    // found the 500M model reliable one-at-a-time (8/9) but noisy when asked
    // about several subjects in a single prompt. Set this to the union of the
    // active rules' subjects so only what's needed is inferred.
    std::vector<Subject> subjects{Subject::Person, Subject::Vehicle, Subject::Animal,
                                  Subject::Package};
};

// Real Tier-2 worker backed by llama.cpp mtmd. Loads the model once (warm
// context), then per candidate runs one grammar-constrained inference that
// emits four y/n answers (person, vehicle, animal, package) and fills Facts
// with the encode / prefill / decode split (design doc §5.4). This is the only
// module that links llama.cpp; it is built behind the NIGHTJAR_VLM option so
// the engine core stays dependency-free and offline-buildable.
class MtmdVlmWorker : public IVlmWorker {
public:
    explicit MtmdVlmWorker(const MtmdConfig& config);
    ~MtmdVlmWorker() override;

    MtmdVlmWorker(const MtmdVlmWorker&) = delete;
    MtmdVlmWorker& operator=(const MtmdVlmWorker&) = delete;

    bool ok() const { return ok_; }              // false if the model failed to load
    const std::string& error() const { return error_; }

    Facts infer(const CandidateFrame& candidate) override;

private:
    struct Impl;
    Impl* impl_ = nullptr;
    bool ok_ = false;
    std::string error_;
    std::vector<Subject> subjects_;
};

}  // namespace nightjar
