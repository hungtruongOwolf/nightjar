#pragma once

#include <string>

namespace nightjar {

// The v1 closed vocabulary (NG6): person / vehicle / animal / package.
enum class Subject { Person, Vehicle, Animal, Package };

// What the VLM reported about one candidate frame. Booleans drive the rule
// engine; the three timings are mandatory (design doc §5.4) so every published
// latency number can be split encode/prefill/decode.
struct Facts {
    bool person = false;
    bool vehicle = false;
    bool animal = false;
    bool package = false;

    float encode_ms = 0.0f;
    float prefill_ms = 0.0f;
    float decode_ms = 0.0f;
    std::string raw_json;

    bool has(Subject s) const {
        switch (s) {
            case Subject::Person: return person;
            case Subject::Vehicle: return vehicle;
            case Subject::Animal: return animal;
            case Subject::Package: return package;
        }
        return false;
    }
};

}  // namespace nightjar
