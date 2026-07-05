#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

// Boundary between the SwiftUI/AppKit shell and the portable C++ engine. All
// logic stays in C++ (design rule: 0% logic in Swift). Two ways to feed it:
//   • camera mode  — the shell pushes real camera frames via submitPixelBuffer:;
//                    the engine runs the whole pipeline and reports stats/alerts.
//   • synthetic    — the engine runs its own 30fps loop over a procedural night
//                    scene (for the Simulator, which has no camera) and also
//                    streams the frame image back for display.
NS_ASSUME_NONNULL_BEGIN

typedef struct {
    int framesProcessed;
    int framesSkippedPct;
    int vlmChecks;
    int conflationDrops;
    double eventToAlertMs;
    double gateMs;
    double gateP99Ms;
    BOOL motion;
    CGRect motionRect;  // normalized [0,1]
} NJStats;

@interface NightjarEngine : NSObject

// Real camera: the shell owns AVCaptureSession and pushes each frame here.
- (void)startCameraWithTrigger:(NSString *)trigger
                       onStats:(void (^)(NJStats stats))onStats
                       onAlert:(void (^)(NSString *oneLiner))onAlert;
- (void)submitPixelBuffer:(CVPixelBufferRef)pixelBuffer;

// Simulator fallback: engine generates + streams frames itself.
- (void)startSyntheticWithTrigger:(NSString *)trigger
                          onFrame:(void (^)(CGImageRef frame, NJStats stats))onFrame
                          onAlert:(void (^)(NSString *oneLiner))onAlert;

- (void)stop;

@end

NS_ASSUME_NONNULL_END
