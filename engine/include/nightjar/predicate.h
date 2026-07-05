#pragma once

#include <string>
#include <vector>

namespace nightjar {

// A per-frame yes/no fact the VLM answers, defined by a natural-language
// question. This is what lifts Nightjar past a closed-vocabulary detector:
// instead of fixed classes, a rule references arbitrary attribute predicates
// ("a person wearing a delivery uniform", "a person carrying a box") that the
// VLM evaluates one focused question at a time (KT: the small model is reliable
// one-fact-at-a-time). The engine only ever sees the boolean answers.
struct Predicate {
    std::string id;        // short handle used by rules and observations
    std::string question;  // the y/n question posed to the VLM
};

// The subject predicates that every build ships (the closed core), expressed as
// predicates so they compose with attribute predicates uniformly.
inline std::vector<Predicate> core_predicates() {
    return {
        {"person", "Is there a person in this image? Answer y or n."},
        {"vehicle", "Is there a vehicle in this image? Answer y or n."},
        {"animal", "Is there an animal in this image? Answer y or n."},
        {"package", "Is there a package or box in this image? Answer y or n."},
    };
}

}  // namespace nightjar
