#include "nightjar/temporal_rule_engine.h"

#include <string>

#include "check.h"

using namespace nightjar;

namespace {

Observation obs(int64_t t, std::map<std::string, bool> preds, const std::string& zone = "any") {
    Observation o;
    o.predicates = std::move(preds);
    o.zone = zone;
    o.unix_s = t;
    o.minute_of_day = 23 * 60;  // 23:00, inside typical night windows
    return o;
}

TemporalRule rule(const std::string& id, const std::string& pred, Trigger trig) {
    TemporalRule r;
    r.id = id;
    r.predicate = pred;
    r.trigger = trig;
    r.zone_id = "any";
    r.time_window = TimeWindow{0, 0};  // always
    r.cooldown_s = 120;
    r.actions = {Action{ActionType::Ntfy, "t"}};
    return r;
}

// ---- Appears: rising edge only ----

void test_appears_fires_on_rising_edge_only() {
    TemporalRuleEngine e;
    e.set_rules({rule("r", "person", Trigger::Appears)});
    CHECK(e.observe(obs(0, {{"person", false}})).empty());
    CHECK_EQ(e.observe(obs(1, {{"person", true}})).size(), size_t(1));  // edge -> fire
    CHECK(e.observe(obs(2, {{"person", true}})).empty());               // still true -> no refire
    CHECK(e.observe(obs(3, {{"person", false}})).empty());
    CHECK_EQ(e.observe(obs(300, {{"person", true}})).size(), size_t(1)); // new edge (past cooldown)
}

// ---- Sustained: loitering ----

void test_sustained_fires_after_dwell() {
    TemporalRuleEngine e;
    auto r = rule("loiter", "person", Trigger::Sustained);
    r.dwell_s = 60;
    e.set_rules({r});

    CHECK(e.observe(obs(0, {{"person", true}})).empty());    // just arrived
    CHECK(e.observe(obs(30, {{"person", true}})).empty());   // 30s < 60
    CHECK(e.observe(obs(59, {{"person", true}})).empty());   // 59s < 60
    CHECK_EQ(e.observe(obs(60, {{"person", true}})).size(), size_t(1));  // 60s -> loitering
}

void test_sustained_resets_when_presence_breaks() {
    TemporalRuleEngine e;
    auto r = rule("loiter", "person", Trigger::Sustained);
    r.dwell_s = 60;
    e.set_rules({r});
    e.observe(obs(0, {{"person", true}}));
    e.observe(obs(40, {{"person", false}}));               // gone -> clock resets
    CHECK(e.observe(obs(70, {{"person", true}})).empty());  // only 0s of the new stay
    CHECK(e.observe(obs(90, {{"person", true}})).empty());  // 20s
    CHECK_EQ(e.observe(obs(130, {{"person", true}})).size(), size_t(1));  // 60s since t=70
}

// ---- LeftBehind: package dropped, person leaves ----

void test_left_behind_fires_when_actor_leaves_object() {
    TemporalRuleEngine e;
    auto r = rule("left", "package", Trigger::LeftBehind);
    r.actor_predicate = "person";
    e.set_rules({r});

    // Person arrives with a package.
    CHECK(e.observe(obs(0, {{"package", true}, {"person", true}})).empty());
    // Package still there, person still there -> not yet.
    CHECK(e.observe(obs(5, {{"package", true}, {"person", true}})).empty());
    // Person leaves, package remains -> LEFT BEHIND.
    CHECK_EQ(e.observe(obs(10, {{"package", true}, {"person", false}})).size(), size_t(1));
}

void test_left_behind_no_fire_if_person_stays() {
    TemporalRuleEngine e;
    auto r = rule("left", "package", Trigger::LeftBehind);
    e.set_rules({r});
    e.observe(obs(0, {{"package", true}, {"person", true}}));
    CHECK(e.observe(obs(10, {{"package", true}, {"person", true}})).empty());  // person stays
}

void test_left_behind_no_fire_if_object_also_gone() {
    TemporalRuleEngine e;
    auto r = rule("left", "package", Trigger::LeftBehind);
    e.set_rules({r});
    e.observe(obs(0, {{"package", true}, {"person", true}}));
    // Person picks the package back up and leaves, nothing left behind.
    CHECK(e.observe(obs(10, {{"package", false}, {"person", false}})).empty());
}

// ---- gating ----

// The demo-critical distinction: placing vs taking look identical in one frame,
// but the object's presence trajectory differs. Same person+object frames, only
// the object's before/after presence flips the outcome.
void test_place_vs_take_distinguished() {
    // TAKE (theft): object present, person arrives, person leaves, object GONE.
    TemporalRuleEngine take;
    auto rt = rule("theft", "package", Trigger::Removed);
    rt.actor_predicate = "person";
    take.set_rules({rt});
    take.observe(obs(0, {{"package", true}, {"person", false}}));  // object sitting there
    take.observe(obs(1, {{"package", true}, {"person", true}}));   // person arrives
    auto d = take.observe(obs(2, {{"package", false}, {"person", true}}));  // object GONE, person here
    CHECK_EQ(d.size(), size_t(1));  // theft detected

    // PLACE (delivery): object absent, person arrives WITH it, leaves, object STAYS.
    TemporalRuleEngine place;
    auto rp = rule("theft", "package", Trigger::Removed);  // same theft rule
    place.set_rules({rp});
    place.observe(obs(0, {{"package", false}, {"person", true}}));  // person arrives
    place.observe(obs(1, {{"package", true}, {"person", true}}));   // sets object down
    auto d2 = place.observe(obs(2, {{"package", true}, {"person", false}}));  // leaves, object STAYS
    CHECK(d2.empty());  // NOT theft, object still present
}

void test_zone_and_cooldown() {
    TemporalRuleEngine e;
    auto r = rule("r", "person", Trigger::Appears);
    r.zone_id = "backyard";
    e.set_rules({r});
    CHECK(e.observe(obs(0, {{"person", true}}, "driveway")).empty());     // wrong zone
    CHECK_EQ(e.observe(obs(1, {{"person", false}}, "backyard")).size(), size_t(0));
    CHECK_EQ(e.observe(obs(2, {{"person", true}}, "backyard")).size(), size_t(1));  // edge in zone
}

void test_time_window_gates() {
    TemporalRuleEngine e;
    auto r = rule("r", "person", Trigger::Appears);
    r.time_window = TimeWindow{22 * 60, 6 * 60};  // overnight
    e.set_rules({r});
    Observation day = obs(0, {{"person", true}});
    day.minute_of_day = 12 * 60;  // noon edge, outside window, no alert
    CHECK(e.observe(day).empty());
    Observation gone = obs(1, {{"person", false}});
    gone.minute_of_day = 20 * 60;  // person leaves (arms the next edge)
    e.observe(gone);
    Observation night = obs(2, {{"person", true}});
    night.minute_of_day = 23 * 60;  // new edge, inside window -> fires
    CHECK_EQ(e.observe(night).size(), size_t(1));
}

}  // namespace

int main() {
    test_appears_fires_on_rising_edge_only();
    test_sustained_fires_after_dwell();
    test_sustained_resets_when_presence_breaks();
    test_left_behind_fires_when_actor_leaves_object();
    test_left_behind_no_fire_if_person_stays();
    test_left_behind_no_fire_if_object_also_gone();
    test_place_vs_take_distinguished();
    test_zone_and_cooldown();
    test_time_window_gates();
    return njtest::failures() == 0 ? 0 : 1;
}
