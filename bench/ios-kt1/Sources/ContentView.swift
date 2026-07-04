import SwiftUI

struct ContentView: View {
    @StateObject private var bench = BenchRunner()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                Text("Nightjar KT1 — on-device VLM latency")
                    .font(.headline)

                GroupBox("ISA (KT6)") {
                    Text(ISA.summary)
                        .font(.system(.caption, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

                GroupBox("Memory") {
                    Text("os_proc_available_memory: \(ISA.availableMemoryMB) MB")
                        .font(.system(.caption, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                }

                Button(bench.running ? "Running…" : "Run bench (50 inferences)") {
                    bench.start()
                }
                .buttonStyle(.borderedProminent)
                .disabled(bench.running)

                GroupBox("Log") {
                    Text(bench.log.isEmpty ? "idle" : bench.log)
                        .font(.system(.caption2, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .textSelection(.enabled)
                }
            }
            .padding()
        }
        // Guard-mode habit from day one: the bench must survive a long run
        // with the screen unattended.
        .onAppear { UIApplication.shared.isIdleTimerDisabled = true }
    }
}
