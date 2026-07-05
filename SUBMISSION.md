# Nightjar, submission checklist

Arm Create: AI Optimization Challenge 2026, Track 3 (Mobile AI).

## What's done (in this repo)

- **Portable C++ engine** (`engine/`), ring buffer → NEON MotionGate →
  BestFrameSelector → ConflatingSlot → VLM → TemporalRuleEngine → AlertSink →
  Telemetry. Zero platform deps; **`make test` = 23/23**; **`make demo`** replays
  a clip end-to-end and prints `report.md`, no iPhone, no model needed (G5).
- **Real VLM**, SmolVLM-500M INT4 via llama.cpp `mtmd` (`engine/vlm/`), Metal
  encoder. Real inference verified (encode/prefill/decode split in telemetry).
- **Two thin shells over one engine** (`shells/ios/`, 0% logic in Swift):
  - **NightjarMac**, live guard on the **Mac webcam** (real SmolVLM Tier-2).
  - **Nightjar (iOS)**, device camera; Simulator falls back to a synthetic
    scene (labelled `SIM · scripted`, honest).
  - Full flow: English rule → on-device compile → confirm → draw zone → watch
    list → live guard → evidence review → performance monitor.
  - Optional real **phone push via ntfy** (set a topic on the rules screen).
- **Arm optimizations, measured** (`README.md` table): 88% VLM compute gated,
  NEON gate, KV-cache reuse (encode-once), fast-path decoupling, INT4 + inter-
  frame clip storage, **KleidiAI on/off** (`bench/kleidiai_results.md`).
- **Three Arm platforms, one source**: macOS (M2 Max), Linux aarch64
  (`Dockerfile.linux-arm64`), iPhone (Simulator-verified; A15 pending a cable).
- **Docs**: `README.md` (diagram-first), `JUDGES.md` (5-tier ladder + claim→
  reproduce map), `VIDEO_SCRIPT.md`, `recipes/model.md`, MIT `LICENSE`.

## Judges can reproduce in 5 minutes (no iPhone, no model)

```sh
make test    # 23/23 unit tests (NEON == scalar parity, temporal engine, codec)
make demo    # full pipeline on a synthetic clip -> alerts + report.md
```
Real inference + KleidiAI: see `JUDGES.md` Tier 2 and `bench/kleidiai_results.md`.

## Track, **Track 3: Mobile AI**

Per the Devpost **Track Details** page (the authoritative taxonomy): Track 1 =
Physical AI, Track 2 = Cloud AI, **Track 3 = Mobile AI**, "AI that runs locally
on Arm-powered client devices such as smartphones… camera intelligence… private,
offline-capable." That is exactly Nightjar → **submit Track 3**. Track 3 is a
**source-code submission** (which we have); its listed learning paths (KleidiAI /
on-device LLM) are **optional resources, not a required artifact**. (The
"Optimization/Migration/Scale+learning" wording in the generic Official Rules
section does not match the Track Details page; the Track Details page governs.)

## TODO before you submit (needs you)

- [ ] **Register**: click *Join Hackathon* on Devpost + create a free **Arm
      Developer Program** account (both are in the rules' "How to Enter").
- [ ] **Fill the Devpost submission form**, copy the write-up from
      `DEVPOST_SUBMISSION.md` (Project Overview / why-it-wins, Functionality /
      Output, Setup Instructions), select **Track 3, Mobile AI**, paste the repo URL.
- [ ] **Confirm the repo About shows the MIT license** (GitHub auto-detects the
      `LICENSE` file, check the right sidebar says "MIT License").
- [ ] **Record + upload the demo video** (≤3 min), follow `VIDEO_SCRIPT.md`,
      upload to **YouTube/Vimeo/Youku** (public/unlisted), paste the link in the
      form. Optional but strongly weighted for judges.
- [ ] *(optional, great money-shot for the video)* install the **ntfy** app,
      subscribe to a topic, enter it on the rules screen → alerts push live.
- [ ] *(optional, not required, but a nice bonus)* complete one Arm **KleidiAI**
      learning path and screenshot the completion/badge. Best fit, because it's
      exactly our stack: a **KleidiAI + llama.cpp** path (e.g. "Run an LLM chatbot
      with llama.cpp using KleidiAI on Arm servers", or the Track-3 "Measure LLM
      inference performance with KleidiAI" one). You already have the toolchain,
      so it's quick. Save the screenshot in `docs/arm-learning/`; then tell me and
      I'll cite it in the write-up.

Note: Track 3 (Mobile AI) itself requires only the repo + write-up + optional
video. The learning path above is purely a bonus, and the project already uses
KleidiAI for real (`bench/kleidiai_results.md`).

## Honest caveats (kept visible, per our own rules)

- iPhone **A15 on-device numbers** are pending a working data cable, labelled
  TBD in the README; the Mac + Linux numbers are real and reproducible.
- The "54→2 alerts/day" style figures are **illustrative** until the 24h live
  runs replace them; they're labelled as such.
- On a build without the model (iOS/Simulator), Tier-2 is the scripted stand-in
  (person/motion only), the app says so; the real SmolVLM runs on the Mac.
- Battery "time-to-empty" is the OS whole-machine estimate, not Nightjar's own
  draw (no app can isolate that without root), labelled accordingly.
