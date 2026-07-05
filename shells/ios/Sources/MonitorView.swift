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

                    // Hardware / power — the efficiency story, live.
                    VStack(alignment: .leading, spacing: 12) {
                        MonoLabel(text: "HARDWARE & POWER", size: 9.5, opacity: 0.5)
                        HStack(spacing: 12) {
                            Bar(label: "VLM compute avoided", pct: Int(s.framesSkippedPct), tint: NW.green)
                        }
                        Row(k: "Thermal state", v: thermal().0, vColor: thermal().1)
                        Row(k: "Expensive path (VLM)", v: "\(s.vlmChecks) of \(s.framesProcessed) frames")
                        Row(k: "Perf-per-watt lever", v: "gate skips \(s.framesSkippedPct)% before the VLM")
                        Row(k: "Data leaving device", v: "none", vColor: NW.green)
                    }.padding(16).background(NW.card).cornerRadius(16)

                    Text("Efficiency isn't a number to hit — it's the design: the cheap NEON gate discards most frames so the expensive VLM (P-cores) runs on a few %. That's the perf-per-watt lever this monitor shows live.")
                        .font(.system(size: 12)).lineSpacing(3).foregroundColor(NW.muted(0.5))

                    if !driver.usingCamera || true {
                        Text("Note — this build's Tier-2 is the scripted stand-in and recognizes person/motion only; other subjects need the on-device VLM (they won't false-fire). Benchmark numbers of record come from the offline replay harness (make demo → report.md), not this app.")
                            .font(.system(size: 11)).lineSpacing(3).foregroundColor(NW.muted(0.38))
                    }
                    Spacer()
                }.padding(.horizontal, 20)
            }
        }
        .buttonStyle(.plain)
        .preferredColorScheme(.dark)
    }

    private func fmt(_ v: Double, _ unit: String) -> String { v > 0 ? String(format: "%.2f %@", v, unit) : "—" }

    private func thermal() -> (String, Color) {
        switch ProcessInfo.processInfo.thermalState {
        case .nominal: return ("nominal · running cool", NW.green)
        case .fair: return ("fair · a bit warm", Color(hex: 0xFACC15))
        case .serious: return ("serious · pacing itself", Color(hex: 0xFB923C))
        case .critical: return ("critical · motion-only", NW.rose)
        @unknown default: return ("—", NW.cream)
        }
    }
}

// A labelled progress bar (0–100).
private struct Bar: View {
    let label: String; let pct: Int; let tint: Color
    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack { Text(label).font(.system(size: 12)).foregroundColor(NW.muted(0.6)); Spacer()
                Text("\(pct)%").font(NW.mono(12)).foregroundColor(tint) }
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(NW.muted(0.1)).frame(height: 7)
                    Capsule().fill(tint).frame(width: g.size.width * CGFloat(min(max(pct, 0), 100)) / 100, height: 7)
                }
            }.frame(height: 7)
        }.frame(maxWidth: .infinity)
    }
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
    let k: String; let v: String; var vColor: Color = NW.cream
    var body: some View {
        HStack { Text(k).font(.system(size: 13)).foregroundColor(NW.muted(0.55)); Spacer(); Text(v).font(NW.mono(12)).foregroundColor(vColor) }
    }
}
