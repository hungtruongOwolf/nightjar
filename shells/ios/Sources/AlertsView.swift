import SwiftUI

// One captured alert: the photo that triggered it + what fired, when.
struct AlertRecord: Identifiable {
    let id = UUID()
    let text: String
    let image: CGImage?
    var frames: [CGImage] = []   // short event clip (last ~2s)
    let date: Date
}

// The review screen — tap an alert to see the frame Otto flagged. This is the
// app-side view of the evidence; the engine's EventClipStore keeps the fuller
// pre/post-roll clip on disk (differential-coded).
struct AlertsView: View {
    let alerts: [AlertRecord]
    @Environment(\.dismiss) private var dismiss
    @State private var selected: AlertRecord?

    var body: some View {
        ZStack {
            NW.ink.ignoresSafeArea()
            VStack(alignment: .leading, spacing: 0) {
                HStack {
                    VStack(alignment: .leading, spacing: 3) {
                        MonoLabel(text: "CAPTURED · ON-DEVICE ONLY", size: 10, opacity: 0.5)
                        (Text("The ").foregroundColor(NW.cream) + Text("evidence").foregroundColor(NW.rose).italic() + Text(".").foregroundColor(NW.cream)).font(NW.serif(28))
                    }
                    Spacer()
                    Button { dismiss() } label: {
                        Image(systemName: "xmark").font(.system(size: 14, weight: .bold)).foregroundColor(NW.muted(0.6))
                            .padding(10).background(NW.card).clipShape(Circle())
                    }
                }.padding(.horizontal, 20).padding(.top, 8).padding(.bottom, 12)

                if alerts.isEmpty {
                    Spacer()
                    VStack(spacing: 10) {
                        OttoOwl(size: 80)
                        Text("No alerts yet — all quiet.").font(NW.serif(17, italic: true)).foregroundColor(NW.muted(0.55))
                    }.frame(maxWidth: .infinity)
                    Spacer()
                } else {
                    ScrollView {
                        LazyVGrid(columns: [GridItem(.flexible(), spacing: 12), GridItem(.flexible(), spacing: 12)], spacing: 12) {
                            ForEach(alerts) { a in
                                Button { selected = a } label: { card(a) }
                            }
                        }.padding(.horizontal, 20).padding(.bottom, 30)
                    }
                }
            }
        }
        .buttonStyle(.plain)
        .preferredColorScheme(.dark)
        .sheet(item: $selected) { a in DetailView(alert: a) }
    }

    private func card(_ a: AlertRecord) -> some View {
        VStack(alignment: .leading, spacing: 0) {
            ZStack(alignment: .topTrailing) {
                thumb(a.image ?? a.frames.first).frame(height: 120).frame(maxWidth: .infinity).clipped()
                if a.frames.count > 1 {
                    Text("▶ CLIP").font(NW.mono(8)).foregroundColor(.white)
                        .padding(.horizontal, 6).padding(.vertical, 3).background(NW.rose).clipShape(Capsule()).padding(6)
                }
            }
            VStack(alignment: .leading, spacing: 3) {
                Text(a.text).font(.system(size: 11.5, weight: .medium)).foregroundColor(NW.creamDim).lineLimit(2)
                Text(Self.fmt.string(from: a.date)).font(NW.mono(9)).foregroundColor(NW.muted(0.45))
            }.padding(10).frame(maxWidth: .infinity, alignment: .leading)
        }
        .background(NW.card).cornerRadius(14)
        .overlay(RoundedRectangle(cornerRadius: 14).stroke(NW.rose.opacity(0.25)))
    }

    @ViewBuilder private func thumb(_ img: CGImage?) -> some View {
        if let img { Image(decorative: img, scale: 1).resizable().aspectRatio(contentMode: .fill) }
        else { NW.guardBg.overlay(Image(systemName: "photo").foregroundColor(NW.muted(0.3))) }
    }

    static let fmt: DateFormatter = { let f = DateFormatter(); f.dateFormat = "MMM d · HH:mm:ss"; return f }()
}

private struct DetailView: View {
    let alert: AlertRecord
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 16) {
                if alert.frames.count > 1 {
                    // play the event clip as a looping flipbook (~12 fps)
                    TimelineView(.animation(minimumInterval: 1.0 / 12.0)) { tl in
                        let i = Int(tl.date.timeIntervalSinceReferenceDate * 12) % alert.frames.count
                        Image(decorative: alert.frames[i], scale: 1).resizable().aspectRatio(contentMode: .fit).cornerRadius(12)
                    }
                } else if let img = alert.image {
                    Image(decorative: img, scale: 1).resizable().aspectRatio(contentMode: .fit).cornerRadius(12)
                }
                VStack(spacing: 6) {
                    Text(alert.text).font(.system(size: 15)).foregroundColor(NW.creamDim).multilineTextAlignment(.center)
                    Text(AlertsView.fmt.string(from: alert.date)).font(NW.mono(11)).foregroundColor(NW.muted(0.5))
                    MonoLabel(text: alert.frames.count > 1 ? "CLIP · \(alert.frames.count) FRAMES · ON-DEVICE" : "STAYED ON THIS DEVICE", size: 9, opacity: 0.4)
                }
                Button { dismiss() } label: {
                    Text("Close").font(.system(size: 15, weight: .semibold)).foregroundColor(.white)
                        .padding(.horizontal, 40).padding(.vertical, 13).background(NW.rose).cornerRadius(14)
                }.buttonStyle(.plain)
            }.padding(24)
        }.preferredColorScheme(.dark)
    }
}
