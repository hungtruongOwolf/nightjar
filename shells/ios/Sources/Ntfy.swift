import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

// Optional real push to your phone via ntfy.sh, the only thing that ever leaves
// the device, and only when you set a topic. Pure URLSession (no engine/network
// dependency); fire-and-forget so a failed/absent network never affects the
// guard. Install the free "ntfy" app, subscribe to your topic, and alerts buzz
// your phone with the crop attached.
enum Ntfy {
    static func send(topic: String, title: String, message: String, image: CGImage?) {
        let t = topic.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !t.isEmpty, let url = URL(string: "https://ntfy.sh/\(t)") else { return }
        var req = URLRequest(url: url)
        req.timeoutInterval = 8
        req.setValue(asciiSafe(title), forHTTPHeaderField: "X-Title")
        req.setValue("view, Open, https://ntfy.sh/\(t)", forHTTPHeaderField: "X-Actions")

        if let img = image, let jpeg = jpegData(img) {
            req.httpMethod = "PUT"                          // attach the crop
            req.setValue("nightjar.jpg", forHTTPHeaderField: "X-Filename")
            req.setValue(asciiSafe(message), forHTTPHeaderField: "X-Message")
            req.httpBody = jpeg
        } else {
            req.httpMethod = "POST"
            req.httpBody = message.data(using: .utf8)       // body allows full UTF-8
        }
        URLSession.shared.dataTask(with: req).resume()      // fire-and-forget; errors ignored
    }

    // HTTP headers must be ASCII; the rule phrase can carry an en-dash etc.
    private static func asciiSafe(_ s: String) -> String {
        String(s.unicodeScalars.map { $0.isASCII ? Character($0) : "-" })
    }

    private static func jpegData(_ img: CGImage) -> Data? {
        let out = NSMutableData()
        guard let dest = CGImageDestinationCreateWithData(out, UTType.jpeg.identifier as CFString, 1, nil) else { return nil }
        CGImageDestinationAddImage(dest, img, [kCGImageDestinationLossyCompressionQuality: 0.6] as CFDictionary)
        guard CGImageDestinationFinalize(dest) else { return nil }
        return out as Data
    }
}
