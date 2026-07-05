import AVFoundation
import SwiftUI

// Owns the live C++ engine + camera, republishes the stream for SwiftUI.
// Uses the real camera when present (device / Mac webcam); on the Simulator it
// falls back to the engine's synthetic scene.
final class EngineDriver: ObservableObject {
    @Published var frame: CGImage?      // synthetic mode only
    @Published var stats = NJStats()
    @Published var alertText: String?
    @Published var usingCamera = false

    let engine = NightjarEngine()
    private lazy var camera = CameraCapture(engine: engine)
    var session: AVCaptureSession { camera.session }

    func start(trigger: String, zone: [CGPoint]) {
        if !zone.isEmpty {
            engine.setZonePolygon(zone.map(njPoint))
        }
        if CameraCapture.hasCamera {
            usingCamera = true
            engine.startCamera(withTrigger: trigger,
                               onStats: { [weak self] s in self?.stats = s },
                               onAlert: { [weak self] t in self?.alertText = t })
            camera.start()
        } else {
            usingCamera = false
            engine.startSynthetic(withTrigger: trigger,
                                  onFrame: { [weak self] img, s in self?.frame = img; self?.stats = s },
                                  onAlert: { [weak self] t in self?.alertText = t })
        }
    }
    func stop() { camera.stop(); engine.stop() }
}

// NSValue(CGPoint) differs between iOS and macOS.
func njPoint(_ p: CGPoint) -> NSValue {
    #if os(macOS)
    return NSValue(point: NSPoint(x: p.x, y: p.y))
    #else
    return NSValue(cgPoint: p)
    #endif
}

struct GuardView: View {
    let trigger: String
    let armedCount: Int
    var zone: [CGPoint] = []
    let onExit: () -> Void

    @StateObject private var driver = EngineDriver()
    @State private var since = Date()
    @State private var showAlert = false
    @State private var showMonitor = false

    var body: some View {
        ZStack {
            NW.guardBg.ignoresSafeArea()
            VStack(spacing: 0) {
                preview
                controlBar
            }
            if showAlert { alertOverlay }
        }
        .onAppear { driver.start(trigger: trigger, zone: zone); since = Date() }
        .onDisappear { driver.stop() }
        .onChange(of: driver.alertText) { new in
            guard new != nil else { return }
            withAnimation(.easeOut(duration: 0.4)) { showAlert = true }
            DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
                withAnimation { showAlert = false }; driver.alertText = nil
            }
        }
        .sheet(isPresented: $showMonitor) { MonitorView(driver: driver) }
    }

    private var preview: some View {
        GeometryReader { geo in
            ZStack {
                if driver.usingCamera {
                    CameraPreview(session: driver.session)
                        .frame(width: geo.size.width, height: geo.size.height).clipped()
                } else if let f = driver.frame {
                    Image(decorative: f, scale: 1, orientation: .up)
                        .resizable().aspectRatio(contentMode: .fill)
                        .frame(width: geo.size.width, height: geo.size.height).clipped()
                } else { NW.guardBg }
                ScanLine()
                ZoneAndMotion(motion: driver.stats.motion.boolValue ? driver.stats.motionRect : .zero, zone: zone)
                RadialGradient(colors: [.clear, .black.opacity(0.45)], center: .center, startRadius: 60, endRadius: 380)
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
                        Text("\(driver.usingCamera ? "CAMERA" : "SIM") · ZONE “backyard” · \(uptime)")
                            .font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.6))
                            .padding(.horizontal, 12).padding(.vertical, 6).background(Color.black.opacity(0.78)).clipShape(Capsule())
                        Spacer()
                    }
                }.padding(14).padding(.top, 40)
            }
        }
    }

    // Guard stays focused on the core: watching + the alert. Numbers live in the
    // separate Monitor.
    private var controlBar: some View {
        HStack(spacing: 10) {
            HStack(spacing: 8) {
                BreatheDot(color: NW.green)
                Text("Guarding · \(armedCount) armed").font(.system(size: 13, weight: .semibold)).foregroundColor(NW.muted(0.85))
            }
            Spacer()
            Button { showMonitor = true } label: { pill("Monitor", filled: false) }
            Button(action: onExit) { pill("Rules", filled: false) }
        }.padding(.horizontal, 20).padding(.top, 14).padding(.bottom, 34)
    }

    private func pill(_ t: String, filled: Bool) -> some View {
        Text(t).font(.system(size: 12.5, weight: .semibold)).foregroundColor(NW.muted(0.7))
            .padding(.horizontal, 15).padding(.vertical, 9)
            .overlay(Capsule().stroke(NW.muted(0.28), style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
    }

    private var alertOverlay: some View {
        ZStack {
            Flash()
            VStack {
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
    }

    private var uptime: String {
        let s = max(0, Int(Date().timeIntervalSince(since)))
        return String(format: "UP %02d:%02d", s / 60, s % 60)
    }
}

// ── overlay pieces ──
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
    var zone: [CGPoint] = []
    var body: some View {
        GeometryReader { geo in
            let w = geo.size.width, h = geo.size.height
            TimelineView(.animation) { tl in
                let phase = -tl.date.timeIntervalSinceReferenceDate * 18
                Canvas { ctx, _ in
                    let zonePath: Path = {
                        guard zone.count >= 3 else {
                            return Path(roundedRect: CGRect(x: w * 0.1, y: h * 0.08, width: w * 0.8, height: h * 0.86), cornerRadius: 18)
                        }
                        var p = Path(); p.move(to: CGPoint(x: zone[0].x * w, y: zone[0].y * h))
                        for q in zone.dropFirst() { p.addLine(to: CGPoint(x: q.x * w, y: q.y * h)) }
                        p.closeSubpath(); return p
                    }()
                    ctx.fill(zonePath, with: .color(NW.rose.opacity(0.10)))
                    ctx.stroke(zonePath, with: .color(NW.rose), style: StrokeStyle(lineWidth: 1.3, lineJoin: .round, dash: [5, 4], dashPhase: phase))
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
