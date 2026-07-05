# NightjarKT1, on-device VLM latency bench (KT1) + ISA dump (KT6)

Measures SmolVLM-500M INT4 inference latency (encode / prefill / decode split)
on a real iPhone, warm, over 50 consecutive inferences. The number this app
prints decides the project's C1 go/no-go gate.

## Build

1. Build the llama.cpp XCFramework (once):
   ```sh
   git clone --depth 1 https://github.com/ggml-org/llama.cpp.git ../vendor/llama.cpp
   cd ../vendor/llama.cpp && ./build-xcframework.sh   # needs cmake + full Xcode
   ```
   The project expects the framework at `../../../vendor/llama.cpp/build-apple/llama.xcframework`
   (i.e. `vendor/` sits next to the nightjar checkout).
2. Generate the Xcode project: `xcodegen -s project.yml` (brew install xcodegen)
3. Drop model + test files (git-ignored):
   - `Resources/models/SmolVLM-500M-Instruct-Q4_0.gguf`
   - `Resources/models/mmproj-SmolVLM-500M-Instruct-f16.gguf`
   - `Resources/images/*.jpg` (448px letterboxed test frames)
4. Open `NightjarKT1.xcodeproj`, set Signing → your Personal Team,
   plug in the iPhone (Developer Mode on), Run.

## Measurement discipline

- Warm-up inference excluded from stats; run ≥30 min of repeated benches
  before quoting sustained numbers.
- Report p50/p90/p99 with the ISA block the app shows (KT6) + commit hash.
- Free-provisioning signatures expire after 7 days, re-run from Xcode weekly.
