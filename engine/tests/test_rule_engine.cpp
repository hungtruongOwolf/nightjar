#include "nightjar/rule_engine.h"

#include <string>

#include "check.h"

using namespace nightjar;

namespace {

Rule make_rule(const std::string& id, Subject subj, const std::string& zone, int start, int end,
               int cooldown = 120) {
    Rule r;
    r.id = id;
    r.subject = subj;
    r.zone_id = zone;
    r.time_window = TimeWindow{start, end};
    r.cooldown_s = cooldown;
    r.actions = {Action{ActionType::Ntfy, "topic-" + id}};
    return r;
}

Facts person() {
    Facts f;
    f.person = true;
    return f;
}

// ---- time windows ----

void test_time_window_daytime() {
    TimeWindow w{9 * 60, 18 * 60};  // 09:00-18:00
    CHECK(!w.contains(8 * 60));
    CHECK(w.contains(9 * 60));
    CHECK(w.contains(12 * 60));
    CHECK(!w.contains(18 * 60));  // half-open
}

void test_time_window_overnight() {
    TimeWindow w{22 * 60, 6 * 60};  // 22:00-06:00 wraps midnight
    CHECK(w.contains(23 * 60));
    CHECK(w.contains(0));
    CHECK(w.contains(5 * 60));
    CHECK(!w.contains(12 * 60));
    CHECK(!w.contains(6 * 60));
}

void test_time_window_always() {
    TimeWindow w{0, 0};  // start==end => always
    CHECK(w.contains(0));
    CHECK(w.contains(13 * 60));
}

void test_parse_hhmm() {
    CHECK_EQ(parse_hhmm("22:00"), 22 * 60);
    CHECK_EQ(parse_hhmm("06:30"), 6 * 60 + 30);
    CHECK_EQ(parse_hhmm("00:00"), 0);
    CHECK_EQ(parse_hhmm("24:00"), -1);
    CHECK_EQ(parse_hhmm("9:00"), -1);
    CHECK_EQ(parse_hhmm("bad"), -1);
}

// ---- matching ----

void test_subject_must_match() {
    RuleEngine e;
    e.set_rules({make_rule("r1", Subject::Person, "any", 0, 0)});
    Facts animal;
    animal.animal = true;
    CHECK(e.match(animal, "backyard", Clock{600, 1000}).empty());
    CHECK_EQ(e.match(person(), "backyard", Clock{600, 1000}).size(), size_t(1));
}

void test_zone_specific_vs_any() {
    RuleEngine e;
    e.set_rules({make_rule("r1", Subject::Person, "backyard", 0, 0)});
    CHECK(e.match(person(), "driveway", Clock{600, 1000}).empty());   // wrong zone
    CHECK_EQ(e.match(person(), "backyard", Clock{600, 2000}).size(), size_t(1));

    RuleEngine any;
    any.set_rules({make_rule("r2", Subject::Person, "any", 0, 0)});
    CHECK_EQ(any.match(person(), "anywhere", Clock{600, 1000}).size(), size_t(1));
}

void test_time_gates_match() {
    RuleEngine e;
    e.set_rules({make_rule("r1", Subject::Person, "any", 22 * 60, 6 * 60)});  // overnight
    CHECK(e.match(person(), "yard", Clock{12 * 60, 1000}).empty());          // noon: no
    CHECK_EQ(e.match(person(), "yard", Clock{23 * 60, 2000}).size(), size_t(1));  // 11pm: yes
}

void test_cooldown_suppresses_then_allows() {
    RuleEngine e;
    e.set_rules({make_rule("r1", Subject::Person, "any", 0, 0, /*cooldown=*/120)});
    CHECK_EQ(e.match(person(), "yard", Clock{600, 1000}).size(), size_t(1));  // fires
    CHECK(e.match(person(), "yard", Clock{600, 1060}).empty());               // +60s: cooled down
    CHECK(e.match(person(), "yard", Clock{600, 1119}).empty());               // +119s: still
    CHECK_EQ(e.match(person(), "yard", Clock{600, 1120}).size(), size_t(1));  // +120s: fires again
}

void test_multiple_rules_one_pass() {
    // Two rules on different subjects; facts with both present fire both.
    RuleEngine e;
    e.set_rules({make_rule("r_person", Subject::Person, "any", 0, 0),
                 make_rule("r_vehicle", Subject::Vehicle, "any", 0, 0)});
    Facts both;
    both.person = true;
    both.vehicle = true;
    auto d = e.match(both, "yard", Clock{600, 1000});
    CHECK_EQ(d.size(), size_t(2));
}

void test_decision_carries_actions() {
    RuleEngine e;
    e.set_rules({make_rule("r1", Subject::Person, "any", 0, 0)});
    auto d = e.match(person(), "yard", Clock{600, 1000});
    CHECK_EQ(d.size(), size_t(1));
    CHECK_EQ(d[0].rule_id, std::string("r1"));
    CHECK_EQ(d[0].actions.size(), size_t(1));
    CHECK_EQ(d[0].actions[0].target, std::string("topic-r1"));
}

}  // namespace

int main() {
    test_time_window_daytime();
    test_time_window_overnight();
    test_time_window_always();
    test_parse_hhmm();
    test_subject_must_match();
    test_zone_specific_vs_any();
    test_time_gates_match();
    test_cooldown_suppresses_then_allows();
    test_multiple_rules_one_pass();
    test_decision_carries_actions();
    return njtest::failures() == 0 ? 0 : 1;
}
