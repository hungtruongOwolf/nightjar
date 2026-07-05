// kv_reuse_bench — verify + measure the "encode-once, ask-many" optimization.
// Runs the same N predicates on one image via the re-encode path and the
// KV-reuse path, and prints per-mode encode/prefill/decode/total and the
// answers. Correctness check: the answers must match; speed check: reuse should
// be far faster for N>1 (image encode + prefill paid once).
//   kv_reuse_bench <model.gguf> <mmproj.gguf> <image.pgm>

#include <cstdio>
#include <string>
#include <vector>

#include "mtmd_vlm_worker.h"
#include "nightjar/best_frame_selector.h"
#include "nightjar/pgm.h"

using namespace nightjar;

namespace {
std::vector<Predicate> predicates() {
    return {{"person", "Is there a person in this image? Answer y or n."},
            {"vehicle", "Is there a vehicle in this image? Answer y or n."},
            {"animal", "Is there an animal in this image? Answer y or n."},
            {"package", "Is there a package or box in this image? Answer y or n."}};
}
void run(const char* label, MtmdConfig cfg, const CandidateFrame& c) {
    MtmdVlmWorker w(cfg);
    if (!w.ok()) {
        std::fprintf(stderr, "init: %s\n", w.error().c_str());
        return;
    }
    w.evaluate(c, predicates());  // warm-up
    PredicateResult r = w.evaluate(c, predicates());
    std::string ans;
    for (const auto& kv : r.answers) ans += kv.first + "=" + (kv.second ? "y " : "n ");
    std::printf("%-12s enc=%.0f pre=%.0f dec=%.0f total=%.0fms | %s\n", label, r.encode_ms,
                r.prefill_ms, r.decode_ms, r.encode_ms + r.prefill_ms + r.decode_ms, ans.c_str());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <model.gguf> <mmproj.gguf> <image.pgm>\n", argv[0]);
        return 2;
    }
    auto img = read_pgm(argv[3]);
    if (!img) {
        std::fprintf(stderr, "bad pgm\n");
        return 2;
    }
    CandidateFrame c;
    c.image.size = img->width;
    c.image.pixels = img->pixels;

    MtmdConfig base;
    base.model_path = argv[1];
    base.mmproj_path = argv[2];

    std::printf("4 predicates on one image:\n");
    MtmdConfig reencode = base;
    reencode.reuse_image_kv = false;
    run("re-encode", reencode, c);

    MtmdConfig reuse = base;
    reuse.reuse_image_kv = true;
    run("kv-reuse", reuse, c);
    return 0;
}
