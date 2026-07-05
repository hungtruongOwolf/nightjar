import SwiftUI

enum Screen { case hello, chat, confirm, rules, guarding }

struct RuleItem: Identifiable {
    let id = UUID()
    var title: String
    var sub: String
    var icon: String
    var trigger: String?
    var on: Bool
}

struct ContentView: View {
    @State private var screen: Screen
    @State private var ruleText: String
    @State private var parsed: NJParsedRule?
    @State private var rules: [RuleItem] = [
        .init(title: "Person appears in the backyard", sub: "10 PM – 6 AM · Notify + photo", icon: "figure.walk", trigger: "appears", on: true),
    ]

    init() {
        let env = ProcessInfo.processInfo.environment
        _ruleText = State(initialValue: env["NJ_RULE_TEXT"] ?? "")
        switch env["NJ_SCREEN"] {
        case "chat": _screen = State(initialValue: .chat)
        case "confirm": _screen = State(initialValue: .confirm)
        case "rules": _screen = State(initialValue: .rules)
        case "guard": _screen = State(initialValue: .guarding)
        default: _screen = State(initialValue: .hello)
        }
        if env["NJ_SCREEN"] == "confirm", let t = env["NJ_RULE_TEXT"] {
            _parsed = State(initialValue: NightjarEngine.compileRule(t))
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
                HelloView { go(.chat) }.transition(.opacity)
            case .chat:
                ChatView(text: $ruleText, onSend: { t in parsed = NightjarEngine.compileRule(t); go(.confirm) })
                    .transition(.move(edge: .trailing).combined(with: .opacity))
            case .confirm:
                ConfirmView(ruleText: ruleText, parsed: parsed ?? NightjarEngine.compileRule(ruleText),
                            onRewrite: { go(.chat) }, onConfirm: { addRuleAndGuard() })
                    .transition(.move(edge: .trailing).combined(with: .opacity))
            case .rules:
                RulesView(rules: $rules, onStart: { go(.guarding) }, onAdd: { ruleText = ""; go(.chat) })
                    .transition(.move(edge: .trailing).combined(with: .opacity))
            case .guarding:
                GuardView(trigger: activeTrigger, armedCount: rules.filter { $0.on }.count, onExit: { go(.rules) })
                    .transition(.opacity)
            }
        }
        .preferredColorScheme(.dark)
    }

    private func go(_ s: Screen) { withAnimation(.easeOut(duration: 0.38)) { screen = s } }

    private func addRuleAndGuard() {
        let p = parsed ?? NightjarEngine.compileRule(ruleText)
        let icon = p.trigger == "loiter" ? "clock.badge.exclamationmark" : "figure.walk"
        rules.insert(.init(title: p.title, sub: "\(p.when) · \(p.then)", icon: icon, trigger: p.trigger, on: true), at: 0)
        // keep only this newly-armed rule active for a clean demo
        for i in rules.indices where i != 0 { rules[i].on = false }
        go(.rules)
    }
}

// ─────────────────────────────── S0 HELLO ───────────────────────────────
struct HelloView: View {
    let onStart: () -> Void
    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 9) { BlinkDot(color: NW.rose); MonoLabel(text: "NIGHTJAR v0.1 — NIGHT GUARD", opacity: 0.5) }.padding(.top, 60)
            HStack(alignment: .bottom, spacing: 14) {
                Bob { OttoOwl(size: 100) }
                Text("otto —\nyour night owl").font(NW.serif(15.5, italic: true)).foregroundColor(NW.muted(0.55)).padding(.bottom, 10)
            }.padding(.top, 22)
            (Text("Your old phone\njust got\n").foregroundColor(NW.cream) + Text("a night job.").foregroundColor(NW.rose).italic())
                .font(NW.serif(50)).lineSpacing(2).padding(.top, 20)
            Text("Tell it what to watch for — in your own words. It thinks right here on the phone. Not one frame leaves the house.")
                .font(.system(size: 14.5)).lineSpacing(4).foregroundColor(NW.muted(0.55)).padding(.top, 16).frame(maxWidth: 300, alignment: .leading)
            HStack(spacing: 7) { FeatureChip(text: "ON-DEVICE"); FeatureChip(text: "MANY RULES"); FeatureChip(text: "WAKES YOU") }.padding(.top, 22)
            Spacer()
            Button(action: onStart) {
                HStack(spacing: 8) { Text("Set up my guard"); Image(systemName: "arrow.right") }
                    .font(.system(size: 16, weight: .semibold)).foregroundColor(.white).frame(maxWidth: .infinity).padding(17).background(NW.rose).cornerRadius(16)
            }
            Marquee(text: "100% ON-DEVICE · WORKS IN AIRPLANE MODE · NO CLOUD · NO ACCOUNT · ARM64 NEON INT4 · ").padding(.top, 16)
        }.padding(.horizontal, 26).padding(.bottom, 24)
    }
}

