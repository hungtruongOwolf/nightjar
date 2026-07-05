# Model recipe — reproducible GGUF setup for Nightjar

Two models, two jobs. Both run on-device via llama.cpp; the compiler model is
loaded **only at rule-setup time**, then freed, so it never touches the runtime
RAM budget.

| Role | Model | Quant | Size | When |
|---|---|---|---|---|
| Runtime VLM (fact sensor) | SmolVLM-500M-Instruct | Q4_0 | 244 MB + 190 MB mmproj | always-on |
| Rule compiler (English → rule) | Qwen2.5-1.5B-Instruct | Q4_K_M | ~1.0 GB | setup only, then freed |

## 1. Tooling

```bash
brew install llama.cpp          # provides llama-mtmd-cli, llama-quantize, libmtmd, libllama
brew install ggml               # ggml headers/libs (llama.cpp depends on them)
```

## 2. SmolVLM-500M (the vision fact-sensor)

The `ggml-org/SmolVLM-500M-Instruct-GGUF` repo ships only `f16` / `Q8_0`, so we
download `f16` + the vision projector (`mmproj`) and quantize to INT4 ourselves:

```bash
HF=https://huggingface.co/ggml-org/SmolVLM-500M-Instruct-GGUF/resolve/main
curl -fL -o SmolVLM-500M-Instruct-f16.gguf        $HF/SmolVLM-500M-Instruct-f16.gguf
curl -fL -o mmproj-SmolVLM-500M-Instruct-f16.gguf $HF/mmproj-SmolVLM-500M-Instruct-f16.gguf

llama-quantize SmolVLM-500M-Instruct-f16.gguf SmolVLM-500M-Instruct-Q4_0.gguf   Q4_0
llama-quantize SmolVLM-500M-Instruct-f16.gguf SmolVLM-500M-Instruct-Q4_K_M.gguf Q4_K_M
```

Measured sizes: `f16` 782 MB → **Q4_0 244 MB (3.2×)** / Q4_K_M 289 MB. Q4_0 is
the default: measurably faster than Q4_K_M on this workload, same answers.
Keep the `mmproj` at f16 (the vision encoder runs on Metal on-device).

## 3. Qwen2.5-1.5B-Instruct (the rule compiler)

```bash
curl -fL -o qwen2.5-1.5b-instruct-q4_k_m.gguf \
  https://huggingface.co/Qwen/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/qwen2.5-1.5b-instruct-q4_k_m.gguf
```

Why 1.5B and not the VLM's own text backbone: the SmolVLM backbone scored
**12/20** on the rule-compile gate (person-biased); Qwen2.5-1.5B scores
**12/12** on the decomposed compiler eval. It only loads during setup.

## 4. Verify the Arm ISA (goes into every report)

```bash
sysctl hw.optional.arm.FEAT_DotProd hw.optional.arm.FEAT_I8MM hw.optional.arm.FEAT_SME
```

Measured on the M2 Max lab machine: `FEAT_DotProd=1 · FEAT_I8MM=1 · FEAT_SME=0`.
On iPhone the app prints the same three flags via `sysctlbyname` (KT6).

## 5. Where they go

Drop the GGUFs in `models/` (git-ignored). Then:

```bash
cmake -S engine -B engine/build -DNIGHTJAR_VLM=ON && cmake --build engine/build
engine/build/vlm_probe   models/SmolVLM-500M-Instruct-Q4_0.gguf models/mmproj-...-f16.gguf img.pgm
engine/build/compile_eval models/qwen2.5-1.5b-instruct-q4_k_m.gguf tests/compile_eval.txt
```
