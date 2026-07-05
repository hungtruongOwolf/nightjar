#include "nightjar/predicate_debouncer.h"

#include "check.h"

using namespace nightjar;

namespace {

bool step(PredicateDebouncer& d, bool raw) { return d.update({{"p", raw}})["p"]; }

void test_ignores_single_blip() {
    PredicateDebouncer d({/*on*/ 2, /*off*/ 3});
    CHECK(!step(d, false));
    CHECK(!step(d, true));   // 1 true, not enough (on_streak=2)
    CHECK(!step(d, false));  // blip gone, still off
    CHECK(!step(d, false));
}

void test_turns_on_after_streak() {
    PredicateDebouncer d({2, 3});
    CHECK(!step(d, true));  // 1
    CHECK(step(d, true));   // 2 consecutive -> on
    CHECK(step(d, true));
}

void test_slow_to_clear() {
    PredicateDebouncer d({2, 3});
    step(d, true);
    step(d, true);  // on now
    CHECK(step(d, false));  // 1 false, still on (off_streak=3)
    CHECK(step(d, false));  // 2, still on
    CHECK(!step(d, false));  // 3 consecutive false -> off
}

void test_disagreement_streak_resets() {
    PredicateDebouncer d({3, 3});
    step(d, true);  // 1 toward on
    step(d, false); // resets the toward-on streak (agrees with off state)
    step(d, true);  // 1 again
    step(d, true);  // 2
    CHECK(!step(d, false));  // never reached 3 consecutive true -> still off
}

void test_multiple_predicates_independent() {
    PredicateDebouncer d({1, 1});
    auto out = d.update({{"person", true}, {"package", false}});
    CHECK(out["person"]);
    CHECK(!out["package"]);
}

}  // namespace

int main() {
    test_ignores_single_blip();
    test_turns_on_after_streak();
    test_slow_to_clear();
    test_disagreement_streak_resets();
    test_multiple_predicates_independent();
    return njtest::failures() == 0 ? 0 : 1;
}
