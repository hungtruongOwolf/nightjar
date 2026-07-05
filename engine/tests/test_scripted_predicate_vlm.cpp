#include "nightjar/predicate_vlm.h"

#include "check.h"

using namespace nightjar;

namespace {

std::vector<Predicate> preds() {
    return {{"person", "q1"}, {"package", "q2"}, {"uniform", "q3"}};
}

void test_fixed_answers() {
    ScriptedPredicateVlm vlm({{"person", true}, {"package", true}});
    auto r = vlm.evaluate(CandidateFrame{}, preds());
    CHECK(r.get("person"));
    CHECK(r.get("package"));
    CHECK(!r.get("uniform"));  // missing => false
    CHECK_EQ(vlm.calls(), uint64_t(1));
}

void test_callback_form() {
    // Everything ending in the letter of interest...
    ScriptedPredicateVlm vlm(
        [](const CandidateFrame&, const Predicate& p) { return p.id == "uniform"; });
    auto r = vlm.evaluate(CandidateFrame{}, preds());
    CHECK(!r.get("person"));
    CHECK(r.get("uniform"));
}

void test_only_requested_predicates_answered() {
    ScriptedPredicateVlm vlm({{"person", true}});
    auto r = vlm.evaluate(CandidateFrame{}, {{"person", "q"}});
    CHECK_EQ(r.answers.size(), size_t(1));  // didn't invent answers for un-asked predicates
}

}  // namespace

int main() {
    test_fixed_answers();
    test_callback_form();
    test_only_requested_predicates_answered();
    return njtest::failures() == 0 ? 0 : 1;
}
