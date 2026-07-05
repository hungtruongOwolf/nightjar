// vlm_ask, free-form question about a PGM image (relational-reasoning probe).
//   vlm_ask <model.gguf> <mmproj.gguf> <image.pgm> "<question>"

#include <cstdio>

#include "mtmd_vlm_worker.h"
#include "nightjar/best_frame_selector.h"
#include "nightjar/pgm.h"

using namespace nightjar;

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: %s <model.gguf> <mmproj.gguf> <image.pgm> \"<question>\"\n",
                     argv[0]);
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
        std::fprintf(stderr, "init failed: %s\n", worker.error().c_str());
        return 1;
    }
    CandidateFrame c;
    c.image.size = img->width;
    c.image.pixels = img->pixels;

    worker.ask(c, "Describe the image.", 8);  // warm-up
    const std::string answer = worker.ask(c, argv[4], 80);
    std::printf("Q: %s\nA: %s\n", argv[4], answer.c_str());
    return 0;
}
