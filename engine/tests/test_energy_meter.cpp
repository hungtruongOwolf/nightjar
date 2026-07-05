#include "nightjar/energy_meter.h"

#include <cmath>

#include "check.h"

using namespace nightjar;

namespace {

bool approx(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

void test_net_energy_idle_delta() {
    EnergyMeter m;
    m.set_idle_baseline_w(2.0);
    // 10 samples of 5W for 1s each => gross 50J; idle 2W*10s=20J; net 30J.
    for (int i = 0; i < 10; ++i) m.add_sample(5.0, 1.0);
    CHECK(approx(m.wall_seconds(), 10.0));
    CHECK(approx(m.gross_joules(), 50.0));
    CHECK(approx(m.net_joules(), 30.0));
    CHECK(approx(m.avg_active_watts(), 3.0));  // 30J / 10s
}

void test_net_clamped_at_zero() {
    EnergyMeter m;
    m.set_idle_baseline_w(10.0);
    m.add_sample(3.0, 5.0);  // below idle
    CHECK(approx(m.net_joules(), 0.0));
}

void test_joules_per_inference() {
    EnergyMeter m;
    m.set_idle_baseline_w(0.0);
    m.add_sample(4.0, 10.0);  // 40J
    CHECK(approx(m.joules_per(20), 2.0));   // 40J / 20 inferences
    CHECK(approx(m.joules_per(0), 0.0));    // guard
}

void test_battery_projection() {
    EnergyMeter m;
    m.set_idle_baseline_w(0.0);
    m.add_sample(2.0, 100.0);  // avg active 2W
    // A 10 Wh battery at 2W => 5 hours continuous; 0.05 h per 1%.
    CHECK(approx(m.sustainable_hours(10.0), 5.0));
    CHECK(approx(m.hours_per_percent(10.0), 0.05));
}

void test_variable_dt() {
    EnergyMeter m;
    m.set_idle_baseline_w(1.0);
    m.add_sample(5.0, 0.2);  // 1.0J
    m.add_sample(3.0, 0.8);  // 2.4J ; gross 3.4J, idle 1W*1s=1J, net 2.4J
    CHECK(approx(m.wall_seconds(), 1.0));
    CHECK(approx(m.gross_joules(), 3.4));
    CHECK(approx(m.net_joules(), 2.4));
}

}  // namespace

int main() {
    test_net_energy_idle_delta();
    test_net_clamped_at_zero();
    test_joules_per_inference();
    test_battery_projection();
    test_variable_dt();
    return njtest::failures() == 0 ? 0 : 1;
}
