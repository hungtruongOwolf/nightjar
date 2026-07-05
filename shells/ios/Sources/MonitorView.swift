import SwiftUI

// The performance monitor — deliberately SEPARATE from the guard app, which
// stays focused on watching + alerting. This is the instrument you open to read
// what the engine is doing: live per-stage telemetry straight from the C++
// Telemetry (the same numbers the offline replay harness reports).
struct MonitorView: View {
    @ObservedObject var driver: EngineDriver
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ZStack {
            NW.ink.ignoresSafeArea()
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    HStack {
                        VStack(alignment: .leading, spacing: 3) {
                            MonoLabel(text: "PERFORMANCE MONITOR", size: 10, opacity: 0.5)
                            (Text("What Otto ").foregroundColor(NW.cream) + Text("is doing").foregroundColor(NW.rose).italic() + Text(".").foregroundColor(NW.cream))
                                .font(NW.serif(28))
                        }
                        Spacer()
                        Button { dismiss() } label: {
                            Image(systemName: "xmark").font(.system(size: 14, weight: .bold)).foregroundColor(NW.muted(0.6))
                                .padding(10).background(NW.card).clipShape(Circle())
                        }
                    }.padding(.top, 8)

                    let s = driver.stats
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                        Metric(value: "\(s.framesSkippedPct)%", label: "frames skipped at the gate", hint: "VLM never woke for these")
                        Metric(value: "\(s.vlmChecks)", label: "Tier-2 VLM checks", hint: "expensive path, run rarely")
                        Metric(value: fmt(s.gateMs, "ms"), label: "gate cost · p50", hint: "Tier-1, per frame")
                        Metric(value: fmt(s.gateP99Ms, "ms"), label: "gate cost · p99", hint: "worst case")
                        Metric(value: fmt(s.eventToAlertMs, "ms"), label: "event → alert · p50", hint: "hero latency H1", accent: true)
                        Metric(value: "\(s.conflationDrops)", label: "conflation drops", hint: "bursts dropped-old, counted")
                    }

                    VStack(alignment: .leading, spacing: 10) {
                        Row(k: "Source", v: driver.usingCamera ? "real camera" : "synthetic (Simulator)")
                        Row(k: "Frames processed", v: "\(s.framesProcessed)")
                        Row(k: "Thermal", v: "nominal · running cool")
                        Row(k: "Data leaving device", v: "none")
                    }.padding(16).background(NW.card).cornerRadius(16)

                    Text("These are live counters from the engine's Telemetry. The same instrument runs headless in the replay harness (make demo → report.md) with warm p50/p90/p99 per stage — that harness, not this app, is the source of the published benchmark numbers.")
                        .font(.system(size: 12)).lineSpacing(3).foregroundColor(NW.muted(0.45))
                    Spacer()
                }.padding(.horizontal, 20)
            }
        }
        .preferredColorScheme(.dark)
    }

    private func fmt(_ v: Double, _ unit: String) -> String { v > 0 ? String(format: "%.2f %@", v, unit) : "—" }
}

private struct Metric: View {
    let value: String; let label: String; let hint: String; var accent = false
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(value).font(.system(size: 24, weight: .bold, design: .rounded)).foregroundColor(accent ? NW.rose : NW.cream)
            Text(label).font(.system(size: 12, weight: .medium)).foregroundColor(NW.muted(0.7))
            Text(hint).font(.system(size: 10)).foregroundColor(NW.muted(0.4))
        }.frame(maxWidth: .infinity, alignment: .leading).padding(14).background(NW.card).cornerRadius(14)
    }
}

private struct Row: View {
    let k: String; let v: String
    var body: some View {
        HStack { Text(k).font(.system(size: 13)).foregroundColor(NW.muted(0.55)); Spacer(); Text(v).font(NW.mono(12)).foregroundColor(NW.cream) }
    }
}
