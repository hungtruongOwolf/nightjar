# KleidiAI on/off — measured

The Arm challenge rewards Arm-specific optimization. llama.cpp's CPU backend can
use [KleidiAI](https://gitlab.arm.com/kleidi/kleidiai) INT4 matmul micro-kernels
(dotprod / i8mm). This measures the delta cleanly on **Linux aarch64** — no Apple
Accelerate in the mix to confound the result (as there would be on macOS).

## How to reproduce

```sh
docker build --platform linux/arm64 -f Dockerfile.kleidiai -t nightjar-kleidiai .
docker run --rm --platform linux/arm64 -v "$PWD/models:/models" nightjar-kleidiai
```

It builds `llama.cpp` twice — `-DGGML_CPU_KLEIDIAI=ON` vs `OFF` — and runs
`llama-bench` on the SmolVLM-500M language model (Q4_0), so the only variable is
the kernel.

## Result

`[VERIFIED: Docker linux/arm64 on M2 Max, debian:bookworm, clang-14, SmolVLM-500M
language model Q4_0 (242 MiB / 409M params)]`

| batch | metric | KleidiAI OFF | KleidiAI ON | Δ |
|------:|--------|-------------:|------------:|:--|
| pp128, 4t | prefill tok/s | 1082 ± 30 | 1112 ± 11 | **+2.7%** |
| pp512, 8t | prefill tok/s | 1468 ± 101 | 1575 ± 38 | **+7.2%** |
| tg32, 4t  | decode tok/s  | 280 ± 12 | 250 ± 14 | −11% |
| tg64, 8t  | decode tok/s  | 288 ± 13 | 199 ± 10 | −31% |

## Reading it honestly

KleidiAI's repacked-weight kernels optimize **batched GEMM (prefill)**, and the
gain grows with batch size (+2.7% → +7.2% as prefill goes 128 → 512). They
**regress single-row GEMV (token decode)** in this configuration, and the
regression is amplified by the virtualized CPU.

For **this** workload that tradeoff is favourable: a VLM's cost is dominated by
**image-token prefill** (encode + prefill is the bulk of the ~2.5 s Tier-2 budget;
the GBNF-constrained JSON output is only a few dozen decode tokens). Prefill is
exactly what KleidiAI speeds up.

The definitive per-stage number belongs on **real Arm silicon (iPhone A15)**,
where KT1 measures encode/prefill/decode separately on device — a virtualized
`x`-on-Apple VM is a directional signal, not the headline. Tagged accordingly.
