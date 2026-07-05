import AVFoundation
import SwiftUI
#if os(iOS)
import UIKit
#elseif os(macOS)
import AppKit
#endif

// Owns the real camera. Delivers luma frames straight to the C++ engine, the
// only place platform capture touches the engine. Cross-platform: same code
// runs on iOS (device) and macOS (webcam).
final class CameraCapture: NSObject, ObservableObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    let session = AVCaptureSession()
    @Published var running = false
    private let engine: NightjarEngine
    private let queue = DispatchQueue(label: "nightjar.camera")
    private var configured = false  // configure the session once; re-entry just re-runs it

    init(engine: NightjarEngine) { self.engine = engine; super.init() }

    static var hasCamera: Bool { AVCaptureDevice.default(for: .video) != nil }

    func start() {
        AVCaptureDevice.requestAccess(for: .video) { [weak self] ok in
            guard ok, let self else { return }
            self.queue.async {
                if self.configured {
                    if !self.session.isRunning { self.session.startRunning() }
                    DispatchQueue.main.async { self.running = true }
                } else {
                    self.configure()
                }
            }
        }
    }

    private func configure() {
        session.beginConfiguration()
        if session.canSetSessionPreset(.vga640x480) { session.sessionPreset = .vga640x480 }
        if let dev = AVCaptureDevice.default(for: .video),
           let input = try? AVCaptureDeviceInput(device: dev), session.canAddInput(input) {
            session.addInput(input)
        }
        let out = AVCaptureVideoDataOutput()
        out.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]
        out.alwaysDiscardsLateVideoFrames = true
        out.setSampleBufferDelegate(self, queue: queue)
        if session.canAddOutput(out) { session.addOutput(out) }
        session.commitConfiguration()
        configured = true
        session.startRunning()
        DispatchQueue.main.async { self.running = true }
    }

    func stop() { queue.async { if self.session.isRunning { self.session.stopRunning() } } }

    func captureOutput(_ output: AVCaptureOutput, didOutput sampleBuffer: CMSampleBuffer, from connection: AVCaptureConnection) {
        guard let pb = CMSampleBufferGetImageBuffer(sampleBuffer) else { return }
        engine.submitPixelBuffer(pb)
    }
}

// AVCaptureVideoPreviewLayer wrapped for SwiftUI on both platforms.
#if os(iOS)
struct CameraPreview: UIViewRepresentable {
    let session: AVCaptureSession
    func makeUIView(context: Context) -> PreviewView { let v = PreviewView(); v.previewLayer.session = session; v.previewLayer.videoGravity = .resizeAspectFill; return v }
    func updateUIView(_ v: PreviewView, context: Context) {}
    final class PreviewView: UIView {
        override class var layerClass: AnyClass { AVCaptureVideoPreviewLayer.self }
        var previewLayer: AVCaptureVideoPreviewLayer { layer as! AVCaptureVideoPreviewLayer }
    }
}
#elseif os(macOS)
struct CameraPreview: NSViewRepresentable {
    let session: AVCaptureSession
    func makeNSView(context: Context) -> NSView {
        let v = NSView(); v.wantsLayer = true
        let pl = AVCaptureVideoPreviewLayer(session: session)
        pl.videoGravity = .resizeAspectFill; pl.frame = v.bounds; pl.autoresizingMask = [.layerWidthSizable, .layerHeightSizable]
        v.layer = CALayer(); v.layer?.addSublayer(pl)
        context.coordinator.layer = pl
        return v
    }
    func updateNSView(_ v: NSView, context: Context) {
        context.coordinator.layer?.frame = v.bounds
    }
    func makeCoordinator() -> Coordinator { Coordinator() }
    final class Coordinator { var layer: AVCaptureVideoPreviewLayer? }
}
#endif
