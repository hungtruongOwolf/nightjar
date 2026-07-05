import SwiftUI

enum Screen { case hello, rules, guarding }

struct RuleItem: Identifiable {
    let id = UUID()
    var title: String
    var sub: String
    var icon: String       // SF Symbol
    var trigger: String?   // "appears" | "loiter" | nil (cosmetic)
    var on: Bool
}

struct ContentView: View {
    @State private var screen: Screen
    @State private var rules: [RuleItem]

    init() {
        // Screenshot/UI-test hook: NJ_SCREEN=hello|rules|guard, NJ_TRIGGER=appears|loiter.
        let env = ProcessInfo.processInfo.environment
        let loiter = env["NJ_TRIGGER"] == "loiter"
        _rules = State(initialValue: [
            .init(title: "Person appears in the backyard", sub: "10 PM – 6 AM · Notify + photo", icon: "figure.walk", trigger: "appears", on: !loiter),
            .init(title: "Someone loiters out back", sub: "Sustained ≥ dwell · Notify + photo", icon: "clock.badge.exclamationmark", trigger: "loiter", on: loiter),
            .init(title: "Cat on the kitchen counter", sub: "Anytime · Notify", icon: "pawprint.fill", trigger: nil, on: false),
        ])
        switch env["NJ_SCREEN"] {
        case "rules": _screen = State(initialValue: .rules)
        case "guard": _screen = State(initialValue: .guarding)
        default: _screen = State(initialValue: .hello)
        }
    }

    private var activeTrigger: String {
        rules.first(where: { $0.on && $0.trigger != nil })?.trigger ?? "appears"
    }

    var body: some View {
        ZStack {
            NW.screen.ignoresSafeArea()
            switch screen {
            case .hello:
                HelloView { withAnimation(.easeOut(duration: 0.4)) { screen = .rules } }
                    .transition(.opacity)
            case .rules:
                RulesView(rules: $rules, onStart: { withAnimation(.easeOut(duration: 0.4)) { screen = .guarding } })
                    .transition(.move(edge: .trailing).combined(with: .opacity))
            case .guarding:
                GuardView(trigger: activeTrigger, armedCount: rules.filter { $0.on }.count,
                          onExit: { withAnimation { screen = .rules } })
                    .transition(.opacity)
            }
        }
        .preferredColorScheme(.dark)
    }
}

// ─────────────────────────────── S0 HELLO ───────────────────────────────
struct HelloView: View {
    let onStart: () -> Void
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 9) {
                BlinkDot(color: NW.rose)
                MonoLabel(text: "NIGHTJAR v0.1 — NIGHT GUARD", opacity: 0.5)
            }.padding(.top, 60)

            HStack(alignment: .bottom, spacing: 14) {
                Bob { OttoOwl(size: 100) }
                Text("otto —\nyour night owl")
                    .font(NW.serif(15.5, italic: true)).foregroundColor(NW.muted(0.55))
                    .padding(.bottom, 10)
            }.padding(.top, 22)

            (Text("Your old phone\njust got\n").foregroundColor(NW.cream)
                + Text("a night job.").foregroundColor(NW.rose).italic())
                .font(NW.serif(50)).lineSpacing(2).padding(.top, 20)

            Text("Tell it what to watch for — in your own words. It thinks right here on the phone. Not one frame leaves the house.")
                .font(.system(size: 14.5)).lineSpacing(4).foregroundColor(NW.muted(0.55))
                .padding(.top, 16).frame(maxWidth: 300, alignment: .leading)

            HStack(spacing: 7) {
                FeatureChip(text: "ON-DEVICE")
                FeatureChip(text: "MANY RULES")
                FeatureChip(text: "WAKES YOU")
            }.padding(.top, 22)

            Spacer()

            Button(action: onStart) {
                HStack(spacing: 8) { Text("Set up my guard"); Image(systemName: "arrow.right") }
                    .font(.system(size: 16, weight: .semibold)).foregroundColor(.white)
                    .frame(maxWidth: .infinity).padding(17)
                    .background(NW.rose).cornerRadius(16)
            }
            Marquee(text: "100% ON-DEVICE · WORKS IN AIRPLANE MODE · NO CLOUD · NO ACCOUNT · ARM64 NEON INT4 · ")
                .padding(.top, 16)
        }
        .padding(.horizontal, 26).padding(.bottom, 24)
    }
}