// ─────────────────────────────── S1 CHAT ───────────────────────────────
struct ChatView: View {
    @Binding var text: String
    let onSend: (String) -> Void
    private let suggestions = ["someone in the backyard at night", "someone loitering at the front door", "a package left on the porch"]

    var body: some View {
        VStack(spacing: 10) {
            MonoLabel(text: "NEW RULE", size: 10).padding(.top, 56)
            Spacer()
            HStack(alignment: .bottom, spacing: 9) {
                OttoOwl(size: 30, showBelly: false)
                Text("What should I watch for? Say it like you'd tell a neighbour.")
                    .font(.system(size: 15)).foregroundColor(NW.creamDim).padding(13)
                    .background(NW.bubble).clipShape(RoundedCorner(16, corners: [.topLeft, .topRight, .bottomRight]))
                Spacer(minLength: 20)
            }
            HStack { Spacer(minLength: 35)
                VStack(alignment: .leading, spacing: 7) {
                    ForEach(suggestions, id: \.self) { g in
                        Button { text = "Tell me if there's " + g } label: {
                            Text(g).font(.system(size: 12.5)).foregroundColor(NW.muted(0.6))
                                .padding(.horizontal, 12).padding(.vertical, 8).overlay(Capsule().stroke(NW.muted(0.18)))
                        }
                    }
                }
            }.padding(.leading, 0)
            inputBar
        }.padding(.horizontal, 16).padding(.bottom, 14)
    }

    private var inputBar: some View {
        HStack(spacing: 9) {
            TextField("", text: $text, prompt: Text("Type a rule…").foregroundColor(NW.muted(0.4)))
                .font(.system(size: 15)).foregroundColor(NW.cream).textFieldStyle(.plain).padding(.leading, 6)
            Button { if !text.isEmpty { onSend(text) } } label: {
                Image(systemName: "arrow.up").font(.system(size: 16, weight: .bold)).foregroundColor(.white)
                    .frame(width: 34, height: 34).background(text.isEmpty ? NW.muted(0.2) : NW.rose).clipShape(Circle())
            }.disabled(text.isEmpty)
        }.padding(8).background(NW.bubble).cornerRadius(24)
    }
}

// ─────────────────────────────── S2 CONFIRM ───────────────────────────────
struct ConfirmView: View {
    let ruleText: String
    let parsed: NJParsedRule
    let onRewrite: () -> Void
    let onConfirm: () -> Void

