# Devpost description (paste into the submission text field)

The repo URL and the video link go in their own fields on the form. Track: Track 3, Mobile AI. Setup and full validation live in the repo (README + JUDGES.md), so this text is the pitch, not a manual.

---

**A spare phone in a drawer is a supercomputer with a camera. I gave mine a job.**

Nightjar turns an old phone into a security camera you program in one plain-English sentence, with every bit of the AI running on the Arm CPU. You type "tell me if someone loiters near my car after 10pm." It compiles that once, on the device, then watches. Nothing ever leaves the phone.

**What makes it more than a demo is how little the expensive model runs.** A hand-written Arm NEON motion gate costs 58 microseconds a frame and throws away about 88% of frames. Only the 1-5% that pass wake an INT4 SmolVLM-500M running on the Arm CPU through llama.cpp and KleidiAI. No NVIDIA, no CUDA, no cloud. And none of that is a claim: 88% of VLM compute gated, encode-once KV-cache reuse at 2.6x, a fast path that keeps gate p99 flat under burst (54x), and a KleidiAI on/off benchmark on Linux aarch64, all reproducible. I even show the gate compiling to real NEON instructions rather than autovectorized scalar. The same portable C++ engine runs on three Arm platforms from one codebase: macOS, Linux aarch64, and iPhone A15.

**Why it is more than a camera.** It separates perception from reasoning. The small model is only a per-frame fact sensor ("is there a person? a box?"), and deterministic microsecond code integrates those facts over time. So it fires on things a normal detector cannot express, like loitering, or a package left behind versus one taken, and there is never a model call in the hot decision path.

**Why it matters.** Programming a camera drops from "hire an ML team and rent a cloud GPU" to a single sentence, on hardware you already own, private by design because the pixels never leave the device. It puts a billion idle Arm phones back to work instead of buying new silicon. Security is only the first sentence; elder-care, child-safety and accessibility are the same engine with a different one.

**See for yourself in five minutes.** Clone the repo and run `make demo` (no phone, no model) to watch the whole pipeline run and print its own per-stage latency report. The video shows the live guard on a Mac webcam catching someone in real time. Every number above, the setup, and a 5-tier validation ladder for judges are in the repo.
