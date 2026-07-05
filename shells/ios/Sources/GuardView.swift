import SwiftUI

// Owns the live C++ engine and republishes its stream for SwiftUI.
final class EngineDriver: ObservableObject {
    @Published var frame: CGImage?
    @Published var stats = NJStats()
    @Published var alertText: String?
    private let engine = NightjarEngine()

    func start(trigger: String) {
        engine.start(withTrigger: trigger,
                     onFrame: { [weak self] img, s in
                         self?.frame = img
                         self?.stats = s
                     },
                     onAlert: { [weak self] text in self?.alertText = text })
    }
    func stop() { engine.stop() }
}

struct GuardView: View {
    let trigger: String
    let armedCount: Int
    let onExit: () -> Void

    @StateObject private var driver = EngineDriver()
    @State private var awake = true
    @State private var since = Date()
    @State private var showAlert = false

    var body: some View {
        ZStack {
            NW.guardBg.ignoresSafeArea()
            if awake { liveScreen } else { dimScreen }
            if showAlert { alertOverlay.transition(.identity) }
        }
        .onAppear { driver.start(trigger: trigger); since = Date() }
        .onDisappear { driver.stop() }
        .onChange(of: driver.alertText) { new in
            guard new != nil else { return }
            withAnimation(.easeOut(duration: 0.4)) { showAlert = true }
            DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
                withAnimation { showAlert = false }
                driver.alertText = nil
            }
        }
        .contentShape(Rectangle())
        .onTapGesture { withAnimation { awake.toggle() } }
    }

    // ── LIVE ──
    private var liveScreen: some View {
        VStack(spacing: 0) {
            GeometryReader { geo in
                ZStack {
                    if let f = driver.frame {
                        Image(decorative: f, scale: 1, orientation: .up)
                            .resizable().aspectRatio(contentMode: .fill)
                            .frame(width: geo.size.width, height: geo.size.height).clipped()
                    } else { NW.guardBg }
                    Dither().opacity(0.5)
                    ScanLine()
                    ZoneAndMotion(motion: driver.stats.motion.boolValue ? driver.stats.motionRect : .zero)
                    RadialGradient(colors: [.clear, .black.opacity(0.55)], center: .center, startRadius: 40, endRadius: 320)
                        .allowsHitTesting(false)
                    VStack {
                        HStack {
                            HStack(spacing: 8) {
                                BlinkDot(color: NW.rose)
                                Text("LIVE · frames stay on this phone").font(.system(size: 11.5, weight: .semibold)).foregroundColor(NW.creamDim)
                            }.padding(.horizontal, 13).padding(.vertical, 7).background(Color.black.opacity(0.78)).clipShape(Capsule())
                            Spacer()
                            ClockText().padding(.horizontal, 11).padding(.vertical, 7).background(Color.black.opacity(0.78)).clipShape(Capsule())
                        }
                        Spacer()
                        HStack {
                            Text("REAR CAM · ZONE “backyard” · \(uptime)")
                                .font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.6))
                                .padding(.horizontal, 12).padding(.vertical, 6).background(Color.black.opacity(0.78)).clipShape(Capsule())
                            Spacer()
                        }
                    }.padding(14).padding(.top, 40)
                }
            }
            statsPanel
        }
    }

    private var statsPanel: some View {
        VStack(spacing: 11) {
            HStack(spacing: 9) {
                StatTile(value: "\(armedCount)", label: "rules armed")
                StatTile(value: "\(driver.stats.framesSkippedPct)%", label: "frames skipped")
                StatTile(value: String(format: "%.0f ms", max(driver.stats.eventToAlertMs, 0)), label: "event→alert")
                StatTile(value: "◉", label: "running cool", valueColor: NW.green)
            }
            HStack {
                Text("Tap to dim · \(driver.stats.vlmChecks) VLM checks · gate \(String(format: "%.2f", driver.stats.gateMs)) ms")
                    .font(.system(size: 11.5)).foregroundColor(NW.muted(0.35))
                Spacer()
                Button(action: onExit) {
                    Text("Rules").font(.system(size: 11.5, weight: .semibold)).foregroundColor(NW.muted(0.55))
                        .padding(.horizontal, 14).padding(.vertical, 9)
                        .overlay(RoundedRectangle(cornerRadius: 10).stroke(NW.muted(0.28), style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
                }
            }
        }.padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 40)
    }

    // ── DIM ──
    private var dimScreen: some View {
        VStack(spacing: 0) {
            HStack(spacing: 8) {
                BreatheDot(color: NW.green)
                Text("Guarding").font(.system(size: 12.5, weight: .semibold)).foregroundColor(NW.muted(0.85))
            }.padding(.horizontal, 15).padding(.vertical, 8).background(NW.pill).clipShape(Capsule()).padding(.top, 70)

            Bob { OttoOwl(size: 118, alert: showAlert) }.padding(.top, 40)
            ClockBig().padding(.top, 20)
            Text("Otto's on watch — sleep tight.").font(NW.serif(17, italic: true)).foregroundColor(NW.muted(0.7)).padding(.top, 12)
            Text("\(armedCount) rules armed · all quiet").font(.system(size: 12.5)).foregroundColor(NW.muted(0.4)).padding(.top, 5)
            Text("Tap anywhere to peek at the camera").font(.system(size: 12.5)).foregroundColor(NW.muted(0.3)).padding(.top, 26)
            Spacer()
        }.frame(maxWidth: .infinity)
    }

    // ── ALERT ──
    private var alertOverlay: some View {
        ZStack {
            Flash()
            VStack(spacing: 8) {
                HStack(spacing: 12) {
                    ZStack { RoundedRectangle(cornerRadius: 11).fill(NW.rose).frame(width: 40, height: 40)
                        OttoOwl(size: 30, alert: true, bodyColor: .white, showBelly: false) }
                    VStack(alignment: .leading, spacing: 2) {
                        HStack { Text("Nightjar").font(.system(size: 13, weight: .semibold)).foregroundColor(NW.cream); Spacer(); Text("now").font(.system(size: 11)).foregroundColor(NW.muted(0.45)) }
                        Text((driver.alertText ?? "") + " · Photo attached.").font(.system(size: 13)).foregroundColor(NW.muted(0.85)).lineLimit(2)
                    }
                    RoundedRectangle(cornerRadius: 10).fill(NW.rose.opacity(0.25)).frame(width: 46, height: 46)
                }.padding(13).background(NW.card.opacity(0.97)).cornerRadius(20)
                    .overlay(RoundedRectangle(cornerRadius: 20).stroke(NW.muted(0.12)))
                    .shadow(color: .black.opacity(0.6), radius: 20, y: 16)
                HStack(spacing: 9) {
                    Circle().fill(Color(hex: 0xFACC15)).frame(width: 7, height: 7)
                    Text("Porch light switched on via Home Assistant").font(.system(size: 12)).foregroundColor(NW.muted(0.7))
                    Spacer()
                }.padding(.horizontal, 14).padding(.vertical, 10).background(NW.card.opacity(0.94)).cornerRadius(14)
            }.padding(.horizontal, 12).padding(.top, 58)
            Spacer()
        }
    }

    private var uptime: String {
        let s = max(0, Int(Date().timeIntervalSince(since)))
        return String(format: "UP %02d:%02d", s / 60, s % 60)
    }
}

