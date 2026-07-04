#include "llama_text_model.h"

#include <vector>

#include "llama.h"

namespace nightjar {

struct LlamaTextModel::Impl {
    llama_model* model = nullptr;
    llama_context* lctx = nullptr;
    const llama_vocab* vocab = nullptr;
};

LlamaTextModel::LlamaTextModel(const std::string& model_path) {
    impl_ = new Impl();
    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 0;  // CPU — the Arm/KleidiAI path
    impl_->model = llama_model_load_from_file(model_path.c_str(), mparams);
    if (!impl_->model) {
        error_ = "failed to load model: " + model_path;
        return;
    }
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 2048;
    cparams.n_batch = 512;
    impl_->lctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->lctx) {
        error_ = "failed to create context";
        return;
    }
    impl_->vocab = llama_model_get_vocab(impl_->model);
    ok_ = true;
}

LlamaTextModel::~LlamaTextModel() {
    if (impl_) {
        if (impl_->lctx) llama_free(impl_->lctx);
        if (impl_->model) llama_model_free(impl_->model);
        delete impl_;
    }
    llama_backend_free();
}

std::string LlamaTextModel::generate(const std::string& prompt, const std::string& grammar,
                                     int max_tokens) {
    if (!ok_) return "";
    llama_memory_clear(llama_get_memory(impl_->lctx), true);

    // Tokenize the prompt.
    const int n_max = static_cast<int>(prompt.size()) + 16;
    std::vector<llama_token> tokens(n_max);
    const int n = llama_tokenize(impl_->vocab, prompt.c_str(), static_cast<int>(prompt.size()),
                                 tokens.data(), n_max, /*add_special=*/true, /*parse_special=*/true);
    if (n <= 0) return "";
    tokens.resize(n);

    llama_batch batch = llama_batch_get_one(tokens.data(), n);
    if (llama_decode(impl_->lctx, batch) != 0) return "";

    // Greedy + grammar-constrained decode.
    llama_sampler* chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (!grammar.empty()) {
        llama_sampler_chain_add(chain, llama_sampler_init_grammar(impl_->vocab, grammar.c_str(), "root"));
    }
    llama_sampler_chain_add(chain, llama_sampler_init_greedy());

    std::string out;
    for (int i = 0; i < max_tokens; ++i) {
        const llama_token tok = llama_sampler_sample(chain, impl_->lctx, -1);
        if (llama_vocab_is_eog(impl_->vocab, tok)) break;
        char buf[64];
        const int m = llama_token_to_piece(impl_->vocab, tok, buf, sizeof(buf), 0, false);
        if (m > 0) out.append(buf, m);
        llama_token one = tok;
        llama_batch b = llama_batch_get_one(&one, 1);
        if (llama_decode(impl_->lctx, b) != 0) break;
        if (out.find('}') != std::string::npos) break;  // rule JSON complete
    }
    llama_sampler_free(chain);
    return out;
}

}  // namespace nightjar
