import SwiftUI

// The iOS shell is intentionally thin (design rule: 0% business logic in
// Swift). It presents rules and results; every decision is made by the portable
// C++ engine behind NightjarBridge.
@main
struct NightjarApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
                .preferredColorScheme(.dark)
        }
    }
}