    var body: some View {
        VStack(spacing: 12) {
            MonoLabel(text: "NEW RULE", size: 10).padding(.top, 56)
            Spacer()
            Text(ruleText).font(.system(size: 15)).foregroundColor(.white).padding(13)
                .background(NW.rose).clipShape(RoundedCorner(16, corners: [.topLeft, .topRight, .bottomLeft]))
                .frame(maxWidth: .infinity, alignment: .trailing)
            HStack(alignment: .top, spacing: 9) {
                OttoOwl(size: 30, showBelly: false)
                VStack(alignment: .leading, spacing: 10) {
                    Text("Here's my understanding —").font(.system(size: 14.5)).foregroundColor(NW.creamDim)
                    LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 8) {
                        chip("WHO", parsed.who); chip("WHERE", parsed.where)
                        chip("WHEN", parsed.when); chip("THEN", parsed.then)
                    }
                    Text("Compiled once, on this phone. Checked in microseconds — no AI at match time.")
                        .font(.system(size: 12)).foregroundColor(NW.muted(0.4))
                }.padding(15).background(NW.bubble).clipShape(RoundedCorner(16, corners: [.topLeft, .topRight, .bottomRight]))
                Spacer(minLength: 8)
            }
            Spacer()
            HStack(spacing: 9) {
                Button(action: onRewrite) {
                    Text("Rewrite").font(.system(size: 14.5, weight: .semibold)).foregroundColor(NW.muted(0.7))
                        .frame(width: 104).padding(.vertical, 15).overlay(RoundedRectangle(cornerRadius: 14).stroke(NW.muted(0.22)))
                }
                Button(action: onConfirm) {
                    Text("Looks right — start").font(.system(size: 14.5, weight: .semibold)).foregroundColor(.white)
                        .frame(maxWidth: .infinity).padding(.vertical, 15).background(NW.rose).cornerRadius(14)
                }
            }
        }.padding(.horizontal, 16).padding(.bottom, 20)
    }

    private func chip(_ k: String, _ v: String) -> some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(k).font(NW.mono(8.5)).tracking(1.5).foregroundColor(NW.muted(0.45))
            Text(v).font(.system(size: 13.5, weight: .semibold)).foregroundColor(NW.cream)
        }.frame(maxWidth: .infinity, alignment: .leading).padding(.horizontal, 11).padding(.vertical, 9)
            .background(NW.rose.opacity(0.11)).overlay(RoundedRectangle(cornerRadius: 12).stroke(NW.rose.opacity(0.32))).cornerRadius(12)
    }
}

// ─────────────────────────────── S4 RULES ───────────────────────────────
struct RulesView: View {
    @Binding var rules: [RuleItem]
    let onStart: () -> Void
    let onAdd: () -> Void
    private var armed: Int { rules.filter { $0.on }.count }

    var body: some View {
        VStack(spacing: 12) {
            HStack(alignment: .firstTextBaseline) {
                (Text("The ").foregroundColor(NW.cream) + Text("watch list").foregroundColor(NW.rose).italic() + Text(".").foregroundColor(NW.cream)).font(NW.serif(32))
                Spacer(); MonoLabel(text: "\(armed) ARMED", opacity: 0.4); OttoOwl(size: 34, showBelly: false)
            }.padding(.top, 56)
            HStack(spacing: 10) { BreatheDot(color: NW.green); Text("Ready to guard · frames never leave the phone").font(.system(size: 13.5)).foregroundColor(NW.muted(0.8)); Spacer() }
                .padding(13).background(NW.card).cornerRadius(16)
            ForEach($rules) { $r in
                HStack(spacing: 12) {
                    ZStack { RoundedRectangle(cornerRadius: 10).fill(r.on ? NW.rose.opacity(0.14) : NW.muted(0.07)).frame(width: 34, height: 34)
                        Image(systemName: r.icon).font(.system(size: 15)).foregroundColor(r.on ? NW.rose : NW.muted(0.35)) }
                    VStack(alignment: .leading, spacing: 1) {
                        Text(r.title).font(.system(size: 14.5, weight: .semibold)).foregroundColor(NW.creamDim).lineLimit(1)
                        Text(r.sub).font(.system(size: 12)).foregroundColor(NW.muted(0.5))
                    }
                    Spacer(); Toggle("", isOn: $r.on).labelsHidden().tint(NW.rose)
                }.padding(13).background(NW.card).cornerRadius(16)
            }
            Button(action: onAdd) {
                HStack(spacing: 10) { Image(systemName: "plus"); Text("Add a rule") }.font(.system(size: 15, weight: .semibold)).foregroundColor(NW.rose)
                    .frame(maxWidth: .infinity).padding(14).overlay(RoundedRectangle(cornerRadius: 16).stroke(NW.muted(0.25), style: StrokeStyle(lineWidth: 1, dash: [4, 4])))
            }
            Spacer()
            Button(action: onStart) {
                Text("Start guarding").font(.system(size: 16, weight: .semibold)).foregroundColor(.white).frame(maxWidth: .infinity).padding(17).background(NW.rose).cornerRadius(16)
            }.disabled(armed == 0).opacity(armed == 0 ? 0.5 : 1)
            MonoLabel(text: "PROP AT A WINDOW · KEEP PLUGGED IN", size: 8.5, opacity: 0.3)
        }.padding(.horizontal, 20).padding(.bottom, 24)
    }
}

