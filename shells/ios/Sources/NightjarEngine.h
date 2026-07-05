#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// Live streaming boundary between the SwiftUI shell and the portable C++ engine.
// All logic stays in C++ (design rule: 0% logic in Swift). This starts a
// real-time loop that drives frames through the WHOLE pipeline (motion gate ->
// best-frame -> VLM -> temporal rule engine -> alert) at ~30 fps and streams
// each processed frame, live stats, and any alert back to the UI — the same
// path the AVFoundation camera takes on a real device. On the Simulator the
// frames come from a procedural night scene instead of the camera.
NS_ASSUME_NONNULL_BEGIN

typedef struct {
    int framesProcessed;
    int framesSkippedPct;  // % of frames the gate discarded (VLM never woke)
    int vlmChecks;         // Tier-2 inferences run
    int conflationDrops;
    double eventToAlertMs;  // p50 event->alert (0 until first alert path completes)
    double gateMs;          // p50 Tier-1 gate cost
    BOOL motion;            // gate fired on the latest frame
    CGRect motionRect;      // normalized [0,1] bbox of the motion blob (.zero if none)
} NJStats;

@interface NightjarEngine : NSObject

// trigger: "appears" (rising edge) or "loiter" (sustained dwell).
// onFrame is called on the main queue ~30x/sec with the current grayscale frame
// (caller must CGImageRelease) and live stats. onAlert fires the moment the
// temporal rule fires.
- (void)startWithTrigger:(NSString *)trigger
                 onFrame:(void (^)(CGImageRef frame, NJStats stats))onFrame
                 onAlert:(void (^)(NSString *oneLiner))onAlert;
- (void)stop;

@end

NS_ASSUME_NONNULL_END
