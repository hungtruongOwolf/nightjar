import SwiftUI

// Faint dither so a placeholder "camera" surface reads like a live sensor.
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

// S3 — mark the watched zone. The drawn shape is rasterized to the gate's block
// grid (in the engine), so motion outside the line is discarded before the VLM
// ever wakes. Three ways to mark: freehand, drag four corners, or full frame.
struct ZoneView: View {
    let zoneName: String
    let onSave: ([CGPoint]) -> Void  // normalized [0,1]; [] = whole frame

    @State private var mode = 0  // 0 draw · 1 corners · 2 full
    @State private var pts: [CGPoint] = [
        (0.30, 0.37), (0.48, 0.31), (0.66, 0.33), (0.80, 0.41), (0.86, 0.54),
        (0.82, 0.69), (0.68, 0.79), (0.50, 0.83), (0.34, 0.79), (0.24, 0.67), (0.22, 0.53), (0.24, 0.43),
    ].map { CGPoint(x: $0.0, y: $0.1) }
    @State private var corners: [CGPoint] = [
        CGPoint(x: 0.16, y: 0.16), CGPoint(x: 0.86, y: 0.13),
        CGPoint(x: 0.92, y: 0.80), CGPoint(x: 0.08, y: 0.84),
    ]
    @State private var drawing = false

    private var polygon: [CGPoint] {
        switch mode {
        case 1: return corners
        case 2: return []  // whole frame
        default: return pts
        }
    }

    var body: some View {
        VStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 3) {
                (Text("Mark the ").foregroundColor(NW.cream) + Text(zoneName.lowercased()).foregroundColor(NW.rose).italic() + Text(".").foregroundColor(NW.cream))
                    .font(NW.serif(30))
                Text(hint).font(.system(size: 13)).foregroundColor(NW.muted(0.5))
            }.frame(maxWidth: .infinity, alignment: .leading).padding(.top, 56)

            modePicker
            canvas
            Text("Round, jagged, banana-shaped — motion outside your line is thrown away before the AI even wakes up.")
                .font(.system(size: 11.5)).foregroundColor(NW.muted(0.38)).lineSpacing(2)

            HStack(spacing: 9) {
                Button { reset() } label: {
                    Text("Reset").font(.system(size: 14.5, weight: .semibold)).foregroundColor(NW.muted(0.7))
                        .frame(width: 104).padding(.vertical, 15).overlay(RoundedRectangle(cornerRadius: 14).stroke(NW.muted(0.22)))
                }
                Button { onSave(polygon) } label: {
                    Text("Save zone →").font(.system(size: 14.5, weight: .semibold)).foregroundColor(.white)
                        .frame(maxWidth: .infinity).padding(.vertical, 15).background(NW.rose).cornerRadius(14)
                }
            }
        }.padding(.horizontal, 20).padding(.bottom, 20)
    }

    private var hint: String {
        switch mode {
        case 1: return "Drag each corner — any four-sided shape."
        case 2: return "The whole camera view is armed."
        default: return "Trace around the area with one finger."
        }
    }

    private var modePicker: some View {
        HStack(spacing: 4) {
            ForEach(Array(["Draw free", "Drag corners", "Full frame"].enumerated()), id: \.offset) { i, label in
                Button { mode = i; drawing = false } label: {
                    Text(label).font(.system(size: 12.5, weight: .semibold))
                        .foregroundColor(mode == i ? .white : NW.muted(0.55))
                        .frame(maxWidth: .infinity).padding(.vertical, 9)
                        .background(mode == i ? NW.rose : Color.clear).cornerRadius(9)
                }
            }
        }.padding(4).background(NW.bubble).cornerRadius(13)
    }

    private var canvas: some View {
        GeometryReader { geo in
            let w = geo.size.width, h = geo.size.height
            ZStack {
                Dither().background(Color(hex: 0x0F0B09))
                RadialGradient(colors: [.clear, .black.opacity(0.5)], center: .center, startRadius: 60, endRadius: 320).allowsHitTesting(false)
                VStack { HStack {
                    MonoLabel(text: "REAR CAM", size: 9, opacity: 0.5); Spacer()
                    HStack(spacing: 6) { BlinkDot(color: NW.rose); MonoLabel(text: "REC", size: 9, opacity: 0.6) }
                }; Spacer() }.padding(12)

                // zone shape (marching ants when idle)
                TimelineView(.animation) { tl in
                    let phase = drawing ? 0 : -tl.date.timeIntervalSinceReferenceDate * 20
                    Canvas { ctx, _ in
                        let path = zonePath(polygon, w: w, h: h)
                        ctx.fill(path, with: .color(NW.rose.opacity(0.14)))
                        ctx.stroke(path, with: .color(NW.rose), style: StrokeStyle(lineWidth: 1.6, lineJoin: .round, dash: drawing ? [] : [5, 4], dashPhase: phase))
                    }
                }
                if mode == 1 {
                    ForEach(0..<corners.count, id: \.self) { i in
                        Circle().fill(.white).overlay(Circle().stroke(NW.rose, lineWidth: 3)).frame(width: 15, height: 15)
                            .position(x: corners[i].x * w, y: corners[i].y * h)
                            .gesture(DragGesture(minimumDistance: 0).onChanged { g in
                                corners[i] = CGPoint(x: min(max(g.location.x / w, 0), 1), y: min(max(g.location.y / h, 0), 1))
                            })
                    }
                }
            }
            .contentShape(Rectangle())
            .gesture(mode == 0 ? DragGesture(minimumDistance: 0)
                .onChanged { g in
                    let p = CGPoint(x: min(max(g.location.x / w, 0), 1), y: min(max(g.location.y / h, 0), 1))
                    if !drawing { drawing = true; pts = [p] }
                    else if let l = pts.last, hypot(p.x - l.x, p.y - l.y) > 0.015 { pts.append(p) }
                }
                .onEnded { _ in drawing = false } : nil)
        }
        .frame(maxHeight: .infinity)
        .background(Color(hex: 0x0F0B09)).cornerRadius(20).clipped()
    }

    private func zonePath(_ p: [CGPoint], w: CGFloat, h: CGFloat) -> Path {
        var path = Path()
        let poly = mode == 2 ? [CGPoint(x: 0.03, y: 0.03), CGPoint(x: 0.97, y: 0.03), CGPoint(x: 0.97, y: 0.97), CGPoint(x: 0.03, y: 0.97)] : p
        guard poly.count >= 2 else { return path }
        path.move(to: CGPoint(x: poly[0].x * w, y: poly[0].y * h))
        for q in poly.dropFirst() { path.addLine(to: CGPoint(x: q.x * w, y: q.y * h)) }
        path.closeSubpath()
        return path
    }

    private func reset() {
        drawing = false
        pts = []
        corners = [CGPoint(x: 0.16, y: 0.16), CGPoint(x: 0.86, y: 0.13), CGPoint(x: 0.92, y: 0.80), CGPoint(x: 0.08, y: 0.84)]
    }
}
