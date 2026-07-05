import SwiftUI

// Otto the owl, the exact SVG geometry from the Nightwatch prototype (viewBox
// 0 0 120 120), redrawn as a Canvas so it scales crisp. Eyes blink and the
// pupils scan slowly; in alert mode the pupils widen and stop scanning.
struct OttoOwl: View {
    var size: CGFloat = 118
    var alert: Bool = false
    var bodyColor: Color = NW.owlBody
    var showBelly: Bool = true

    var body: some View {
        TimelineView(.animation) { tl in
            let t = tl.date.timeIntervalSinceReferenceDate
            Canvas { ctx, sz in
                let s = sz.width / 120.0
                func P(_ x: Double, _ y: Double) -> CGPoint { CGPoint(x: x * s, y: y * s) }
                func disc(_ cx: Double, _ cy: Double, _ r: Double) -> Path {
                    Path(ellipseIn: CGRect(x: (cx - r) * s, y: (cy - r) * s, width: 2 * r * s, height: 2 * r * s))
                }
                func poly(_ pts: [(Double, Double)]) -> Path {
                    var p = Path(); p.move(to: P(pts[0].0, pts[0].1))
                    for q in pts.dropFirst() { p.addLine(to: P(q.0, q.1)) }
                    p.closeSubpath(); return p
                }

                ctx.fill(poly([(30, 30), (42, 12), (50, 32)]), with: .color(bodyColor))
                ctx.fill(poly([(90, 30), (78, 12), (70, 32)]), with: .color(bodyColor))
                ctx.fill(Path(ellipseIn: CGRect(x: (60 - 40) * s, y: (70 - 42) * s, width: 80 * s, height: 84 * s)),
                         with: .color(bodyColor))
                if showBelly {
                    ctx.fill(Path(ellipseIn: CGRect(x: (60 - 25) * s, y: (86 - 22) * s, width: 50 * s, height: 44 * s)),
                             with: .color(NW.owlBelly))
                }

                // eyes (blink: brief vertical squish ~ every 4.6s)
                let blinkPhase = t.truncatingRemainder(dividingBy: 4.6) / 4.6
                let blink = (blinkPhase > 0.93 && blinkPhase < 0.97) ? 0.08 : 1.0
                let scan = alert ? 0.0 : sin(t * 0.9) * 3.5
                let pr = alert ? 9.5 : 7.0
                for cxDouble in [42.0, 78.0] {
                    var eye = ctx
                    let cy = 58.0
                    eye.translateBy(x: cxDouble * s, y: cy * s)
                    eye.scaleBy(x: 1, y: blink)
                    eye.translateBy(x: -cxDouble * s, y: -cy * s)
                    eye.fill(disc(cxDouble, cy, 16), with: .color(NW.creamDim))
                    eye.fill(disc(cxDouble + 2 + scan, 60, pr), with: .color(Color(hex: 0x17100A)))
                    eye.fill(disc(cxDouble + 4.5 + scan, 57.5, 2), with: .color(.white))
                }

                ctx.fill(poly([(60, 70), (53, 77), (60, 85), (67, 77)]), with: .color(NW.rose))
            }
        }
        .frame(width: size, height: size)
    }
}
