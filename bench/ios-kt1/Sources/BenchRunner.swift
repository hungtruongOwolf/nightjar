import Foundation

/// Owns the bench state for the UI. The mtmd inference engine lands in a
/// follow-up commit; this scaffold only wires UI + logging so the app is
/// runnable (and KT6 readable) on its own.
@MainActor
final class BenchRunner: ObservableObject {
    @Published var running = false
    @Published var log = ""

    func start() {
        running = true
        log = "bench engine not wired yet — scaffold commit\n"
        log += "models expected in app bundle: Resources/models/*.gguf\n"
        log += "images expected in app bundle: Resources/images/*.jpg\n"
        running = false
    }

    func append(_ line: String) {
        log += line + "\n"
    }
}
