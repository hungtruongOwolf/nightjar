import SwiftUI

// Palette + type lifted from the Nightwatch prototype (sentry-source). Instrument
// Serif / IBM Plex Mono aren't system fonts on iOS, so we use the closest system
// designs (serif italic / monospaced) — same feel, no bundled font files.
extension Color {
    init(hex: UInt32) {
        self.init(.sRGB,
                  red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255)
    }
}

enum NW {
    static let rose = Color(hex: 0xF43F5E)
    static let ink = Color(hex: 0x0D0A08)
    static let screen = Color(hex: 0x161009)
    static let guardBg = Color(hex: 0x0A0705)
    static let cream = Color(hex: 0xF8F1EA)
    static let creamDim = Color(hex: 0xF6EFE8)
    static let owlBody = Color(hex: 0x2E241D)
    static let owlBelly = Color(hex: 0x3B2D22)
    static let bubble = Color(hex: 0x241B16)
    static let card = Color(hex: 0x211914)
    static let cardAlt = Color(hex: 0x1B1410)
    static let pill = Color(hex: 0x171009)
    static let green = Color(hex: 0x4ADE80)

    static func muted(_ o: Double) -> Color { creamDim.opacity(o) }

    // Serif display (Instrument Serif stand-in)
    static func serif(_ size: CGFloat, italic: Bool = false) -> Font {
        let f = Font.system(size: size, weight: .regular, design: .serif)
        return italic ? f.italic() : f
    }
    // Mono label (IBM Plex Mono stand-in)
    static func mono(_ size: CGFloat, weight: Font.Weight = .semibold) -> Font {
        .system(size: size, weight: weight, design: .monospaced)
    }
}

// A tracked monospaced caption, the recurring label style in the prototype.
struct MonoLabel: View {
    let text: String
    var size: CGFloat = 9.5
    var opacity: Double = 0.45
    var body: some View {
        Text(text).font(NW.mono(size)).tracking(2).foregroundColor(NW.muted(opacity))
    }
}
