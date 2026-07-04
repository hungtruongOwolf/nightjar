import Foundation

/// Drives the KT1 bench: warm-up excluded, 50 timed inferences cycling the
/// bundled images, p50/p90/p99 reported per stage (encode/prefill/decode).
@MainActor
final class BenchRunner: ObservableObject {
    @Published var running = false
    @Published var log = ""

    private static let runs = 50
    private static let question = "Is there a person in this image? Answer y or n."

    func start() {
        guard !running else { return }
        running = true
        log = ""

        Task.detached(priority: .userInitiated) { [weak self] in
            guard let self else { return }
            await self.runBench()
            await MainActor.run { self.running = false }
        }
    }

    private func runBench() async {
        guard
            let modelPath = Bundle.main.path(forResource: "SmolVLM-500M-Instruct-Q4_0",
                                             ofType: "gguf", inDirectory: "models"),
            let mmprojPath = Bundle.main.path(forResource: "mmproj-SmolVLM-500M-Instruct-f16",
                                              ofType: "gguf", inDirectory: "models")
        else {
            await append("ERROR: models not found in bundle (Resources/models/*.gguf)")
            return
        }
        let images = Bundle.main.paths(forResourcesOfType: "jpg", inDirectory: "images")
        guard !images.isEmpty else {
            await append("ERROR: no test images in bundle (Resources/images/*.jpg)")
            return
        }

        await append("device ISA:\n\(ISA.summary)")
        await append("mem before load: \(ISA.availableMemoryMB) MB available")

        do {
            let t0 = Date()
            // Encoder on Metal — the default configuration from the Mac KT1
            // finding (CPU encoder measured ~34x slower there).
            let bench = try MtmdBench(modelPath: modelPath, mmprojPath: mmprojPath,
                                      encoderUseGPU: true)
            await append(String(format: "model loaded in %.1fs · mem: %d MB available",
                                Date().timeIntervalSince(t0), ISA.availableMemoryMB))

            // Warm-up (excluded from stats — cold numbers are reported separately).
            let warm = try bench.infer(imagePath: images[0], question: Self.question)
            await append(String(format: "warm-up (cold, excluded): %.0fms enc=%.0f pre=%.0f dec=%.0f → \"%@\"",
                                warm.totalMs, warm.encodeMs, warm.prefillMs, warm.decodeMs, warm.output))

            var timings: [MtmdBench.StageTiming] = []
            for i in 0..<Self.runs {
                let img = images[i % images.count]
                let t = try bench.infer(imagePath: img, question: Self.question)
                timings.append(t)
                if (i + 1) % 10 == 0 {
                    await append("… \(i + 1)/\(Self.runs) · mem: \(ISA.availableMemoryMB) MB")
                }
            }
            await report(timings)
        } catch {
            await append("ERROR: \(error)")
        }
    }

    private func report(_ timings: [MtmdBench.StageTiming]) async {
        func pct(_ values: [Double], _ p: Double) -> Double {
            let sorted = values.sorted()
            let idx = Int(p / 100.0 * Double(sorted.count - 1))
            return sorted[idx]
        }
        func row(_ name: String, _ values: [Double]) -> String {
            String(format: "%@  p50=%5.0f  p90=%5.0f  p99=%5.0f ms",
                   name, pct(values, 50), pct(values, 90), pct(values, 99))
        }
        await append("""
        ── KT1 result (\(timings.count) warm runs) ──
        \(row("encode ", timings.map(\.encodeMs)))
        \(row("prefill", timings.map(\.prefillMs)))
        \(row("decode ", timings.map(\.decodeMs)))
        \(row("TOTAL  ", timings.map(\.totalMs)))
        outputs: \(Set(timings.map(\.output)).sorted().joined(separator: " | "))
        """)
    }

    private func append(_ line: String) async {
        await MainActor.run { log += line + "\n" }
    }
}
