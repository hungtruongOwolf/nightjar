# Nightjar, demo video script (aim ~2:00, demo ~1:00)

Record on the Mac (NightjarMac, real webcam). Lead with the hook. Don't explain
the plumbing; show that it works, then show why it's fast on Arm. Keep the energy
up. Optional: an ntfy topic set + your phone on screen for the push moment.

Setup: `cd shells/ios && xcodegen && xcodebuild -scheme NightjarMac -configuration Release build`, open the app, grant camera. Good light, you'll walk in as the "intruder".

---

### Hook (0:00 to 0:12)
Face the camera or show the drawer of old phones.
> "There are 300 billion Arm chips on the planet, and the smartest one you own is
> asleep in a drawer. I woke mine up. One sentence, and a spare phone is a
> security camera that runs all its AI on the Arm CPU. No cloud. No NVIDIA."

### Why it matters (0:12 to 0:25)
> "Normally, making a camera notice what you care about needs an ML team and a
> cloud GPU streaming your living room to someone's server. Nightjar does it in
> plain English, on hardware you already own, and nothing leaves the device."

### Demo, about one minute (0:25 to 1:20)
Keep it moving. On the Mac:
- Type *"tell me if someone loiters near my car after 10pm"*, send. "I just say what to watch for."
- The WHO / WHERE / WHEN chips appear. "Compiled once, on the device."
- Start guarding, walk into frame. The box tracks you, the alert fires. "It caught me, with the exact rule I typed."
- (If ntfy is set) hold up the phone as it buzzes with the crop. "And it pushed one photo to my phone, the only thing that ever leaves."

### The Arm optimization, the part that wins (1:20 to 1:50)
Open the Monitor while you talk.
> "Here is why it fits on a phone. Two tiers. A hand-written Arm NEON gate costs
> 58 microseconds a frame and throws away 88 percent of them. Only the few
> percent that pass wake the INT4 SmolVLM, which runs on the Arm CPU through
> llama.cpp and KleidiAI. Every number here is measured and reproducible, the
> gate provably compiles to real NEON instructions, and the same C++ engine runs
> on macOS, Linux aarch64, and iPhone. No discrete GPU anywhere."

### Close (1:50 to 2:00)
> "No cloud, no account, no NVIDIA. Clone the repo, run make demo, and you see the
> whole pipeline in five minutes. That old phone finally has a night job."

---

Notes for the edit:
- If you go over, cut the "why it matters" lines, not the Arm-optimization part.
- On-screen text to flash during the Arm section: "88% of frames gated", "NEON 58 us/frame", "INT4 SmolVLM on Arm CPU (KleidiAI)", "3 Arm platforms, one codebase".
- The point to land: not "look how much it does", but "look how little the expensive model has to run, because the cheap Arm-CPU gate does the heavy lifting".
