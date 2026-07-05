# NEON ISA proof — the Tier-1 gate really compiles to Arm SIMD

Nightjar's motion gate is hand-written Arm NEON (with a scalar twin + a parity
test that asserts `scalar == NEON` — see `engine/tests/test_gate_steps.cpp`).
This is the evidence that it emits the intended Arm SIMD instructions, not just
autovectorized scalar code — i.e. it genuinely leverages the Arm architecture.

## Intrinsic → emitted Arm instruction

Disassembly of the built object
(`engine/build/.../gate_steps.cpp.o`, `-O2 -arch arm64`):

| Gate step | Intrinsic (source) | Arm instruction emitted |
|---|---|---|
| Abs-diff `|frame − bg|` | `vabdq_u8` | `uabd.16b v0, v0, v4` (16 bytes/op) |
| Fixed-point EMA background (Q8.8) | shift/widen | `ushll.4s`, `ushll2.4s`, `ushl.4s`, `dup.4s`, `neg.4s` |
| Per-block foreground-pixel sum | widening accumulate | `uaddw.8h`, `uaddw2.8h` |

Raw excerpt:
```
0090  uabd.16b   v0, v0, v4     ; |frame - bg|, 16 px at a time
0094  uabd.16b   v1, v1, v5
...
01b0  ushll2.4s  v17, v16, #0x0 ; fixed-point EMA / threshold math, 4x int32
01c0  ushl.4s    v19, v7, v2
...
0874  uaddw2.8h  v0, v0, v16    ; widen-accumulate per-block pixel counts
0878  uaddw.8h   v1, v1, v16
```

**294 vector-lane NEON instructions** (`.16b/.8b/.8h/.4s/.2d`) in this one
object — the gate is doing real SIMD work per frame, which is why it costs
~0.2 ms/frame while touching every pixel.

## ISA on the measurement machine (M2 Max)

```
hw.optional.arm.FEAT_DotProd: 1
hw.optional.arm.FEAT_I8MM:    1
hw.optional.arm.FEAT_FP16:    1
hw.optional.arm.FEAT_BF16:    1
hw.optional.arm.FEAT_SME:     0
```
DotProd + I8MM are exactly what the Tier-2 INT4 matmul kernels (KleidiAI) use;
the app also reads these flags at runtime and shows them.

## Reproduce

```sh
make build
otool -tvV engine/build/CMakeFiles/nightjar.dir/src/gate_steps.cpp.o | grep -E '\.(16b|8h|4s)'
# on Linux aarch64: objdump -d ... | grep -E 'uabd|uaddw|ushll'
sysctl -a | grep 'hw.optional.arm.FEAT'
```

## Arm tools & products this project uses (honestly)

- **KleidiAI** — Arm's INT4 matmul micro-kernels (dotprod/i8mm) for Tier-2, via
  llama.cpp; measured on/off in `bench/kleidiai_results.md`.
- **Arm NEON** — the hand-written Tier-1 gate above; intrinsics referenced
  against Arm's **SIMD.info** and verified with **Compiler Explorer** / `otool`.
- **Arm ISA feature detection** (`FEAT_DotProd/I8MM/...`) at build + runtime.
- Not used (would be dishonest for an on-device project): the cloud/server tools
  in the Arm Developer Program (Graviton, Kubernetes, MigrateEase, etc.).
