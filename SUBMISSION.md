# Nightjar — submission checklist

Arm Create: AI Optimization Challenge 2026 — Track 3 (Mobile AI).

## What's done (in this repo)

- **Portable C++ engine** (`engine/`) — ring buffer → NEON MotionGate →
  BestFrameSelector → ConflatingSlot → VLM → TemporalRuleEngine → AlertSink →
  Telemetry. Zero platform deps; **`make test` = 23/23**; **`make demo`** replays
  a clip end-to-end and prints `report.md` — no iPhone, no model needed (G5).
- **Real VLM** — SmolVLM-500M INT4 via llama.cpp `mtmd` (`engine/vlm/`), Metal
  encoder. Real inference verified (encode/prefill/decode split in telemetry).
- **Two thin shells over one engine** (`shells/ios/`, 0% logic in Swift):
  - **NightjarMac** — live guard on the **Mac webcam** (real SmolVLM Tier-2).
  - **Nightjar (iOS)** — device camera; Simulator falls back to a synthetic
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

## TODO before you submit (needs you)

- [ ] **Record the demo video** (≤3 min) on the Mac — follow `VIDEO_SCRIPT.md`.
- [ ] **Complete ≥1 Arm learning path** and screenshot it — the challenge's
      required proof artifact. Add the screenshot to the repo / submission.
- [ ] *(optional, great for the video)* install the **ntfy** app on your phone,
      subscribe to a topic, enter it on the rules screen → alerts push live.
- [ ] **Submit**: public repo link (github.com/hungtruongOwolf/nightjar) + the
      video, on the challenge portal.

## Honest caveats (kept visible, per our own rules)

- iPhone **A15 on-device numbers** are pending a working data cable — labelled
  TBD in the README; the Mac + Linux numbers are real and reproducible.
- The "54→2 alerts/day" style figures are **illustrative** until the 24h live
  runs replace them; they're labelled as such.
- On a build without the model (iOS/Simulator), Tier-2 is the scripted stand-in
  (person/motion only) — the app says so; the real SmolVLM runs on the Mac.
- Battery "time-to-empty" is the OS whole-machine estimate, not Nightjar's own
  draw (no app can isolate that without root) — labelled accordingly.
