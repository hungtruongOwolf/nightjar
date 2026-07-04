#include "nightjar/alert_sink.h"

#include <memory>

#include "check.h"

using namespace nightjar;

namespace {

Alert make_alert(const std::string& rule, const std::string& text) {
    Alert a;
    a.rule_id = rule;
    a.one_liner = text;
    a.unix_s = 1000;
    return a;
}

void test_capturing_sink_records() {
    CapturingSink sink;
    CHECK_EQ(sink.count(), size_t(0));
    sink.send(make_alert("r1", "person in backyard"));
    sink.send(make_alert("r2", "vehicle in driveway"));
    CHECK_EQ(sink.count(), size_t(2));
    CHECK_EQ(sink.alerts()[0].rule_id, std::string("r1"));
    CHECK_EQ(sink.alerts()[1].one_liner, std::string("vehicle in driveway"));
}

void test_multisink_fans_out_to_all() {
    auto a = std::make_shared<CapturingSink>();
    auto b = std::make_shared<CapturingSink>();
    MultiSink multi;
    multi.add(a);
    multi.add(b);
    multi.add(nullptr);  // ignored

    multi.send(make_alert("r1", "hello"));
    // Both sinks receive every alert (dual-post ntfy + Telegram).
    CHECK_EQ(a->count(), size_t(1));
    CHECK_EQ(b->count(), size_t(1));
    CHECK_EQ(a->alerts()[0].rule_id, std::string("r1"));
    CHECK_EQ(b->alerts()[0].rule_id, std::string("r1"));
}

}  // namespace

int main() {
    test_capturing_sink_records();
    test_multisink_fans_out_to_all();
    return njtest::failures() == 0 ? 0 : 1;
}
