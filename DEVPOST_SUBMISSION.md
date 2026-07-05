# Devpost submission write-up (copy-paste into the form)

**Track:** Track 3, Mobile AI. On-device inference on an Arm client (a spare
iPhone or an Apple-Silicon Mac): camera intelligence, private, offline, low
latency.

**Repository:** https://github.com/hungtruongOwolf/nightjar (public, MIT license
shown in About).

## Project Overview

Nightjar turns a spare phone into a security camera you program in one
plain-English sentence, with every bit of inference on the Arm CPU.

Why it should win, in the terms the judging actually looks at:

- **It is a real Arm optimization, not a cloud wrapper.** The expensive model
  barely runs. A hand-written Arm NEON motion gate (58 us/frame, and I show it
  compiling to `uabd.16b` / `uaddw.8h`, not autovectorized scalar) discards about
  88% of frames, so the INT4 SmolVLM-500M (llama.cpp + KleidiAI, on the Arm CPU)
  only wakes on the 1-5% that matter. No NVIDIA, no CUDA, no cloud.
- **Every optimization is a measured, reproducible number, not a claim:** 88% of
  VLM compute gated away, encode-once KV-cache reuse (2.6x), a fast path that
  holds gate p99 flat under burst (54x), INT4 plus inter-frame clip storage, and
  a KleidiAI on/off benchmark on Linux aarch64. A judge reproduces the whole
  pipeline in five minutes with `make test` and `make demo`, no phone and no
  model required.
- **One portable C++ engine, three Arm platforms, one codebase:** macOS (M2 Max),
  Linux aarch64 (Docker), and iPhone A15.
- **The idea is the standout.** It splits perception from reasoning: the small
  model is only a per-frame fact sensor, and deterministic microsecond code
  integrates those facts over time. So it fires on conditions a plain detector
  cannot express (loitering, a package left behind versus taken), and there is
  never a model call in the hot decision path.
- **The impact is broad.** Programming a camera drops from "hire an ML team and
  rent a GPU" to one sentence; it puts idle Arm devices back to work instead of
  buying new silicon; and it is private by architecture, since pixels never leave
  the device. Security is only the first sentence. Elder-care, child-safety and
  accessibility are the same engine with a different sentence.

## Functionality / Output

You type a rule in English. On the device it is compiled once into a structured
rule (who, where, when, what to do), shown on a confirmation screen so a wrong
guess is caught before it arms. Then the guard runs live: the NEON gate watches
every frame, the VLM confirms simple per-frame facts on the few frames that pass,
and a temporal rule engine decides the alert (rising-edge "appears", sustained
"loitering", object "left behind"). Each alert keeps the photo or a short clip,
tagged by which rule fired, and can push to your phone over ntfy. The
reproducible output is a self-generated per-stage latency and counters report
(`report.md`) produced by `make demo`.

## Setup Instructions (build / run / validate on Arm64)

Prereqs: an Apple-Silicon Mac (Arm64) with Xcode command-line tools and CMake, or
any Arm64 Linux with Docker.

**1. Validate in five minutes, no iPhone and no model (any Arm64 Mac):**
```sh
git clone https://github.com/hungtruongOwolf/nightjar && cd nightjar
make test    # 23/23 unit tests: NEON==scalar parity, temporal engine, clip codec
make demo    # full pipeline on a synthetic clip, prints alerts and a report
```

**2. Third Arm platform (Linux aarch64, native on Apple Silicon):**
```sh
docker build --platform linux/arm64 -f Dockerfile.linux-arm64 -t nightjar-arm64 .
docker run --rm --platform linux/arm64 nightjar-arm64   # tests, gate bench, demo
```
KleidiAI on/off: `Dockerfile.kleidiai` and `bench/kleidiai_results.md`.

**3. Live camera demo (real SmolVLM on the Mac webcam):**
```sh
# download the model per recipes/model.md into ./models, then:
cd shells/ios && xcodegen && xcodebuild -scheme NightjarMac -configuration Release build
open ~/Library/Developer/Xcode/DerivedData/Nightjar-*/Build/Products/Release/NightjarMac.app
# allow the camera when prompted
```

The full validation ladder and where each claim is checked: JUDGES.md.
