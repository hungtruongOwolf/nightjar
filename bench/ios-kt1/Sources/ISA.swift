import Foundation

/// KT6 — dump the Arm ISA feature flags of the device this runs on.
/// Every published number must carry this line (measurement discipline).
enum ISA {
    static func flag(_ name: String) -> String {
        var value: Int32 = 0
        var size = MemoryLayout<Int32>.size
        let ok = sysctlbyname(name, &value, &size, nil, 0) == 0
        return ok ? String(value) : "n/a"
    }

    static var summary: String {
        let flags = [
            "hw.optional.arm.FEAT_DotProd",
            "hw.optional.arm.FEAT_I8MM",
            "hw.optional.arm.FEAT_SME",
            "hw.optional.arm.FEAT_SME2",
            "hw.optional.neon",
        ]
        var lines = flags.map { "\($0) = \(flag($0))" }

        var model = [CChar](repeating: 0, count: 64)
        var size = model.count
        if sysctlbyname("hw.machine", &model, &size, nil, 0) == 0 {
            lines.insert("device: \(String(cString: model))", at: 0)
        }
        return lines.joined(separator: "\n")
    }

    /// Jetsam-relevant headroom, sampled on demand (bytes the OS says we may still use).
    static var availableMemoryMB: Int {
        Int(os_proc_available_memory() / (1024 * 1024))
    }
}
