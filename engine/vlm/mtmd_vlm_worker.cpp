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

// Single y/n answer per question (KT1: reliable one-at-a-time on the 500M model).
constexpr const char* kGrammar = "root ::= \"y\" | \"n\"\n";

}  // namespace

struct MtmdVlmWorker::Impl {
    llama_model* model = nullptr;
    llama_context* lctx = nullptr;
    const llama_vocab* vocab = nullptr;
    mtmd_context* mctx = nullptr;
};

MtmdVlmWorker::MtmdVlmWorker(const MtmdConfig& config) : subjects_(config.subjects) {
    (void)subjects_;  // retained for config compatibility; evaluate() drives predicates
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

// Ask one y/n question about a pre-built RGB bitmap; returns the boolean and
// accumulates the encode/prefill/decode split into `out`.
bool MtmdVlmWorker::answer_one(void* bitmap_ptr, const std::string& question, PredicateResult& out) {
    mtmd_bitmap* bitmap = static_cast<mtmd_bitmap*>(bitmap_ptr);
    // Fresh context per question so answers don't contaminate each other; the
    // image is re-encoded (encode-once-reuse across questions = Open Q1).
    llama_memory_clear(llama_get_memory(impl_->lctx), true);

    const std::string marker = mtmd_default_marker();
    const std::string prompt =
        "<|im_start|>User: " + marker + question + "<end_of_utterance>\nAssistant:";
    mtmd_input_chunks* chunks = mtmd_input_chunks_init();
    mtmd_input_text text{prompt.c_str(), /*add_special=*/true, /*parse_special=*/true};
    const mtmd_bitmap* bitmaps[1] = {bitmap};
    if (mtmd_tokenize(impl_->mctx, chunks, &text, bitmaps, 1) != 0) {
        mtmd_input_chunks_free(chunks);
        return false;
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
            out.encode_ms += static_cast<float>(ms_since(t));
            t = steady::now();
            llama_pos np = n_past;
            if (mtmd_helper_decode_image_chunk(impl_->mctx, impl_->lctx, chunk, embd, n_past, 0,
                                               2048, &np, nullptr, nullptr) != 0)
                break;
            n_past = np;
            out.prefill_ms += static_cast<float>(ms_since(t));
        } else {
            auto t = steady::now();
            llama_pos np = n_past;
            if (mtmd_helper_eval_chunk_single(impl_->mctx, impl_->lctx, chunk, n_past, 0, 2048,
                                              is_last, &np) != 0)
                break;
            n_past = np;
            out.prefill_ms += static_cast<float>(ms_since(t));
        }
    }

    // Grammar-constrained decode: exactly one y/n token. The chain owns/frees both.
    auto t_dec = steady::now();
    llama_sampler* chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(chain, llama_sampler_init_grammar(impl_->vocab, kGrammar, "root"));
    llama_sampler_chain_add(chain, llama_sampler_init_greedy());
    const llama_token tok = llama_sampler_sample(chain, impl_->lctx, -1);
    char buf[32];
    const int n = llama_token_to_piece(impl_->vocab, tok, buf, sizeof(buf), 0, false);
    char answer = 'n';
    for (int k = 0; k < n; ++k)
        if (buf[k] == 'y' || buf[k] == 'n') answer = buf[k];
    out.decode_ms += static_cast<float>(ms_since(t_dec));
    llama_sampler_free(chain);
    mtmd_input_chunks_free(chunks);
    return answer == 'y';
}

PredicateResult MtmdVlmWorker::evaluate(const CandidateFrame& candidate,
                                        const std::vector<Predicate>& predicates) {
    PredicateResult result;
    if (!ok_) return result;

    // mtmd bitmaps are RGB; replicate the grayscale plane across 3 channels once,
    // then reuse for every predicate question.
    const uint32_t w = static_cast<uint32_t>(candidate.image.size);
    std::vector<unsigned char> rgb(static_cast<size_t>(w) * w * 3);
    for (size_t i = 0; i < static_cast<size_t>(w) * w; ++i) {
        const unsigned char v = candidate.image.pixels[i];
        rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v;
    }
    mtmd_bitmap* bitmap = mtmd_bitmap_init(w, w, rgb.data());
    if (!bitmap) return result;

    for (const Predicate& p : predicates) {
        result.answers[p.id] = answer_one(bitmap, p.question, result);
    }
    mtmd_bitmap_free(bitmap);
    return result;
}

Facts MtmdVlmWorker::infer(const CandidateFrame& candidate) {
    const PredicateResult r = evaluate(candidate, core_predicates());
    Facts facts;
    facts.person = r.get("person");
    facts.vehicle = r.get("vehicle");
    facts.animal = r.get("animal");
    facts.package = r.get("package");
    facts.encode_ms = r.encode_ms;
    facts.prefill_ms = r.prefill_ms;
    facts.decode_ms = r.decode_ms;
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
