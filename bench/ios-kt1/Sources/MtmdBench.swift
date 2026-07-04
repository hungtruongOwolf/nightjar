import Foundation
import llama

/// KT1 measurement engine: loads SmolVLM once (warm context), then runs
/// repeated single-image inferences with the per-stage timing split the
/// design doc mandates: encode_ms / prefill_ms / decode_ms.
///
/// Deliberately NOT the production VlmWorker — this exists to produce the
/// C1 go/no-go number on real hardware. Grammar is omitted (timing is what
/// KT1 measures; schema accuracy was validated on macOS in KT3).
final class MtmdBench {
    struct StageTiming {
        var encodeMs: Double = 0   // vision encoder forward pass
        var prefillMs: Double = 0  // image-embd decode + text prompt decode
        var decodeMs: Double = 0   // autoregressive token generation
        var output: String = ""
        var totalMs: Double { encodeMs + prefillMs + decodeMs }
    }

    enum BenchError: Error, CustomStringConvertible {
        case loadFailed(String)
        case inferFailed(String)
        var description: String {
            switch self {
            case .loadFailed(let s): return "load failed: \(s)"
            case .inferFailed(let s): return "infer failed: \(s)"
            }
        }
    }

    private let model: OpaquePointer
    private let lctx: OpaquePointer
    private let vocab: OpaquePointer
    private let mctx: OpaquePointer

    /// - Parameter encoderUseGPU: true = encoder on Metal (expected default
    ///   from the Mac KT1 result), false = encoder on CPU (ablation row).
    init(modelPath: String, mmprojPath: String, encoderUseGPU: Bool) throws {
        llama_backend_init()

        var mparams = llama_model_default_params()
        mparams.n_gpu_layers = 0 // LLM stays on CPU — the Arm/KleidiAI story
        guard let model = llama_model_load_from_file(modelPath, mparams) else {
            throw BenchError.loadFailed("model: \(modelPath)")
        }
        self.model = model

        var cparams = llama_context_default_params()
        cparams.n_ctx = 4096
        cparams.n_batch = 2048
        guard let lctx = llama_init_from_model(model, cparams) else {
            throw BenchError.loadFailed("llama context")
        }
        self.lctx = lctx
        self.vocab = llama_model_get_vocab(model)

        var mtmdParams = mtmd_context_params_default()
        mtmdParams.use_gpu = encoderUseGPU
        mtmdParams.print_timings = false
        mtmdParams.warmup = true
        guard let mctx = mtmd_init_from_file(mmprojPath, model, mtmdParams) else {
            throw BenchError.loadFailed("mtmd: \(mmprojPath)")
        }
        self.mctx = mctx
    }

    deinit {
        mtmd_free(mctx)
        llama_free(lctx)
        llama_model_free(model)
        llama_backend_free()
    }

    /// One full inference; KV cache is cleared first so every run measures
    /// the same cold-KV work the production pipeline would do per event.
    func infer(imagePath: String, question: String, maxTokens: Int32 = 8) throws -> StageTiming {
        var t = StageTiming()
        llama_memory_clear(llama_get_memory(lctx), true)

        let wrapper = mtmd_helper_bitmap_init_from_file(mctx, imagePath, false)
        guard let bitmap = wrapper.bitmap else {
            throw BenchError.inferFailed("bitmap: \(imagePath)")
        }
        defer { mtmd_bitmap_free(bitmap) }

        // SmolVLM chat format, media marker replaced by mtmd_tokenize.
        let marker = String(cString: mtmd_default_marker())
        let prompt = "<|im_start|>User: \(marker)\(question)<end_of_utterance>\nAssistant:"

        guard let chunks = mtmd_input_chunks_init() else {
            throw BenchError.inferFailed("chunks alloc")
        }
        defer { mtmd_input_chunks_free(chunks) }

        try prompt.withCString { cPrompt in
            var inputText = mtmd_input_text(text: cPrompt, add_special: true, parse_special: true)
            var bitmaps: [OpaquePointer?] = [bitmap]
            let rc = bitmaps.withUnsafeMutableBufferPointer { buf in
                mtmd_tokenize(mctx, chunks, &inputText, buf.baseAddress, 1)
            }
            if rc != 0 { throw BenchError.inferFailed("tokenize rc=\(rc)") }
        }

        // Walk chunks: encode timed separately from the two prefill flavours.
        var nPast: llama_pos = 0
        let nChunks = mtmd_input_chunks_size(chunks)
        for i in 0..<nChunks {
            guard let chunk = mtmd_input_chunks_get(chunks, i) else { continue }
            let isLast = (i == nChunks - 1)

            if mtmd_input_chunk_get_type(chunk) == MTMD_INPUT_CHUNK_TYPE_IMAGE {
                let t0 = now()
                if mtmd_encode_chunk(mctx, chunk) != 0 {
                    throw BenchError.inferFailed("encode_chunk")
                }
                guard let embd = mtmd_get_output_embd(mctx) else {
                    throw BenchError.inferFailed("output_embd")
                }
                t.encodeMs += now() - t0

                let t1 = now()
                var newPast: llama_pos = nPast
                if mtmd_helper_decode_image_chunk(mctx, lctx, chunk, embd,
                                                  nPast, 0, 2048, &newPast, nil, nil) != 0 {
                    throw BenchError.inferFailed("decode_image_chunk")
                }
                nPast = newPast
                t.prefillMs += now() - t1
            } else {
                let t1 = now()
                var newPast: llama_pos = nPast
                if mtmd_helper_eval_chunk_single(mctx, lctx, chunk,
                                                 nPast, 0, 2048, isLast, &newPast) != 0 {
                    throw BenchError.inferFailed("eval text chunk")
                }
                nPast = newPast
                t.prefillMs += now() - t1
            }
        }

        // Greedy decode, few tokens — the Facts answer is 1-8 tokens by design.
        let t2 = now()
        let sparams = llama_sampler_chain_default_params()
        guard let sampler = llama_sampler_chain_init(sparams) else {
            throw BenchError.inferFailed("sampler")
        }
        defer { llama_sampler_free(sampler) }
        llama_sampler_chain_add(sampler, llama_sampler_init_greedy())

        var out = ""
        for _ in 0..<maxTokens {
            let tok = llama_sampler_sample(sampler, lctx, -1)
            if llama_vocab_is_eog(vocab, tok) { break }
            out += pieceFor(token: tok)
            var tokCopy = tok
            let batch = llama_batch_get_one(&tokCopy, 1)
            if llama_decode(lctx, batch) != 0 {
                throw BenchError.inferFailed("decode step")
            }
        }
        t.decodeMs = now() - t2
        t.output = out.trimmingCharacters(in: .whitespacesAndNewlines)
        return t
    }

    private func pieceFor(token: llama_token) -> String {
        var buf = [CChar](repeating: 0, count: 64)
        let n = llama_token_to_piece(vocab, token, &buf, Int32(buf.count), 0, false)
        guard n > 0 else { return "" }
        return String(decoding: buf[0..<Int(n)].map { UInt8(bitPattern: $0) }, as: UTF8.self)
    }

    private func now() -> Double {
        Double(DispatchTime.now().uptimeNanoseconds) / 1_000_000.0
    }
}
