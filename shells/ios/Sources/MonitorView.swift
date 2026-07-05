import SwiftUI

// The performance monitor, deliberately SEPARATE from the guard app, which
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
                    let w = driver.windowed(minutes: 2)

                    // Windowed, the accurate "right now", not a lifetime average.
                    MonoLabel(text: w.frames > 0 ? "LAST 2 MINUTES" : "LAST 2 MINUTES · warming up…", size: 9.5, opacity: 0.5)
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                        Metric(value: "\(w.frames)", label: "frames seen", hint: "in the window")
                        Metric(value: "\(w.skippedPct)%", label: "skipped at the gate", hint: "VLM stayed asleep")
                        Metric(value: "\(w.vlm)", label: "VLM checks", hint: "the expensive path")
                        Metric(value: "\(w.alerts)", label: "captures", hint: "alerts fired", accent: true)
                    }

                    MonoLabel(text: "LATENCY (WARM PERCENTILES)", size: 9.5, opacity: 0.5)
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
                        Metric(value: fmt(s.gateMs, "ms"), label: "gate cost · p50", hint: "Tier-1, per frame")
                        Metric(value: fmt(s.gateP99Ms, "ms"), label: "gate cost · p99", hint: "worst case")
                        Metric(value: fmt(s.eventToAlertMs, "ms"), label: "event → alert · p50", hint: "hero latency H1", accent: true)
                        Metric(value: "\(s.conflationDrops)", label: "conflation drops", hint: "bursts dropped-old")
                    }

                    // Hardware / power, the efficiency + endurance story, live.
                    VStack(alignment: .leading, spacing: 12) {
                        MonoLabel(text: "EFFICIENCY (NIGHTJAR'S LEVER)", size: 9.5, opacity: 0.5)
                        // The honest, Nightjar-specific number: how little the expensive
                        // path runs. This is what buys endurance on a dedicated device.
                        Bar(label: "VLM compute avoided (2 min)", pct: w.skippedPct, tint: NW.green)
                        Row(k: "Tier-2 model", v: driver.tier2, vColor: driver.tier2.hasPrefix("Smol") ? NW.green : NW.cream)
                        Row(k: "Thermal state", v: thermal().0, vColor: thermal().1)
                        Row(k: "Data leaving device", v: "none", vColor: NW.green)

                        Divider().overlay(NW.muted(0.1)).padding(.vertical, 2)
                        MonoLabel(text: "BATTERY (WHOLE MACHINE)", size: 9.5, opacity: 0.5)
                        Row(k: "Charge", v: driver.battery.percent >= 0 ? "\(driver.battery.percent)%\(driver.battery.charging ? " · charging" : "")" : "n/a",
                            vColor: driver.battery.charging ? NW.green : NW.cream)
                        Row(k: "Time-to-empty", v: driver.enduranceText, vColor: NW.muted(0.7))
                    }.padding(16).background(NW.card).cornerRadius(16)

                    Text("Time-to-empty is the OS estimate for the WHOLE machine under everything it's running (on a dev Mac: Xcode, builds, the display, Nightjar). No app can measure just its own battery draw without root, so we don't pretend to. Nightjar's real endurance lever is above, the gate skipping most frames, and it's measured properly on a dedicated guard phone, where whole-device ≈ Nightjar.")
                        .font(.system(size: 12)).lineSpacing(3).foregroundColor(NW.muted(0.5))

                    if driver.tier2.hasPrefix("Smol") {
                        Text("Tier-2 is the real SmolVLM-500M (INT4) running on-device, it classifies person / vehicle / animal / package from the actual camera. Benchmark numbers of record come from the offline replay harness (make demo → report.md).")
                            .font(.system(size: 11)).lineSpacing(3).foregroundColor(NW.muted(0.4))
                    } else {
                        Text("Note, Tier-2 here is the scripted stand-in (person/motion only); other subjects need the on-device VLM. On the Mac build the real SmolVLM loads automatically when the model is present.")
                            .font(.system(size: 11)).lineSpacing(3).foregroundColor(NW.muted(0.38))
                    }
                    Spacer()
                }.padding(.horizontal, 20)
            }
        }
        .buttonStyle(.plain)
        .preferredColorScheme(.dark)
    }

    private func fmt(_ v: Double, _ unit: String) -> String { v > 0 ? String(format: "%.2f %@", v, unit) : "-" }

    private func thermal() -> (String, Color) {
        switch ProcessInfo.processInfo.thermalState {
        case .nominal: return ("nominal · running cool", NW.green)
        case .fair: return ("fair · a bit warm", Color(hex: 0xFACC15))
        case .serious: return ("serious · pacing itself", Color(hex: 0xFB923C))
        case .critical: return ("critical · motion-only", NW.rose)
        @unknown default: return ("-", NW.cream)
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
