#include "nightjar/energy_meter.h"

#include <algorithm>

namespace nightjar {

void EnergyMeter::add_sample(double watts, double dt_s) {
    if (dt_s <= 0) return;
    gross_j_ += watts * dt_s;
    wall_s_ += dt_s;
}

double EnergyMeter::net_joules() const {
    const double net = gross_j_ - idle_w_ * wall_s_;
    return std::max(0.0, net);
}

double EnergyMeter::avg_active_watts() const {
    return wall_s_ > 0 ? net_joules() / wall_s_ : 0.0;
}

double EnergyMeter::joules_per(uint64_t count) const {
    return count > 0 ? net_joules() / static_cast<double>(count) : 0.0;
}

double EnergyMeter::sustainable_hours(double battery_wh) const {
    const double p = avg_active_watts();
    if (p <= 0) return 0.0;
    return (battery_wh) / p;  // Wh / W = hours
}

double EnergyMeter::hours_per_percent(double battery_wh) const {
    return sustainable_hours(battery_wh) / 100.0;
}

}  // namespace nightjar
