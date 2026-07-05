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

// The parsed rule shown on the confirmation screen (design F1 safety net).
@interface NJParsedRule : NSObject
@property(nonatomic, copy) NSString *who;      // A person / A vehicle / ...
@property(nonatomic, copy) NSString *where;    // Backyard / Front door / ...
@property(nonatomic, copy) NSString *when;     // 10 PM – 6 AM / Anytime
@property(nonatomic, copy) NSString *then;     // Ping your phone + photo
@property(nonatomic, copy) NSString *trigger;  // "appears" | "loiter"
@property(nonatomic, copy) NSString *title;    // one-line rule card title
@end

@interface NightjarEngine : NSObject

// Compiles English into a structured rule ONCE, on-device. This app build uses
// a deterministic closed-vocab parser (the production path is the LLM-backed
// DecomposedRuleCompiler in the engine, loaded transiently at setup). The
// confirmation screen is the safety net either way.
+ (NJParsedRule *)compileRule:(NSString *)english;

// Set the watched zone as a normalized polygon (CGPoint values in [0,1]),
// before starting. Empty = whole frame. The engine rasterizes it to the gate's
// block grid so motion outside the zone never wakes the VLM.
- (void)setZonePolygon:(NSArray<NSValue *> *)normalizedPoints;

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
