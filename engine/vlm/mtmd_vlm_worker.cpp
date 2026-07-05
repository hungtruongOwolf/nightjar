#include "mtmd_vlm_worker.h"

#include <chrono>
#include <string>
#include <vector>

#include "llama.h"
#include "mtmd-helper.h"
#include "mtmd.h"

namespace nightjar {
namespace {

using steady = std::chrono::steady_clock;
double ms_since(steady::time_point t0) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(steady::now() - t0).count() / 1e6;
}

// Single y/n answer per subject (KT1: reliable one-at-a-time on the 500M model).
constexpr const char* kGrammar = "root ::= \"y\" | \"n\"\n";

const char* subject_word(Subject s) {
    switch (s) {
        case Subject::Person: return "person";
        case Subject::Vehicle: return "vehicle";
        case Subject::Animal: return "animal";
        case Subject::Package: return "package";
    }
    return "thing";
}

void set_subject(Facts& f, Subject s, bool present) {
    switch (s) {
        case Subject::Person: f.person = present; break;
        case Subject::Vehicle: f.vehicle = present; break;
        case Subject::Animal: f.animal = present; break;
        case Subject::Package: f.package = present; break;
    }
}

}  // namespace

struct MtmdVlmWorker::Impl {
    llama_model* model = nullptr;
    llama_context* lctx = nullptr;
    const llama_vocab* vocab = nullptr;
    mtmd_context* mctx = nullptr;
};

MtmdVlmWorker::MtmdVlmWorker(const MtmdConfig& config) : subjects_(config.subjects) {
    impl_ = new Impl();
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;  // LLM on CPU — the Arm/KleidiAI story
    impl_->model = llama_model_load_from_file(config.model_path.c_str(), mparams);
    if (!impl_->model) {
        error_ = "failed to load model: " + config.model_path;
        return;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 4096;
    cparams.n_batch = 2048;
    if (config.n_threads > 0) cparams.n_threads = config.n_threads;
    impl_->lctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->lctx) {
        error_ = "failed to create llama context";
        return;
    }
    impl_->vocab = llama_model_get_vocab(impl_->model);

    mtmd_context_params mp = mtmd_context_params_default();
    mp.use_gpu = config.encoder_use_gpu;
    mp.print_timings = false;
    impl_->mctx = mtmd_init_from_file(config.mmproj_path.c_str(), impl_->model, mp);
    if (!impl_->mctx) {
        error_ = "failed to load mmproj: " + config.mmproj_path;
        return;
    }
    ok_ = true;
}

MtmdVlmWorker::~MtmdVlmWorker() {
    if (impl_) {
        if (impl_->mctx) mtmd_free(impl_->mctx);
        if (impl_->lctx) llama_free(impl_->lctx);
        if (impl_->model) llama_model_free(impl_->model);
        delete impl_;
    }
    llama_backend_free();
}

