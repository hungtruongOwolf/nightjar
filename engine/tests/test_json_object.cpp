#include "nightjar/json_object.h"

#include "check.h"

using namespace nightjar;

namespace {

void test_strings_and_numbers() {
    auto o = parse_flat_json_object(R"({"a":"hello","b":123,"c":"two words"})");
    CHECK(o.has_value());
    CHECK_EQ((*o)["a"], std::string("hello"));
    CHECK_EQ((*o)["b"], std::string("123"));
    CHECK_EQ((*o)["c"], std::string("two words"));
}

void test_whitespace_tolerant() {
    auto o = parse_flat_json_object("  {  \"x\" : \"y\" ,  \"n\" : 42 }  ");
    CHECK(o.has_value());
    CHECK_EQ((*o)["x"], std::string("y"));
    CHECK_EQ((*o)["n"], std::string("42"));
}

void test_empty_object() {
    auto o = parse_flat_json_object("{}");
    CHECK(o.has_value());
    CHECK_EQ(o->size(), size_t(0));
}

void test_bool_values() {
    auto o = parse_flat_json_object(R"({"t":true,"f":false})");
    CHECK(o.has_value());
    CHECK_EQ((*o)["t"], std::string("true"));
    CHECK_EQ((*o)["f"], std::string("false"));
}

void test_rejects_malformed() {
    CHECK(!parse_flat_json_object("not json").has_value());
    CHECK(!parse_flat_json_object(R"({"a":"unterminated)").has_value());
    CHECK(!parse_flat_json_object(R"({"a" "missing colon"})").has_value());
    CHECK(!parse_flat_json_object(R"({"a":1 "b":2})").has_value());  // missing comma
}

}  // namespace

int main() {
    test_strings_and_numbers();
    test_whitespace_tolerant();
    test_empty_object();
    test_bool_values();
    test_rejects_malformed();
    return njtest::failures() == 0 ? 0 : 1;
}
