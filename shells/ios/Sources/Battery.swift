import Foundation
#if os(iOS)
import UIKit
#elseif os(macOS)
import IOKit.ps
#endif

struct BatteryInfo {
    var percent: Int = -1            // -1 unknown
    var charging: Bool = false
    var systemHoursRemaining: Double? = nil  // OS estimate (macOS), whole machine
}

// Reads the device battery so the Monitor can project endurance, the honest
// "how much longer can it guard" number, not a synthetic figure.
enum Battery {
    static func read() -> BatteryInfo {
        #if os(iOS)
        UIDevice.current.isBatteryMonitoringEnabled = true
        let lvl = UIDevice.current.batteryLevel
        let st = UIDevice.current.batteryState
        return BatteryInfo(percent: lvl < 0 ? -1 : Int((lvl * 100).rounded()),
                           charging: st == .charging || st == .full, systemHoursRemaining: nil)
        #elseif os(macOS)
        guard let snap = IOPSCopyPowerSourcesInfo()?.takeRetainedValue(),
              let list = IOPSCopyPowerSourcesList(snap)?.takeRetainedValue() as? [CFTypeRef],
              let ps = list.first,
              let desc = IOPSGetPowerSourceDescription(snap, ps)?.takeUnretainedValue() as? [String: Any] else {
            return BatteryInfo()
        }
        let cur = desc[kIOPSCurrentCapacityKey as String] as? Int ?? -1
        let mx = desc[kIOPSMaxCapacityKey as String] as? Int ?? 100
        let charging = (desc[kIOPSPowerSourceStateKey as String] as? String) == (kIOPSACPowerValue as String)
        let tte = desc[kIOPSTimeToEmptyKey as String] as? Int
        let pct = mx > 0 && cur >= 0 ? Int((Double(cur) / Double(mx) * 100).rounded()) : -1
        let hrs = (tte != nil && tte! > 0) ? Double(tte!) / 60.0 : nil
        return BatteryInfo(percent: pct, charging: charging, systemHoursRemaining: hrs)
        #else
        return BatteryInfo()
        #endif
    }
}