// ─────────────────────────────── small pieces ───────────────────────────────
struct BlinkDot: View {
    let color: Color
    var body: some View { TimelineView(.animation) { tl in
        Rectangle().fill(color).frame(width: 8, height: 8).opacity(Int(tl.date.timeIntervalSinceReferenceDate * 1.25) % 2 == 0 ? 1 : 0.1) } }
}
struct BreatheDot: View {
    let color: Color
    var body: some View { TimelineView(.animation) { tl in
        Circle().fill(color).frame(width: 8, height: 8).opacity(0.7 + 0.3 * sin(tl.date.timeIntervalSinceReferenceDate * 3)) } }
}
struct Bob<Content: View>: View {
    @ViewBuilder let content: Content
    var body: some View { TimelineView(.animation) { tl in content.offset(y: -4 * sin(tl.date.timeIntervalSinceReferenceDate * 1.4)) } }
}
struct FeatureChip: View {
    let text: String
    var body: some View { Text(text).font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.6)).padding(.horizontal, 11).padding(.vertical, 7).overlay(Capsule().stroke(NW.muted(0.16))) }
}
struct Marquee: View {
    let text: String
    var body: some View {
        GeometryReader { geo in TimelineView(.animation) { tl in
            let off = -CGFloat(tl.date.timeIntervalSinceReferenceDate * 30).truncatingRemainder(dividingBy: geo.size.width)
            HStack(spacing: 0) { Text(text + text).font(NW.mono(8.5)).tracking(2.2).foregroundColor(NW.muted(0.28)).fixedSize().offset(x: off) }
        } }.frame(height: 14).clipped()
    }
}

// Rounded specific corners (chat bubble tails).
struct RoundedCorner: Shape {
    var radius: CGFloat; var corners: UIRectCornerCompat
    init(_ r: CGFloat, corners: UIRectCornerCompat) { radius = r; self.corners = corners }
    func path(in rect: CGRect) -> Path {
        var p = Path()
        let tl = corners.contains(.topLeft) ? radius : 0, tr = corners.contains(.topRight) ? radius : 0
        let bl = corners.contains(.bottomLeft) ? radius : 0, br = corners.contains(.bottomRight) ? radius : 0
        p.move(to: CGPoint(x: rect.minX + tl, y: rect.minY))
        p.addLine(to: CGPoint(x: rect.maxX - tr, y: rect.minY)); p.addArc(center: CGPoint(x: rect.maxX - tr, y: rect.minY + tr), radius: tr, startAngle: .degrees(-90), endAngle: .degrees(0), clockwise: false)
        p.addLine(to: CGPoint(x: rect.maxX, y: rect.maxY - br)); p.addArc(center: CGPoint(x: rect.maxX - br, y: rect.maxY - br), radius: br, startAngle: .degrees(0), endAngle: .degrees(90), clockwise: false)
        p.addLine(to: CGPoint(x: rect.minX + bl, y: rect.maxY)); p.addArc(center: CGPoint(x: rect.minX + bl, y: rect.maxY - bl), radius: bl, startAngle: .degrees(90), endAngle: .degrees(180), clockwise: false)
        p.addLine(to: CGPoint(x: rect.minX, y: rect.minY + tl)); p.addArc(center: CGPoint(x: rect.minX + tl, y: rect.minY + tl), radius: tl, startAngle: .degrees(180), endAngle: .degrees(270), clockwise: false)
        p.closeSubpath(); return p
    }
}

struct UIRectCornerCompat: OptionSet {
    let rawValue: Int
    static let topLeft = UIRectCornerCompat(rawValue: 1 << 0)
    static let topRight = UIRectCornerCompat(rawValue: 1 << 1)
    static let bottomLeft = UIRectCornerCompat(rawValue: 1 << 2)
    static let bottomRight = UIRectCornerCompat(rawValue: 1 << 3)
}