// ── overlay pieces ──
struct Dither: View {
    var body: some View {
        Canvas { ctx, size in
            for y in stride(from: 0, to: size.height, by: 3) {
                for x in stride(from: 0, to: size.width, by: 3) where (Int(x) + Int(y)) % 6 == 0 {
                    ctx.fill(Path(CGRect(x: x, y: y, width: 1, height: 1)), with: .color(.white.opacity(0.05)))
                }
            }
        }.allowsHitTesting(false)
    }
}
struct ScanLine: View {
    var body: some View {
        GeometryReader { geo in
            TimelineView(.animation) { tl in
                let p = tl.date.timeIntervalSinceReferenceDate.truncatingRemainder(dividingBy: 3.6) / 3.6
                LinearGradient(colors: [.clear, NW.rose.opacity(0.06), .clear], startPoint: .top, endPoint: .bottom)
                    .frame(height: 110).offset(y: p * (geo.size.height + 110) - 110)
            }
        }.allowsHitTesting(false)
    }
}
struct ZoneAndMotion: View {
    let motion: CGRect
    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width, h = geo.size.height
            TimelineView(.animation) { tl in
                let phase = -tl.date.timeIntervalSinceReferenceDate * 18
                Canvas { ctx, _ in
                    let zone = Path(roundedRect: CGRect(x: w * 0.1, y: h * 0.08, width: w * 0.8, height: h * 0.86), cornerRadius: 18)
                    ctx.fill(zone, with: .color(NW.rose.opacity(0.10)))
                    ctx.stroke(zone, with: .color(NW.rose), style: StrokeStyle(lineWidth: 1.3, dash: [5, 4], dashPhase: phase))
                    if motion != .zero {
                        let r = CGRect(x: motion.minX * w, y: motion.minY * h, width: motion.width * w, height: motion.height * h)
                        ctx.stroke(Path(roundedRect: r, cornerRadius: 4), with: .color(NW.rose), lineWidth: 2)
                    }
                }
            }
        }.allowsHitTesting(false)
    }
}
struct Flash: View {
    @State private var o = 0.35
    var body: some View {
        NW.rose.opacity(o).ignoresSafeArea().allowsHitTesting(false)
            .onAppear { withAnimation(.easeOut(duration: 0.55)) { o = 0 } }
    }
}
struct ClockText: View {
    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            Text(Self.fmt.string(from: Date())).font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.65))
        }
    }
    static let fmt: DateFormatter = { let f = DateFormatter(); f.dateFormat = "HH:mm:ss"; return f }()
}
struct ClockBig: View {
    var body: some View {
        TimelineView(.periodic(from: .now, by: 1)) { _ in
            Text(Self.fmt.string(from: Date())).font(.system(size: 64, weight: .ultraLight)).foregroundColor(NW.cream.opacity(0.92))
        }
    }
    static let fmt: DateFormatter = { let f = DateFormatter(); f.dateFormat = "HH:mm"; return f }()
}
struct StatTile: View {
    let value: String; let label: String; var valueColor: Color = NW.cream
    var body: some View {
        VStack(spacing: 1) {
            Text(value).font(.system(size: 16, weight: .bold)).foregroundColor(valueColor)
            Text(label).font(.system(size: 10)).foregroundColor(NW.muted(0.45))
        }.frame(maxWidth: .infinity).padding(.vertical, 10).background(NW.cardAlt).cornerRadius(13)
    }
}
