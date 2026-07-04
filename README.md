# Sentry

**Turn the spare iPhone in your drawer into an AI guard that understands plain English — 100% on-device.**

Type a rule like *"notify me if a person enters the backyard after 10pm"*. The phone compiles it once into a deterministic trigger, then watches through a two-tier pipeline: a hand-written NEON motion gate (<0.5ms/frame) feeds an INT4 vision-language model only the ~1–5% of frames that matter. When the rule fires, your main phone buzzes within seconds — with a photo and a one-line explanation. No cloud, no account, no subscription. Pull the network cable and it keeps thinking.

Built for the **Arm Create: AI Optimization Challenge 2026 — Track 3 Mobile AI** ("camera intelligence" on Arm-powered phones). All inference runs locally on Arm64 via llama.cpp + KleidiAI (which qualifies under the track's "or similar runtimes").

> **Status: Week 1 of 6 — kill-tests in progress.** This README grows as measured numbers replace assumptions. Every number in this repo is tagged either `[VERIFIED: source]` or `[ASSUMPTION → KT#]` (KT = kill-test). Numbers without measurement are never promoted to claims.

## Planned sections (filled in as the project lands)

1. **Overview & why it should win** — the living-novelty axis: no shipped system lets a user state a natural-language condition that is compiled once, on a consumer device, into the *trigger* for camera alerts. Prior art & where Sentry differs (Frigate GenAI, HA LLM Vision, SenseCAP Watcher, smolvlm-realtime-webcam, SCOPE, and friends) — with proper credit.
2. **Architecture** — one portable C++ engine, two thin shells (iOS SwiftUI, macOS replay); two-tier gating; deterministic rule engine (no AI at match time).
3. **Arm optimization story** — hand-written NEON Tier 1 (scalar-twin tested), INT4 quantization recipe, KleidiAI on/off ablation, E/P-core QoS partitioning on big.LITTLE, thermal-aware duty cycling. Measured on iPhone 13 Pro Max (A15, Arm64: NEON + dotprod + i8mm) and Apple M2 Max; Linux-aarch64 replay build.
4. **Honest numbers** — per-stage p50/p99 (VLM split encode/prefill/decode), the six mobile constraints the track names: model size · memory use · responsiveness · battery awareness · offline use · time to first token.
5. **Setup: build / run / validate** — `make demo` replays a clip on any Mac in ≤5 minutes, no iPhone required. See `JUDGES.md` (coming) for the 5-tier validation ladder.
6. **When NOT to use Sentry** — not a life-safety system; no face recognition; degraded modes announce themselves honestly.
7. **Reusable artifacts** — portable engine, replay harness, labeled false-positive clip dataset, INT4 model recipe, rule-compiler prompt assets.

## License

MIT — see [LICENSE](LICENSE).
