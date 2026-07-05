#import <Foundation/Foundation.h>

// Thin Objective-C++ boundary between the SwiftUI shell and the portable C++
// engine. All business logic stays in C++ (design rule: 0% logic in Swift);
// this only marshals a request in and results out.
NS_ASSUME_NONNULL_BEGIN

@interface NightjarResult : NSObject
@property(nonatomic, copy) NSString *alert;   // fired alert one-liner, or "" if none
@property(nonatomic) int framesProcessed;
@property(nonatomic) int vlmInferences;
@property(nonatomic) int conflationDrops;
@property(nonatomic) double gateP50us;         // Tier-1 gate cost, microseconds
@property(nonatomic) double e2eP50ms;          // event -> alert, milliseconds
@property(nonatomic, copy) NSString *isa;      // Arm ISA flags on this device
@end

@interface NightjarBridge : NSObject
// Runs the full engine pipeline on a synthetic replayed clip (a person appears
// and lingers), with the given trigger ("appears" or "loiter"), using the
// deterministic scripted VLM — no model, runs on the Simulator. Returns what
// the engine decided plus telemetry.
+ (NightjarResult *)runGuardDemoWithTrigger:(NSString *)trigger;
@end

NS_ASSUME_NONNULL_END
