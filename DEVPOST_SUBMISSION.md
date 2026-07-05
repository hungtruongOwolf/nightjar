# Devpost description (paste into the submission text field)

Track: Track 3, Mobile AI. The repo URL and the video link go in their own fields on the form. The full setup and validation ladder live in the repo (README + JUDGES.md).

---

## Inspiration

The whole industry is scrambling for GPUs right now. They are **scarce, expensive, and rented by the hour** from a data center you do not control. But the most abundant AI hardware on Earth is not in a data center. **It is in a drawer.** Hundreds of billions of Arm chips have shipped, and a huge number of them are old phones that still have a good camera, a capable CPU, a battery and a radio, sitting switched off and heading for landfill.

I wanted to make a point with a product: **instead of chasing the scarce accelerator, squeeze the abundant hardware you already own.** A three-year-old phone is a supercomputer by the standards of a few years ago, and if you optimize for it properly it can run real vision AI on its own, with no cloud and no GPU. The second push was personal: **I come from high-frequency trading**, where the craft is doing the most work in the least time on commodity CPUs and never being caught out by the worst case, and I had not seen anyone build a consumer AI camera that way.

## What it does

Nightjar turns a spare phone into a security camera **you program in one plain-English sentence.** You type something like *"tell me if someone loiters near my car after 10pm."* It reads that sentence once, turns it into a precise rule, and then watches. When the thing you described happens, your phone alerts you in a couple of seconds with a photo and a short clip, and can push it to your everyday phone.

The important part is what it does *not* do: **it never sends your video anywhere.** All the understanding happens on the device. No account, no subscription, and it works in airplane mode. It also understands things a normal motion camera cannot, like the difference between someone walking past and someone who **lingers**, or between a **delivery** being dropped off and a **package being stolen**, because it reasons about what it sees *over time*, not just in one frame.

## How we built it

A vision-language model is powerful but expensive, so **Nightjar almost never runs it.** It uses two tiers, and this is where the trading background came in.

- **Tier 1** is a tiny hand-written **Arm NEON** motion filter on the CPU. It costs about **58 microseconds per frame** and throws away **roughly 88% of frames**. Each frame is like a market tick: mostly noise, rejected almost for free.
- **Tier 2** is the expensive model (**SmolVLM-500M, 4-bit, on the Arm CPU via llama.cpp and KleidiAI**). It only wakes for the small fraction of frames that pass. An alert is like an order: rare, and worth real work.

On top of that I applied the techniques a trading system uses to stay fast and honest: **a lock-free conflating queue** between the tiers (keep the newest frame, drop stale ones, so the slow model never stalls the fast path); **tail-latency discipline** (I tune the **p99 worst case**, not the average, because a guard that is occasionally slow misses the one moment that matters); **efficiency-vs-performance core scheduling** (cheap filter toward the E-cores, model toward the P-cores, an Arm specialty); and **an anti-coordinated-omission harness** that feeds frames on their real 30fps schedule so the latency numbers are honest under load.

Turning the sentence into a rule is its own trick: small models are unreliable at emitting a whole rule at once, so the model only answers small focused questions and deterministic code assembles the rule, with a confirmation screen as the safety net. **At run time there is no model in the decision path at all**, the model only reports simple per-frame facts and microsecond deterministic code turns them, over time, into the alert.

Everything is a **measured, reproducible number**: 88% of VLM compute gated, encode-once KV-cache reuse **586 to 229 ms (2.6x)**, a fast path that holds gate p99 flat under burst **(4 ms to 73 us, 54x)**, and a KleidiAI on/off benchmark on Linux aarch64. The whole thing is **one portable C++ engine on three Arm platforms** (macOS, Linux aarch64, iPhone A15) with a thin SwiftUI/AppKit shell. A judge can run `make demo` and watch the pipeline print its own latency report in five minutes, no phone and no model.

## Challenges we ran into

The two hardest problems were latency-and-reliability problems, which is exactly where the trading background helped. A small model is noisy and occasionally slow, so I could not trust it in the hot path or on a single frame; the conflating queue plus debounced, deterministic temporal reasoning fixed that. A small model also cannot reliably write a whole rule at once (I measured about **2 out of 8**), which forced the decompose-and-assemble compiler and the confirmation screen. And I kept every claim honest under load: the harness measures without coordinated omission, and the published numbers include the misses. Getting my iPhone to pair over a corporate network never worked, so I built the demo and the numbers on the Mac and on Linux aarch64 instead, where they are just as real.

## Accomplishments that we're proud of

- **A real vision-language model running on a phone-class Arm CPU**, with 88% of frames gated away, fast enough and cool enough to watch all day.
- **Every optimization is measured and reproducible in five minutes**, and I can even show the gate compiling to real Arm SIMD (`uabd.16b`, `uaddw.8h`), not autovectorized scalar.
- **One codebase, three Arm platforms.**
- **The perception-versus-reasoning split works**: it fires on loitering and package-left-behind, conditions a plain detector cannot express, with no AI in the decision path.
- And the point behind all of it: **it shows the CPU has far more headroom than the GPU-first narrative assumes, and it turns a dead phone into a private guard.** This is the *spreadsheet moment for computer vision*: programming a camera drops from "hire an ML team and rent a GPU" to a single sentence, on hardware you already own, private by design.

## What we learned

- **Small models classify reliably one fact at a time, but compose poorly.** Asking for a whole rule gave about 2/8; decomposing into focused questions gave 12/12. Design around what the model is actually good at.
- **High-frequency-trading discipline maps directly onto on-device AI.** Conflate stale inputs, optimize the tail not the average, and split work across efficiency and performance cores. The frame is the tick, the alert is the order.
- **KleidiAI's INT4 kernels favor prefill** (the image-token regime that dominates a VLM's latency) more than single-token decode, which is exactly the part that matters here.
- **The GPU-first narrative overlooks how much a well-optimized Arm CPU can do**, and honesty (measuring without coordinated omission, publishing the misses) is what makes the numbers defensible.

## What's next for Nightjar

- **Run it end to end on bare Arm silicon.** Port the engine onto a **Raspberry Pi 5**, a pure Arm board with *no Apple GPU at all*, fed by the open-source Linux camera stack (**libcamera / rpicam**). The engine is already portable C++ that passes its tests on Linux aarch64, so this turns a spare phone or a fifty-dollar board into a dedicated, always-on, CPU-only Arm guard, and it is the strongest possible proof of the thesis that **Arm hardware has far more to give than a GPU-first world assumes.**
- **A wider rule vocabulary and richer temporal conditions**, so more of what people can say in a sentence just works.
- **Package the engine and its measurement harness as a reusable edge-AI starting point**, so other developers can drop their own model behind the same NEON gate, conflating queue, and honest telemetry.
