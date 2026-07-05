# Devpost description (paste into the submission text field)

Track: Track 3, Mobile AI. The repo URL and the video link go in their own fields on the form. Setup and the full validation ladder live in the repo (README + JUDGES.md), so this write-up is the story, not a manual.

---

## Inspiration

The whole industry is scrambling for GPUs right now. They are **scarce, expensive, and rented by the hour** from a data center you do not control. But the most abundant AI hardware on Earth is not in a data center. **It is in a drawer.** Hundreds of billions of Arm chips have shipped, and a huge number of them are old phones that still have a good camera, a capable CPU, a battery, and a radio, sitting switched off and heading for landfill.

I wanted to make a point with a product: **instead of chasing the scarce, expensive accelerator, squeeze the abundant hardware you already own.** A three-year-old phone is a supercomputer by the standards of a few years ago. Optimize for it properly and it can run real vision AI on its own, *with no cloud and no GPU*.

The second push was personal. **I come from high-frequency trading**, where the entire craft is doing the most work in the least time on commodity CPUs, and never being caught out by the worst case. That mindset maps almost perfectly onto on-device AI, and I had not seen anyone build a consumer AI camera that way. So I did.

## What it does

Nightjar turns a spare phone into a security camera **you program in one plain-English sentence**. You type something like *"tell me if someone loiters near my car after 10pm."* The phone reads that sentence once, turns it into a precise rule, and then watches. When the thing you described actually happens, your phone alerts you in **a couple of seconds** with a photo and a short clip, and can push it to your everyday phone.

The important part is what it does *not* do. **It never sends your video anywhere.** All of the understanding happens on the device itself. No cloud account, no subscription, and it keeps working in airplane mode. You are not renting someone else's servers to watch your own home.

It also understands things a normal motion camera cannot. A cheap camera can only tell you *"something moved."* Nightjar can tell the difference between a person walking past and a person who **lingers** (loitering), or between a **delivery** being dropped off and a **package being stolen**, because it reasons about what it sees *over time*, not just in a single frame.

## How it works (in plain terms)

A vision-language model, the kind of AI that can look at an image and answer questions about it, is powerful but expensive. Running one on every video frame would flatten a phone's battery in minutes. So **Nightjar almost never runs it.** It uses two tiers, and this is where the trading background came in.

