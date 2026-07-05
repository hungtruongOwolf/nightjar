#include "nightjar/rule_compiler.h"

#include "check.h"

using namespace nightjar;

namespace {

void test_parse_loiter() {
    auto r = RuleCompiler::parse_temporal(
        R"({"subject":"person","zone":"backyard","trigger":"loiter","dwell_s":90,"start":"22:00","end":"06:00","cooldown_s":120})",
        "r", "someone loitering in the backyard at night");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Sustained);
    CHECK(r->predicate == std::string("person"));
    CHECK_EQ(r->dwell_s, 90);
    CHECK_EQ(r->zone_id, std::string("backyard"));
    CHECK(r->time_window.contains(23 * 60));
}

void test_parse_appears() {
    auto r = RuleCompiler::parse_temporal(
        R"({"subject":"vehicle","zone":"driveway","trigger":"appears","dwell_s":0,"start":"00:00","end":"00:00","cooldown_s":120})",
        "r", "a car in the driveway");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Appears);
    CHECK(r->predicate == std::string("vehicle"));
}

void test_parse_left_behind() {
    auto r = RuleCompiler::parse_temporal(
        R"({"subject":"package","zone":"frontdoor","trigger":"left_behind","dwell_s":0,"start":"00:00","end":"00:00","cooldown_s":120})",
        "r", "a package left at the door");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::LeftBehind);
    CHECK(r->predicate == std::string("package"));
    CHECK(r->actor_predicate == std::string("person"));
}

void test_loiter_defaults_dwell() {
    auto r = RuleCompiler::parse_temporal(
        R"({"subject":"person","zone":"any","trigger":"loiter","dwell_s":0,"start":"00:00","end":"00:00","cooldown_s":120})",
        "r", "loitering");
    CHECK(r.has_value());
    CHECK_EQ(r->dwell_s, 60);  // 0 -> sensible default
}

void test_rejects_bad_trigger() {
    CHECK(!RuleCompiler::parse_temporal(
               R"({"subject":"person","zone":"any","trigger":"teleport","dwell_s":0,"start":"00:00","end":"00:00","cooldown_s":120})",
               "r", "t")
               .has_value());
}

void test_compile_temporal_with_fake() {
    TextInferFn fake = [](const std::string&, const std::string&) {
        return R"({"subject":"person","zone":"backyard","trigger":"loiter","dwell_s":60,"start":"22:00","end":"06:00","cooldown_s":120})";
    };
    RuleCompiler c("{RULE_TEXT}", "g", fake);
    auto r = c.compile_temporal("someone hangs around the backyard at night", "r1");
    CHECK(r.has_value());
    CHECK(r->trigger == Trigger::Sustained);
    CHECK_EQ(r->raw_text, std::string("someone hangs around the backyard at night"));
}

}  // namespace

int main() {
    test_parse_loiter();
    test_parse_appears();
    test_parse_left_behind();
    test_loiter_defaults_dwell();
    test_rejects_bad_trigger();
    test_compile_temporal_with_fake();
    return njtest::failures() == 0 ? 0 : 1;
}
