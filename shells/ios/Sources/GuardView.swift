import AVFoundation
import SwiftUI

// Owns the live C++ engine + camera, republishes the stream for SwiftUI.
// Uses the real camera when present (device / Mac webcam); on the Simulator it
// falls back to the engine's synthetic scene.
// Rolling-window counts (last N minutes) — the honest "right now" numbers, not
// a lifetime average that converges to a meaningless constant.
struct WindowStats {
    var frames = 0, gated = 0, vlm = 0, alerts = 0
    var minutes = 0.0
    var skippedPct: Int { frames > 0 ? Int((100.0 * Double(frames - gated) / Double(frames)).rounded()) : 0 }
}

final class EngineDriver: ObservableObject {
    @Published var frame: CGImage?      // synthetic mode only
    @Published var stats = NJStats()
    @Published var alertText: String?
    @Published var usingCamera = false
    @Published var tier2 = "loading…"
    @Published var loading = false
    @Published var alerts: [AlertRecord] = []
    @Published var battery = BatteryInfo()
    @Published var enduranceText = "measuring…"

    let engine = NightjarEngine()
    private lazy var camera = CameraCapture(engine: engine)
    var session: AVCaptureSession { camera.session }

    private var samples: [(t: Date, cap: Int, gated: Int, vlm: Int)] = []
    private var synthRing: [CGImage] = []
    private var batTimer: Timer?
    private var batStart: (Date, Int)?

    func windowed(minutes: Double) -> WindowStats {
        let cutoff = Date().addingTimeInterval(-minutes * 60)
        let win = samples.filter { $0.t >= cutoff }
        guard let f = win.first, let l = win.last, win.count >= 2 else { return WindowStats() }
        var w = WindowStats()
        w.frames = l.cap - f.cap; w.gated = l.gated - f.gated; w.vlm = l.vlm - f.vlm
        w.alerts = alerts.filter { $0.date >= cutoff }.count
        w.minutes = l.t.timeIntervalSince(f.t) / 60
        return w
    }

    private func ingest(_ s: NJStats) {
        stats = s
        let now = Date()
        if let last = samples.last, now.timeIntervalSince(last.t) < 0.9 { return }  // ~1 sample/s
        samples.append((now, Int(s.framesProcessed), Int(s.framesGated), Int(s.vlmChecks)))
        let cutoff = now.addingTimeInterval(-330)
        samples.removeAll { $0.t < cutoff }
    }

    func start(trigger: String, subject: String, zone: [CGPoint]) {
        alertText = nil; samples.removeAll(); synthRing.removeAll()
        engine.setSubject(subject)
        engine.setZonePolygon(zone.isEmpty ? [] : zone.map(njPoint))
        startBattery()
        if CameraCapture.hasCamera {
            usingCamera = true
            loading = true
            DispatchQueue.global(qos: .userInitiated).async {  // load the VLM off-main
                self.engine.startCamera(withTrigger: trigger,
                                        onStats: { [weak self] s in self?.ingest(s) },
                                        onAlert: { [weak self] t in self?.record(t) })
                self.camera.start()
                let name = self.engine.tier2Name()
                DispatchQueue.main.async { self.tier2 = name; self.loading = false }
            }
        } else {
            usingCamera = false
            engine.startSynthetic(withTrigger: trigger,
                                  onFrame: { [weak self] img, s in self?.frame = img; self?.pushSynth(img); self?.ingest(s) },
                                  onAlert: { [weak self] t in self?.record(t) })
            tier2 = engine.tier2Name()
        }
    }

    func stop() { camera.stop(); engine.stop(); batTimer?.invalidate(); batTimer = nil }

    private func pushSynth(_ img: CGImage) {
        synthRing.append(img); if synthRing.count > 48 { synthRing.removeFirst() }
    }

    // Each alert keeps the photo AND a short event clip (last ~2s). Evidence
    // stays on-device (the engine's EventClipStore is the fuller on-disk clip).
    private func record(_ text: String) {
        alertText = text
        let img: CGImage? = usingCamera ? engine.currentSnapshotCopy() : frame
        let clip: [CGImage] = usingCamera
            ? ((engine.recentClipFrames() as? [Any])?.map { $0 as! CGImage } ?? [])
            : subsample(synthRing, 24)
        alerts.insert(AlertRecord(text: text, image: img, frames: clip, date: Date()), at: 0)
        if alerts.count > 12 { alerts.removeLast() }  // clips hold frames — cap memory
    }

    private func startBattery() {
        battery = Battery.read()
        batStart = battery.percent >= 0 ? (Date(), battery.percent) : nil
        batTimer?.invalidate()
        batTimer = Timer.scheduledTimer(withTimeInterval: 15, repeats: true) { [weak self] _ in self?.tickBattery() }
        tickBattery()
    }

    private func tickBattery() {
        let b = Battery.read(); battery = b
        if b.charging { enduranceText = "on power (endurance is measured on battery)"; return }
        if let (t0, p0) = batStart, b.percent >= 0 {
            let hrs = Date().timeIntervalSince(t0) / 3600
            let drop = Double(p0 - b.percent)
            if hrs > 0.03 && drop >= 1 {
                let remain = Double(b.percent) / (drop / hrs)
                enduranceText = String(format: "~%.1f h left at this rate", remain)
                return
            }
        }
        if let sys = b.systemHoursRemaining { enduranceText = String(format: "~%.1f h left (system est.)", sys) }
        else { enduranceText = "measuring…" }
    }
}

