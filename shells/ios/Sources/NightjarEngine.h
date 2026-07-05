#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>

// Boundary between the SwiftUI/AppKit shell and the portable C++ engine. All
// logic stays in C++ (design rule: 0% logic in Swift). Two ways to feed it:
//   • camera mode  — the shell pushes real camera frames via submitPixelBuffer:;
//                    the engine runs the whole pipeline and reports stats/alerts.
//   • synthetic    — the engine runs its own 30fps loop over a procedural scene
//                    (for the Simulator, which has no camera) and streams frames.
NS_ASSUME_NONNULL_BEGIN

typedef struct {
    int framesProcessed;   // cumulative frames captured
    int framesGated;       // cumulative frames that PASSED the gate (motion)
    int vlmChecks;
    int conflationDrops;
    double eventToAlertMs;
    double gateMs;
    double gateP99Ms;
    BOOL motion;
    CGRect motionRect;  // normalized [0,1]
} NJStats;

// The parsed rule shown on the confirmation screen (design F1 safety net) — the
// full compiled rule, including the numeric time window so the engine can
// enforce "after 10 pm" for real.
@interface NJParsedRule : NSObject
@property(nonatomic, copy) NSString *who;         // A person / A vehicle / ...
@property(nonatomic, copy) NSString *where;       // Backyard / Front door / ...
@property(nonatomic, copy) NSString *when;        // 10 PM – 6 AM / Anytime (display)
@property(nonatomic, copy) NSString *action;      // Notify / Notify + photo (display)
@property(nonatomic, copy) NSString *trigger;     // "appears" | "loiter"
@property(nonatomic, copy) NSString *subjectKey;  // "person" | "vehicle" | "animal" | "package"
@property(nonatomic, copy) NSString *title;       // one-line rule card title
@property(nonatomic) int startMinute;             // time window start (minutes since midnight)
@property(nonatomic) int endMinute;               // == start => always active
@end

// One armed rule handed to the engine. Carries what the user actually typed /
// chose so the alert reflects the real rule (not a fabricated one).
@interface NJRuleSpec : NSObject
@property(nonatomic, copy) NSString *ruleId;      // stable id (maps alerts back to the rule)
@property(nonatomic, copy) NSString *rawText;     // the user's English — becomes the alert phrase
@property(nonatomic, copy) NSString *subjectKey;
@property(nonatomic, copy) NSString *trigger;
@property(nonatomic, copy) NSString *zoneLabel;   // e.g. "Backyard"
@property(nonatomic) int startMinute;
@property(nonatomic) int endMinute;
@end

@interface NightjarEngine : NSObject

// Compiles English into a structured rule ONCE, on-device (deterministic
// closed-vocab parser here; the LLM-backed DecomposedRuleCompiler is the
// production path). The confirmation screen is the safety net.
+ (NJParsedRule *)compileRule:(NSString *)english;

// Which Tier-2 is active after starting: "SmolVLM-500M (INT4)" or "scripted".
- (NSString *)tier2Name;

// The watched zone as a normalized polygon (CGPoint in [0,1]); empty = whole
// frame. Set before starting.
- (void)setZonePolygon:(NSArray<NSValue *> *)normalizedPoints;

// The armed rules. Set before starting. The engine watches all of them at once
// (union of subjects → one inference/candidate) and each alert reports which
// rule fired. onAlert: (one-liner, ruleId, subject).
- (void)setRules:(NSArray<NJRuleSpec *> *)rules;

// Real camera: the shell owns AVCaptureSession and pushes each frame here.
- (void)startCameraOnStats:(void (^)(NJStats stats))onStats
                   onAlert:(void (^)(NSString *oneLiner, NSString *ruleId, NSString *subject))onAlert;
- (void)submitPixelBuffer:(CVPixelBufferRef)pixelBuffer;

// A grayscale snapshot of the most recent processed frame (alert photo). Caller
// releases. nil until the first frame.
- (nullable CGImageRef)currentSnapshotCopy CF_RETURNS_RETAINED;

// The last ~2s of processed frames (subsampled) — the short event clip.
- (NSArray *)recentClipFrames;

// Simulator fallback: engine generates + streams frames itself.
- (void)startSyntheticOnFrame:(void (^)(CGImageRef frame, NJStats stats))onFrame
                      onAlert:(void (^)(NSString *oneLiner, NSString *ruleId, NSString *subject))onAlert;

- (void)stop;

@end

NS_ASSUME_NONNULL_END