Facts MtmdVlmWorker::infer(const CandidateFrame& candidate) {
    Facts facts;
    if (!ok_) return facts;

    // mtmd bitmaps are RGB; replicate the grayscale plane across 3 channels.
    const uint32_t w = static_cast<uint32_t>(candidate.image.size);
    const uint32_t h = w;
    std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; ++i) {
        const unsigned char v = candidate.image.pixels[i];
        rgb[i * 3 + 0] = v;
        rgb[i * 3 + 1] = v;
        rgb[i * 3 + 2] = v;
    }
    mtmd_bitmap* bitmap = mtmd_bitmap_init(w, h, rgb.data());
    if (!bitmap) return facts;

    const std::string marker = mtmd_default_marker();
    std::string raw;
    for (Subject subject : subjects_) {
        // Fresh context per question so answers don't contaminate each other;
        // the image is re-encoded (32ms on Metal) — encode-once-reuse across
        // subjects is a tracked optimization (Open Q1).
        llama_memory_clear(llama_get_memory(impl_->lctx), true);

        const std::string prompt = "<|im_start|>User: " + marker + "Is there a " +
                                   subject_word(subject) +
                                   " in this image? Answer y or n.<end_of_utterance>\nAssistant:";
        mtmd_input_chunks* chunks = mtmd_input_chunks_init();
        mtmd_input_text text{prompt.c_str(), /*add_special=*/true, /*parse_special=*/true};
        const mtmd_bitmap* bitmaps[1] = {bitmap};
        if (mtmd_tokenize(impl_->mctx, chunks, &text, bitmaps, 1) != 0) {
            mtmd_input_chunks_free(chunks);
            continue;
        }

        // Walk chunks: time the vision encode separately from prompt prefill.
        llama_pos n_past = 0;
        const size_t n_chunks = mtmd_input_chunks_size(chunks);
        for (size_t i = 0; i < n_chunks; ++i) {
            const mtmd_input_chunk* chunk = mtmd_input_chunks_get(chunks, i);
            const bool is_last = (i == n_chunks - 1);
            if (mtmd_input_chunk_get_type(chunk) == MTMD_INPUT_CHUNK_TYPE_IMAGE) {
                auto t = steady::now();
                if (mtmd_encode_chunk(impl_->mctx, chunk) != 0) break;
                float* embd = mtmd_get_output_embd(impl_->mctx);
                facts.encode_ms += static_cast<float>(ms_since(t));
                t = steady::now();
                llama_pos np = n_past;
                if (mtmd_helper_decode_image_chunk(impl_->mctx, impl_->lctx, chunk, embd, n_past, 0,
                                                   2048, &np, nullptr, nullptr) != 0)
                    break;
                n_past = np;
                facts.prefill_ms += static_cast<float>(ms_since(t));
            } else {
                auto t = steady::now();
                llama_pos np = n_past;
                if (mtmd_helper_eval_chunk_single(impl_->mctx, impl_->lctx, chunk, n_past, 0, 2048,
                                                  is_last, &np) != 0)
                    break;
                n_past = np;
                facts.prefill_ms += static_cast<float>(ms_since(t));
            }
        }

        // Grammar-constrained decode: exactly one y/n token. The chain owns and
        // frees both samplers.
        auto t_dec = steady::now();
        llama_sampler* chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(chain, llama_sampler_init_grammar(impl_->vocab, kGrammar, "root"));
        llama_sampler_chain_add(chain, llama_sampler_init_greedy());
        const llama_token tok = llama_sampler_sample(chain, impl_->lctx, -1);
        char buf[32];
        const int n = llama_token_to_piece(impl_->vocab, tok, buf, sizeof(buf), 0, false);
        char answer = '?';
        for (int k = 0; k < n; ++k)
            if (buf[k] == 'y' || buf[k] == 'n') answer = buf[k];
        facts.decode_ms += static_cast<float>(ms_since(t_dec));
        llama_sampler_free(chain);

        set_subject(facts, subject, answer == 'y');
        raw.push_back(answer);
        mtmd_input_chunks_free(chunks);
    }

    facts.raw_json = raw;
    mtmd_bitmap_free(bitmap);
    return facts;
}

std::string MtmdVlmWorker::ask(const CandidateFrame& candidate, const std::string& question,
                               int max_tokens) {
    if (!ok_) return "";
    const uint32_t w = static_cast<uint32_t>(candidate.image.size);
    std::vector<unsigned char> rgb(static_cast<size_t>(w) * w * 3);
    for (size_t i = 0; i < static_cast<size_t>(w) * w; ++i) {
        const unsigned char v = candidate.image.pixels[i];
        rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v;
    }
    mtmd_bitmap* bitmap = mtmd_bitmap_init(w, w, rgb.data());
    if (!bitmap) return "";

    llama_memory_clear(llama_get_memory(impl_->lctx), true);
    const std::string marker = mtmd_default_marker();
    const std::string prompt =
        "<|im_start|>User: " + marker + question + "<end_of_utterance>\nAssistant:";
    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    mtmd_input_text text{prompt.c_str(), true, true};
    const mtmd_bitmap* bitmaps[1] = {bitmap};
    if (mtmd_tokenize(impl_->mctx, chunks, &text, bitmaps, 1) != 0) {
        mtmd_input_chunks_free(chunks);
        mtmd_bitmap_free(bitmap);
        return "";
    }
    llama_pos n_past = 0;
    if (mtmd_helper_eval_chunks(impl_->mctx, impl_->lctx, chunks, n_past, 0, 2048,
                                /*logits_last=*/true, &n_past) != 0) {
        mtmd_input_chunks_free(chunks);
        mtmd_bitmap_free(bitmap);
        return "";
    }

    llama_sampler* chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(chain, llama_sampler_init_greedy());
    std::string out;
    for (int i = 0; i < max_tokens; ++i) {
        const llama_token tok = llama_sampler_sample(chain, impl_->lctx, -1);
        if (llama_vocab_is_eog(impl_->vocab, tok)) break;
        char buf[64];
        const int n = llama_token_to_piece(impl_->vocab, tok, buf, sizeof(buf), 0, false);
        if (n > 0) out.append(buf, n);
        llama_token one = tok;
        llama_batch b = llama_batch_get_one(&one, 1);
        if (llama_decode(impl_->lctx, b) != 0) break;
    }
    llama_sampler_free(chain);
    mtmd_input_chunks_free(chunks);
    mtmd_bitmap_free(bitmap);
    return out;
}

}  // namespace nightjar