private func subsample(_ a: [CGImage], _ n: Int) -> [CGImage] {
    guard a.count > n, n > 0 else { return a }
    let step = max(a.count / n, 1)
    return stride(from: 0, to: a.count, by: step).map { a[$0] }
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
    @ObservedObject var driver: EngineDriver
    let trigger: String
    var subject: String = "person"
    let armedCount: Int
    var zone: [CGPoint] = []
    let onExit: () -> Void

    @State private var since = Date()
    @State private var showAlert = false
    @State private var showMonitor = false
    @State private var showAlerts = false
    @State private var dimmed = false

    var body: some View {
        ZStack {
            NW.guardBg.ignoresSafeArea()
            VStack(spacing: 0) {
                if dimmed { dimScreen } else { preview }
                controlBar
            }
            if showAlert { alertOverlay }
        }
        .onAppear {
            driver.start(trigger: trigger, subject: subject, zone: zone); since = Date()
            // Screenshot hook: NJ_OPEN=monitor|alerts auto-opens that sheet.
            if let o = ProcessInfo.processInfo.environment["NJ_OPEN"] {
                DispatchQueue.main.asyncAfter(deadline: .now() + 18) {
                    if o == "monitor" { showMonitor = true } else if o == "alerts" { showAlerts = true }
                }
            }
        }
        .onDisappear { driver.stop() }
        .onChange(of: driver.alertText) { new in
            guard new != nil else { return }
            withAnimation(.easeOut(duration: 0.4)) { showAlert = true }
            DispatchQueue.main.asyncAfter(deadline: .now() + 4) {
                withAnimation { showAlert = false }; driver.alertText = nil
            }
        }
        .sheet(isPresented: $showMonitor) { MonitorView(driver: driver) }
        .sheet(isPresented: $showAlerts) { AlertsView(alerts: driver.alerts) }
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
                        Text("\(driver.usingCamera ? "CAMERA" : "SIM") · \(driver.tier2) · \(uptime)")
                            .font(NW.mono(9)).tracking(1.2).foregroundColor(NW.muted(0.6))
                            .padding(.horizontal, 12).padding(.vertical, 6).background(Color.black.opacity(0.78)).clipShape(Capsule())
                        Spacer()
                    }
                }.padding(14).padding(.top, 40)
                if driver.loading {
                    HStack(spacing: 10) {
                        ProgressView().tint(NW.rose)
                        Text("Loading SmolVLM…").font(.system(size: 13, weight: .semibold)).foregroundColor(NW.creamDim)
                    }.padding(.horizontal, 18).padding(.vertical, 12).background(Color.black.opacity(0.8)).clipShape(Capsule())
                }
            }
        }
    }

    // Guard stays focused on the core: watching + the alert. Numbers live in the
    // separate Monitor.
    private var controlBar: some View {
        HStack(spacing: 9) {
            BreatheDot(color: dimmed ? NW.muted(0.4) : NW.green)
            Text(dimmed ? "Dimmed" : "Guarding").font(.system(size: 12.5, weight: .semibold)).foregroundColor(NW.muted(0.85))
            Spacer(minLength: 4)
            Button { withAnimation { dimmed.toggle() } } label: { pill(dimmed ? "Wake" : "Dim") }
            Button { showAlerts = true } label: { pill(driver.alerts.isEmpty ? "Alerts" : "Alerts \(driver.alerts.count)", accent: !driver.alerts.isEmpty) }
            Button { showMonitor = true } label: { pill("Monitor") }
            Button(action: onExit) { pill("Rules") }
        }.padding(.horizontal, 14).padding(.top, 14).padding(.bottom, 30)
    }

    private func pill(_ t: String, accent: Bool = false) -> some View {
        Text(t).font(.system(size: 12, weight: .semibold)).foregroundColor(accent ? NW.rose : NW.muted(0.7))
            .padding(.horizontal, 11).padding(.vertical, 9)
            .overlay(Capsule().stroke(accent ? NW.rose.opacity(0.6) : NW.muted(0.28), style: StrokeStyle(lineWidth: 1, dash: [3, 3])))
    }

    // Power-save: hide the preview (engine keeps running), show Otto + clock.
    private var dimScreen: some View {
        VStack(spacing: 0) {
            Spacer()
            Bob { OttoOwl(size: 110, alert: showAlert) }
            ClockBig().padding(.top, 18)
            Text("Otto's on watch — screen dimmed to save power.")
                .font(NW.serif(16, italic: true)).foregroundColor(NW.muted(0.6)).padding(.top, 12).multilineTextAlignment(.center)
            Text("\(armedCount) armed · all quiet").font(.system(size: 12.5)).foregroundColor(NW.muted(0.4)).padding(.top, 5)
            Spacer()
        }.frame(maxWidth: .infinity, maxHeight: .infinity)
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
