# Nightjar, demo video script (about 2:00)

One continuous story, not a slideshow. Read the voiceover straight through and let
the screen follow. Record the guard on the Mac webcam (NightjarMac). Confident,
unhurried, a little personal. No music with lyrics.

Before you record: build and open the Mac app (`cd shells/ios && xcodegen && xcodebuild -scheme NightjarMac -configuration Release build`), grant the camera, optionally set an ntfy topic and have your phone in frame. Good light. You are the person who walks in.

---

## The voiceover (read it as one take)

> This is a phone I stopped using three years ago. Today it is worth almost nothing.
> In the next minute it becomes a security guard I program just by talking to it,
> that runs entirely on its own chip, and never sends a single frame to the cloud.
>
> Everyone right now is fighting over GPUs. But the most capable AI hardware most
> people own is already sitting dead in a drawer. I wanted to prove that old Arm
> phone still has a lot left to give.
>
> So I just tell it, in plain English, what to watch for.
> *(type: "tell me if someone loiters near my car after 10pm")*
> It understands the sentence, and shows me what it heard before it arms anything.
> Then it watches. When I walk in,
> *(walk into frame; the box tracks you; the alert fires)*
> it catches me, and buzzes my real phone with the photo. No training, no setup,
> no cloud.
>
> Here is the trick that makes this run on a phone instead of a server. I spent
> years in high-frequency trading, where the whole game is doing the most work in
> the least time and never missing the one event that matters. Same idea here.
> A tiny hand-written Arm NEON filter checks every frame in 58 microseconds and
> throws away 88 percent of them. The expensive vision model only ever wakes for
> the few that count. The frame is the tick, the alert is the order, and I tune
> the worst case, not the average. All of it on the Arm CPU. No NVIDIA, no cloud,
> and every number here is reproducible.
>
> And it is the same engine whether the sentence is "someone near my car" or
> "tell me if grandma has not moved in two hours." This is the spreadsheet moment
> for computer vision: anyone can program what a camera notices, on hardware they
> already own, private by default. A billion dead phones, each one a private AI
> sensor waiting for a job.
>
> Clone the repo, run make demo, and you see the whole thing in five minutes.
> Nightjar. Give your old phone a night job.

---

## What to show, and rough timing

| Time | Voiceover beat | On screen |
|---|---|---|
| 0:00 to 0:12 | the hook (a dead phone becomes a guard) | hold up an old phone, or the app's hello screen with Otto |
| 0:12 to 0:22 | GPUs are scarce, the hardware is in a drawer | you talking, or a drawer of old phones |
| 0:22 to 0:52 | tell it a rule, it confirms, it catches me | the demo: type the rule, the WHO/WHERE/WHEN chips, start guard, walk in, alert fires, phone buzzes |
| 0:52 to 1:30 | the trick, the HFT idea, the Arm-CPU numbers | open the Monitor while you talk |
| 1:30 to 1:50 | same engine, any sentence, the vision | the rules list / a couple of example sentences on screen |
| 1:50 to 2:00 | make demo, the closing line | a terminal running `make demo`, then the app |

## Text to flash on screen during the Arm section (0:52 to 1:30)

`88% of frames gated` · `NEON gate, 58 us/frame` · `INT4 SmolVLM on the Arm CPU (KleidiAI)` · `no NVIDIA, no cloud` · `3 Arm platforms, one codebase`

## The one thing to land

Not "look how much it does." The point is **how little the expensive model has to run, because a tiny Arm-CPU filter does the heavy lifting.** If you only have time for one idea, make it that one.