- **Tier 1 is a tiny hand-written motion filter** using Arm's NEON vector instructions. It runs on the CPU, costs about **58 microseconds per frame**, and throws away **roughly 88 percent of frames** as "nothing is happening." *Think of each frame as a market tick:* the vast majority are noise, and you reject them almost for free.
- **Tier 2 is the expensive vision-language model** (SmolVLM-500M, quantized to 4-bit, on the Arm CPU via llama.cpp and **Arm's KleidiAI** kernels). It only wakes for the small fraction of frames the cheap filter lets through. *An alert is like an order:* rare, and worth spending real work on.

On top of that I applied the techniques a trading system uses to stay fast and honest:

- **A lock-free conflating queue** between the tiers that always keeps the newest frame and drops stale ones, so the slow model never makes the fast path wait, *exactly like dropping stale market data rather than queueing it.*
- **Tail-latency discipline:** the number I optimize is not the average, it is the **p99 worst case**, because a guard that is usually fast but occasionally slow is a guard that misses the one moment that matters.
- **Efficiency-vs-performance core scheduling:** the cheap filter is scheduled toward the efficiency cores, the expensive model toward the performance cores. *That big.LITTLE split is an Arm specialty.*
- **An anti-coordinated-omission harness** that feeds frames on the clip's real 30fps schedule and never waits for the system to be free, so the latency numbers are honest under load rather than flattering.

The last piece is turning the English sentence into a rule. Small models are unreliable if you ask them to write a whole structured rule in one shot (I measured about **2 out of 8** correct). So the model only answers small, focused questions, which it does reliably (**12 out of 12** in my eval), and plain deterministic code assembles the rule. A confirmation screen shows you exactly what it understood before it arms. And crucially, **at run time there is no AI in the decision path at all**: the model only reports simple per-frame facts (*"is there a person? a box?"*), and microsecond deterministic code turns those facts, over time, into the alert. **That separation of perception from reasoning is what makes it both trustworthy and cheap.**

## What I optimized, and the results

Everything here is a **real number a judge can reproduce**, not a marketing figure.

- **88 percent** of the expensive model's work is avoided by the cheap gate.
- The NEON gate costs about **58 microseconds per frame**, and I show it compiling to *real Arm SIMD instructions* (`uabd.16b`, `uaddw.8h`), not autovectorized scalar, with a scalar twin and a parity test.
- **Encode-once KV-cache reuse:** encode the image a single time per event and reuse it across questions, **586 ms to 229 ms (2.6x)**, with identical answers.
- The fast-path decoupling keeps the gate's **p99 flat under burst (4 ms to 73 microseconds, 54x)**.
- On **Linux aarch64** I benchmarked Arm's **KleidiAI** kernels on and off and report the honest delta.
- The **same portable C++ engine runs on three Arm platforms from one codebase**: macOS (M2 Max), Linux aarch64 in Docker, and iPhone A15.

A judge can clone the repo and run `make demo` to watch the whole pipeline run and print its own per-stage latency report in **about five minutes, with no phone and no model.**

## Why it matters (impact and vision)

Programming a camera to notice a specific thing normally means hiring an ML team, collecting a labelled dataset, and paying for a cloud GPU. **Nightjar collapses that to a single sentence on hardware you already own.** This is the ***spreadsheet moment for computer vision***: spreadsheets let ordinary people express logic without being programmers, and a whole economy followed. Nightjar lets ordinary people program a camera without being ML engineers.

Three things make it matter beyond a hackathon:

1. **The CPU has far more headroom than the GPU-first narrative assumes.** In a moment of GPU scarcity, showing that a 4-bit VLM plus a NEON gate can do useful, always-on vision on a phone *CPU* is a genuinely useful direction for developers, and it is exactly the Arm efficiency story.
2. **It reuses hardware instead of buying it.** Every guarding phone is one fewer device in a landfill and zero new silicon. *The greenest accelerator is the one already in your drawer.*
3. **It is private by architecture, not by policy.** The pixels never leave the device, so there is no feed to leak, subpoena, or monetize. That is the opposite of the surveillance-capitalism default.

And **security is only the first sentence.** The same engine, with a different sentence, becomes *elder-care* ("tell me if grandma has not moved in two hours"), *child-safety* ("if the baby climbs out of the crib"), or *accessibility*. The reusable idea underneath, **a small model as a per-frame sensor with deterministic code as the brain**, is a blueprint for on-device AI well past cameras.

## Challenges I ran into

The two hardest problems were both latency-and-reliability problems, which is where the trading background helped. A small model is noisy and occasionally slow, so I could not trust it in the hot path or on a single frame; **the conflating queue and the debounced, deterministic temporal reasoning solved that.** And a small model cannot reliably emit a whole rule at once, which forced the **decompose-and-assemble compiler plus the confirmation screen.** Throughout, I kept every claim honest: the harness measures under load without coordinated omission, and the published numbers include the misses.

## What's next

- **Run it end to end on bare Arm silicon.** Port the engine onto a **Raspberry Pi 5**, a pure Arm board with *no Apple GPU at all*, fed by the open-source Linux camera stack (**libcamera / rpicam**). The engine is already portable C++ that builds and passes its tests on Linux aarch64, so this turns a spare phone or a fifty-dollar board into a dedicated, always-on, CPU-only Arm guard, and it is the strongest possible proof of the thesis: **Arm hardware has far more to give than a GPU-first world assumes.**
- **A wider rule vocabulary and richer temporal conditions**, so more of what people can say in a sentence just works.
- **Package the engine and its measurement harness as a reusable edge-AI starting point**, so other developers can drop their own model behind the same NEON gate, conflating queue, and honest telemetry.

## Honest boundaries

This is not a life-safety system. A small model has false negatives, and I publish them. There is no face recognition and no "known vs stranger." No video is stored or leaves the device except the single alert crop you configure.

## See it in five minutes

Clone the repo and run `make demo` (no phone, no model) to watch the full pipeline and its self-generated report. The video shows the live guard on a Mac webcam catching someone in real time. Every number above, the setup, and a 5-tier validation ladder for judges are in the repository.
