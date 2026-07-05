#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nightjar {

// Turns a power trace into the perf-per-watt numbers that are Nightjar's Tier-2
// Arm story: net energy over a run (idle-delta method, the published standard),
// Joules per VLM inference, and a projection of how long a phone battery would
// sustain the workload. Power samples come from the platform (powermetrics on
// Mac, battery deltas on iPhone); this module stays portable and is unit-tested
// with synthetic traces.
class EnergyMeter {
public:
    // Idle baseline (package watts with the app quiescent) is subtracted so we
    // report the energy attributable to Nightjar's work, not the whole system.
    void set_idle_baseline_w(double watts) { idle_w_ = watts; }

    // One power reading `watts` sustained for `dt_s` seconds.
    void add_sample(double watts, double dt_s);

    double wall_seconds() const { return wall_s_; }
    double gross_joules() const { return gross_j_; }
    // Net of the idle baseline, clamped at 0 (never report negative energy).
    double net_joules() const;
    double avg_active_watts() const;  // net_joules / wall_seconds

    // Energy per unit of work (e.g. per VLM inference or per alert).
    double joules_per(uint64_t count) const;

    // Hours the workload could run continuously on a battery of `battery_wh`
    // watt-hours at the measured average active power. Also the intuitive
    // "hours per 1% battery" = that / 100.
    double sustainable_hours(double battery_wh) const;
    double hours_per_percent(double battery_wh) const;

private:
    double idle_w_ = 0.0;
    double wall_s_ = 0.0;
    double gross_j_ = 0.0;
};

}  // namespace nightjar