// ─────────────────────────────── S4 RULES ───────────────────────────────
struct RulesView: View {
    @Binding var rules: [RuleItem]
    let onStart: () -> Void
    private var armed: Int { rules.filter { $0.on }.count }

    var body: some View {
        VStack(spacing: 12) {
            HStack(alignment: .firstTextBaseline) {
                (Text("The ").foregroundColor(NW.cream) + Text("watch list").foregroundColor(NW.rose).italic() + Text(".").foregroundColor(NW.cream))
                    .font(NW.serif(32))
                Spacer()
                MonoLabel(text: "\(armed) ARMED", opacity: 0.4)
                OttoOwl(size: 34, showBelly: false)
            }.padding(.top, 56)

            HStack(spacing: 10) {
                BreatheDot(color: NW.green)
                Text("Ready to guard · frames never leave the phone")
                    .font(.system(size: 13.5)).foregroundColor(NW.muted(0.8))
                Spacer()
            }.padding(13).background(NW.card).cornerRadius(16)

            ForEach($rules) { $r in
                HStack(spacing: 12) {
                    ZStack {
                        RoundedRectangle(cornerRadius: 10)
                            .fill(r.on ? NW.rose.opacity(0.14) : NW.muted(0.07)).frame(width: 34, height: 34)
                        Image(systemName: r.icon).font(.system(size: 15))
                            .foregroundColor(r.on ? NW.rose : NW.muted(0.35))
                    }
                    VStack(alignment: .leading, spacing: 1) {
                        Text(r.title).font(.system(size: 14.5, weight: .semibold)).foregroundColor(NW.creamDim)
                        Text(r.sub).font(.system(size: 12)).foregroundColor(NW.muted(0.5))
                    }
                    Spacer()
                    Toggle("", isOn: $r.on).labelsHidden().tint(NW.rose)
                }.padding(13).background(NW.card).cornerRadius(16)
            }

            Spacer()
            Button(action: onStart) {
                Text("Start guarding").font(.system(size: 16, weight: .semibold)).foregroundColor(.white)
                    .frame(maxWidth: .infinity).padding(17).background(NW.rose).cornerRadius(16)
            }.disabled(armed == 0).opacity(armed == 0 ? 0.5 : 1)
            MonoLabel(text: "PROP AT A WINDOW · KEEP PLUGGED IN", size: 8.5, opacity: 0.3)
        }
        .padding(.horizontal, 20).padding(.bottom, 24)
    }
}

// ─────────────────────────────── small pieces ───────────────────────────────
struct BlinkDot: View {
    let color: Color
    var body: some View {
        TimelineView(.animation) { tl in
            let on = Int(tl.date.timeIntervalSinceReferenceDate * 1.25) % 2 == 0
            Rectangle().fill(color).frame(width: 8, height: 8).opacity(on ? 1 : 0.1)
        }
    }
}
struct BreatheDot: View {
    let color: Color
    var body: some View {
        TimelineView(.animation) { tl in
            let o = 0.7 + 0.3 * sin(tl.date.timeIntervalSinceReferenceDate * 3)
            Circle().fill(color).frame(width: 8, height: 8).opacity(o)
        }
    }
}
struct Bob<Content: View>: View {
    @ViewBuilder let content: Content
    var body: some View {
        TimelineView(.animation) { tl in
            content.offset(y: -4 * sin(tl.date.timeIntervalSinceReferenceDate * 1.4))
        }
    }
}
struct FeatureChip: View {
    let text: String
    var body: some View {
        Text(text).font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.6))
            .padding(.horizontal, 11).padding(.vertical, 7)
            .overlay(Capsule().stroke(NW.muted(0.16)))
    }
}
struct Marquee: View {
    let text: String
    var body: some View {
        GeometryReader { geo in
            TimelineView(.animation) { tl in
                let w = geo.size.width
                let off = -CGFloat(tl.date.timeIntervalSinceReferenceDate * 30).truncatingRemainder(dividingBy: w)
                HStack(spacing: 0) {
                    Text(text + text).font(NW.mono(8.5)).tracking(2.2).foregroundColor(NW.muted(0.28))
                        .fixedSize().offset(x: off)
                }
            }
        }.frame(height: 14).clipped()
    }
}
