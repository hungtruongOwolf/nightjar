// vlm_probe — run the real mtmd VLM worker on one PGM image and print Facts +
// the encode/prefill/decode split. Verifies on-device inference works before
// wiring it into the pipeline. Usage:
//   vlm_probe <model.gguf> <mmproj.gguf> <image.pgm>

#include <cstdio>

#include "mtmd_vlm_worker.h"
#include "nightjar/best_frame_selector.h"
#include "nightjar/pgm.h"

using namespace nightjar;

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <model.gguf> <mmproj.gguf> <image.pgm>\n", argv[0]);
        return 2;
    }
    auto img = read_pgm(argv[3]);
    if (!img) {
        std::fprintf(stderr, "cannot read PGM: %s\n", argv[3]);
        return 2;
    }

    MtmdConfig cfg;
    cfg.model_path = argv[1];
    cfg.mmproj_path = argv[2];
    MtmdVlmWorker worker(cfg);
    if (!worker.ok()) {
        std::fprintf(stderr, "worker init failed: %s\n", worker.error().c_str());
        return 1;
    }

    CandidateFrame c;
    c.image.size = img->width;  // probe expects a square PGM (already letterboxed)
    c.image.pixels = img->pixels;

    // Warm-up (first call pays graph/setup cost), then a measured call.
    worker.infer(c);
    Facts f = worker.infer(c);

    std::printf("raw='%s'  person=%d vehicle=%d animal=%d package=%d\n", f.raw_json.c_str(),
                f.person, f.vehicle, f.animal, f.package);
    std::printf("encode=%.0fms prefill=%.0fms decode=%.0fms total=%.0fms\n", f.encode_ms,
                f.prefill_ms, f.decode_ms, f.encode_ms + f.prefill_ms + f.decode_ms);
    return 0;
}
