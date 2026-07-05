# Nightjar — demo video script (≤ 3:00)

Record on the **Mac** (NightjarMac, real webcam). Screen-record with QuickTime
(⌘⇧5) + talk over it. Optional: set an **ntfy topic** first (rules screen) and
have your phone with the free **ntfy** app subscribed, on screen, for the push
"money shot". Keep it under 3 minutes.

Setup before recording:
- Build + open: `cd shells/ios && xcodegen && xcodebuild -scheme NightjarMac -configuration Release build`, then open `NightjarMac.app`, grant camera once.
- Good light; you'll walk into frame as the "intruder".

---

### 0:00–0:15 — Hook (Hello screen)
Show the Hello screen (Otto the owl).
> "This is a spare phone — or here, a Mac webcam. Nightjar turns it into an AI
> guard you program in plain English, and everything runs on-device. Nothing
> leaves the machine."

### 0:15–0:45 — Program it in words (chat → confirm)
Tap **Set up my guard** → type: *"tell me if someone loiters in the backyard after 10pm"* → send.
> "I just describe what to watch for. On-device, it compiles that once into a
> structured rule —"
Point at the WHO / WHERE / WHEN / THEN chips.
> "— who, where, when, what to do. Checked in microseconds at runtime; no AI in
> the hot path. I'll save a video clip."
Pick **Video clip** → **Looks right — mark zone** → trace a zone → **Save zone**.

### 0:45–1:20 — Live guard + the alert (Guard screen)
**Start guarding** → grant camera → the live feed.
> "Now it's watching, live. The cheap NEON motion gate runs every frame; the
> expensive vision model only wakes on motion."
Walk into frame.
> "Motion — the box tracks me — the VLM confirms a person —"
Alert fires (banner + Otto).
> "— and it alerts, with the exact rule I typed, the real time, and the frame it
> caught."
(If ntfy set) hold up your phone as it buzzes:
> "And it pushed to my phone — the only thing that ever left the device: this
> one crop I asked for."

### 1:20–1:45 — Why it's not just a detector (the differentiator)
> "A motion cam can't tell *appears* from *loiters* from *a package left behind*
> — those are conditions over **time**. Nightjar splits perception from
> reasoning: the model is a per-frame fact sensor; deterministic code integrates
> those facts over time. That's the loitering rule firing, not just 'a person'."

### 1:45–2:20 — The Arm optimization story (Monitor)
Open **Monitor**.
> "This is Track 3 — mobile AI optimization. The lever is here: the gate skips
> most frames, so the VLM — real SmolVLM-500M INT4 via llama.cpp + KleidiAI —
> runs on a few percent. Live per-stage latency: gate in microseconds,
> event-to-alert in the hundreds of milliseconds."
Mention over b-roll:
> "Same portable C++ engine runs on macOS, on Linux aarch64 in Docker, and on
> iPhone A15 — three Arm platforms. KleidiAI's INT4 kernels give a measured
> prefill speedup on Linux; numbers are in the repo."

### 2:20–2:45 — Evidence + honesty
Open **Alerts** → tap one → the clip plays.
> "Every alert keeps its clip, tagged by which rule fired — reviewable
> on-device. No continuous recording; we store events, not video."
> "And it's honest: published numbers include the misses, and if it ever runs
> degraded it says so."

### 2:45–3:00 — Close
> "No cloud, no account, no subscription. Clone the repo and run `make demo` on
> any Mac in five minutes — no phone, no model needed. Nightjar: give an old
> phone a night job."

---

**B-roll / cutaways to have ready:** the Monitor screen, the Alerts grid, a
terminal running `make demo`, the README architecture diagram.
