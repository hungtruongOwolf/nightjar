# Devpost submission write-up (copy-paste into the form)

**Track:** **Track 3 — Mobile AI.** Nightjar runs its inference fully on-device
on an Arm-powered client (a spare iPhone / Apple-Silicon Mac): **camera
intelligence**, offline-capable, private, low-latency — the track's definition
verbatim. It's optimized for the exact mobile constraints the track names —
**model size** (244 MB Q4_0 + 190 MB mmproj), **memory** (≤1.4 GB budget),
**responsiveness** (event→alert well under the target), **battery awareness**
(88% of VLM compute gated), **offline** (airplane-mode), **TTFT** (encode ~32 ms)
— using llama.cpp + **KleidiAI** ("or similar runtimes", per the track).

**Repository:** https://github.com/hungtruongOwolf/nightjar (public, MIT license
shown in About).

---

## Project Overview

**Nightjar turns a spare phone into an AI guard you program in one plain-English
sentence — 100% on-device on Arm.** Type *"tell me if someone loiters in the
backyard after 10 pm"*; the phone compiles that once into a deterministic rule,
then watches through a two-tier pipeline: a hand-written **NEON motion gate**
(~0.2 ms/frame) discards ~88% of frames and wakes an **INT4 vision-language
model (SmolVLM-500M via llama.cpp + KleidiAI)** only on the frames that matter.
Nothing leaves the device except the one alert crop you opt into.

**Why it should win.** It's an optimization story end-to-end, and every number
is measured, not claimed: 88% of VLM compute gated away, a NEON gate with a
scalar-twin parity test, encode-once/ask-many KV-cache reuse, a fast-path that
keeps p99 flat under burst, INT4 + inter-frame differential clip storage, and a
**KleidiAI on/off benchmark** on Linux aarch64. The same **portable C++ engine**
runs on three Arm platforms from one source (macOS M2 Max, Linux aarch64 in
Docker, iPhone A15), with a thin SwiftUI/AppKit shell (0% business logic in
Swift). The "WOW": it decouples **perception from reasoning** — the VLM is a
per-frame fact sensor, and deterministic µs-level code integrates those facts
over *time*, so it fires on conditions a closed-vocabulary detector can't express
(**appears / loiters / package-left-behind**), all within a doorbell's power
budget. And it's honest: published numbers include the misses; degraded modes
say so.

**The bigger bet.** Arm has shipped ~300 billion chips; the smartest are idle in
drawers. Nightjar is the *spreadsheet moment for computer vision* — VisiCalc let
anyone express logic without being a programmer; Nightjar lets anyone program a
camera without being an ML engineer, on a phone they already own, with pixels
that never leave the device. Security is just the first sentence: *"tell me if
grandma hasn't moved in 2 hours"*, *"if the baby climbs out of the crib"* — same
engine, new sentence. The pattern underneath — a small model as a per-frame
**sensor**, deterministic µs code as the **brain** — is a reusable blueprint for
how on-device AI should be built on Arm.

## Functionality / Output

- **Program in English → compiled rule.** On-device, an English sentence becomes
  a structured rule (WHO / WHERE / ZONE / WHEN / action); a confirmation screen
  is the safety net. Runtime matching is plain deterministic code — no AI in the
  hot path.
- **Two-tier perception.** NEON gate (Tier 1, every frame) → best-frame select →
  SmolVLM-500M INT4 (Tier 2, ~1–5% of frames) answering focused y/n predicates →
  a temporal rule engine that fires on rising-edge / sustained-dwell /
  left-behind conditions.
- **Live guard app** (macOS webcam / iOS camera): live motion box, per-rule
  evidence (photo or short clip, tagged by which rule fired), a performance
  monitor (windowed frames/skip%/VLM-checks, warm p50/p99 latency, thermal), and
  an optional real push to your phone via ntfy — the only thing that leaves the
  device.
- **Reproducible output:** `make demo` replays a synthetic clip through the full
  engine and self-generates a per-stage latency + counters report (see
  `report.md`). Measured: gate ~0.2 ms/frame; real SmolVLM encode ~134 / prefill
  ~447 / decode ~12 ms (Metal encoder); event→alert well under the 3 s target.

## Setup Instructions (build / run / validate on Arm64)

Prereqs: an Apple-Silicon Mac (Arm64) with Xcode command-line tools + CMake, or
any Arm64 Linux with Docker.

**1. Validate in 5 minutes — no iPhone, no model (any Arm64 Mac):**
```sh
git clone https://github.com/hungtruongOwolf/nightjar && cd nightjar
make test    # 23/23 unit tests: NEON==scalar parity, temporal engine, clip codec
make demo    # full pipeline on a synthetic clip → alerts + a self-generated report
```

**2. Third Arm platform (Linux aarch64, native on Apple Silicon):**
```sh
docker build --platform linux/arm64 -f Dockerfile.linux-arm64 -t nightjar-arm64 .
docker run --rm --platform linux/arm64 nightjar-arm64   # tests + gate bench + demo
```
KleidiAI on/off number: `Dockerfile.kleidiai` + `bench/kleidiai_results.md`.

**3. Live camera demo (real SmolVLM on the Mac webcam):**
```sh
# download the model per recipes/model.md into ./models, then:
cd shells/ios && xcodegen && xcodebuild -scheme NightjarMac -configuration Release build
open ~/Library/Developer/Xcode/DerivedData/Nightjar-*/Build/Products/Release/NightjarMac.app
# allow the camera when prompted
```

Full validation ladder + where each claim is checked: **JUDGES.md**.
