import SwiftUI

private let ink = Color(red: 0.05, green: 0.04, blue: 0.03)
private let amber = Color(red: 0.98, green: 0.68, blue: 0.20)
private let dim = Color.white.opacity(0.55)

struct ContentView: View {
    @State private var trigger = "appears"
    @State private var running = false
    @State private var result: NightjarResult?

    var body: some View {
        ZStack {
            LinearGradient(colors: [ink, Color(red: 0.10, green: 0.07, blue: 0.05)],
                           startPoint: .top, endPoint: .bottom)
                .ignoresSafeArea()

            ScrollView {
                VStack(spacing: 22) {
                    OwlView().frame(width: 120, height: 120).padding(.top, 24)

                    VStack(spacing: 4) {
                        Text("NIGHTJAR").font(.system(size: 34, weight: .bold, design: .rounded))
                            .tracking(4).foregroundColor(.white)
                        Text("on-device AI guard · nothing leaves the phone")
                            .font(.footnote).foregroundColor(dim)
                    }

                    rulePicker
                    armButton
                    if let r = result { ResultCard(result: r) }

                    Text("The full C++ engine runs on this device. Tier-2 here is the\ndeterministic scripted VLM — no model, no cloud, no camera needed.")
                        .font(.caption2).foregroundColor(dim)
                        .multilineTextAlignment(.center).padding(.top, 4)
                }
                .padding(.horizontal, 20).padding(.bottom, 40)
                .onAppear {
                    // Screenshot/UI-test hook: NJ_AUTORUN=appears|loiter arms the
                    // guard automatically on launch. No effect in normal use.
                    if let t = ProcessInfo.processInfo.environment["NJ_AUTORUN"] {
                        trigger = t; runGuard()
                    }
                }
            }
        }
    }

    private var rulePicker: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("YOUR RULE").font(.caption).tracking(2).foregroundColor(dim)
            ForEach([("appears", "A person appears", "person.fill.viewfinder"),
                     ("loiter", "Someone loiters (≥ dwell)", "clock.badge.exclamationmark")], id: \.0) { key, label, icon in
                Button { trigger = key; result = nil } label: {
                    HStack(spacing: 12) {
                        Image(systemName: icon).frame(width: 24)
                        Text(label).fontWeight(.medium)
                        Spacer()
                        if trigger == key { Image(systemName: "checkmark.circle.fill") }
                    }
                    .padding(14)
                    .foregroundColor(trigger == key ? ink : .white)
                    .background(trigger == key ? amber : Color.white.opacity(0.06))
                    .cornerRadius(14)
                }
            }
        }
    }

    private func runGuard() {
        running = true; result = nil
        let t = trigger
        DispatchQueue.global(qos: .userInitiated).async {
            let r = NightjarBridge.runGuardDemo(withTrigger: t)
            DispatchQueue.main.async { result = r; running = false }
        }
    }

    private var armButton: some View {
        Button { runGuard() } label: {
            HStack {
                if running { ProgressView().tint(ink) }
                Text(running ? "Watching…" : "Arm the guard")
                    .fontWeight(.bold)
            }
            .frame(maxWidth: .infinity).padding(16)
            .background(amber).foregroundColor(ink).cornerRadius(16)
        }
        .disabled(running)
    }
}

// Otto — the night guard. Drawn, not an asset, so it scales crisp.
struct OwlView: View {
    var body: some View {
        Canvas { ctx, size in
            let w = size.width, h = size.height
            let body = Path(ellipseIn: CGRect(x: w*0.12, y: h*0.10, width: w*0.76, height: h*0.85))
            ctx.fill(body, with: .color(amber.opacity(0.18)))
            ctx.stroke(body, with: .color(amber), lineWidth: 3)
            // ears
            for dx in [0.24, 0.60] {
                var e = Path()
                e.move(to: CGPoint(x: w*dx, y: h*0.16))
                e.addLine(to: CGPoint(x: w*(dx+0.08), y: h*0.02))
                e.addLine(to: CGPoint(x: w*(dx+0.16), y: h*0.16))
                ctx.stroke(e, with: .color(amber), lineWidth: 3)
            }
            // eyes
            for cx in [0.36, 0.64] {
                let eye = Path(ellipseIn: CGRect(x: w*cx - w*0.12, y: h*0.30, width: w*0.24, height: h*0.24))
                ctx.fill(eye, with: .color(.white))
                let pupil = Path(ellipseIn: CGRect(x: w*cx - w*0.05, y: h*0.38, width: w*0.10, height: h*0.10))
                ctx.fill(pupil, with: .color(ink))
            }
            // beak
            var beak = Path()
            beak.move(to: CGPoint(x: w*0.50, y: h*0.54))
            beak.addLine(to: CGPoint(x: w*0.45, y: h*0.62))
            beak.addLine(to: CGPoint(x: w*0.55, y: h*0.62))
            beak.closeSubpath()
            ctx.fill(beak, with: .color(amber))
        }
    }
}

struct ResultCard: View {
    let result: NightjarResult

    var body: some View {
        VStack(spacing: 16) {
            if result.alert.isEmpty {
                banner(icon: "checkmark.shield", tint: dim, title: "No alert",
                       sub: "Motion seen, but the rule's condition wasn't met.")
            } else {
                banner(icon: "bell.badge.fill", tint: amber, title: "ALERT",
                       sub: result.alert)
            }
            Divider().overlay(Color.white.opacity(0.1))
            LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 14) {
                stat("Frames replayed", "\(result.framesProcessed)")
                stat("VLM inferences", "\(result.vlmInferences)")
                stat("Gate cost p50", String(format: "%.1f µs", result.gateP50us))
                stat("Event→alert p50", String(format: "%.0f ms", result.e2eP50ms))
                stat("Conflation drops", "\(result.conflationDrops)")
                stat("Arm ISA", result.isa)
            }
        }
        .padding(18)
        .background(Color.white.opacity(0.05)).cornerRadius(18)
        .overlay(RoundedRectangle(cornerRadius: 18).stroke(Color.white.opacity(0.08)))
    }

    private func banner(icon: String, tint: Color, title: String, sub: String) -> some View {
        HStack(spacing: 14) {
            Image(systemName: icon).font(.title).foregroundColor(tint)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).font(.headline).foregroundColor(.white)
                Text(sub).font(.subheadline).foregroundColor(dim)
            }
            Spacer()
        }
    }

    private func stat(_ label: String, _ value: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(label).font(.caption2).foregroundColor(dim)
            Text(value).font(.system(.body, design: .monospaced)).foregroundColor(.white)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}
