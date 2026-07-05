# Nightjar, top-level convenience targets.
# The engine is portable C++ (CMake); these wrap the common actions so a judge
# can validate without knowing the layout (see JUDGES.md).

ENGINE := engine
BUILD  := $(ENGINE)/build
GEN    := "Unix Makefiles"

.PHONY: build test demo clean

## build, configure + compile the engine, tests, and demo
build:
	cmake -S $(ENGINE) -B $(BUILD) -G $(GEN) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD) -j

## test, run the full engine test suite
test: build
	cd $(BUILD) && ctest --output-on-failure

## demo, replay a synthetic clip through the full pipeline (no iPhone, no model)
demo: build
	$(BUILD)/nightjar_demo

## bench, gate micro-benchmark (per-step scalar vs NEON, full-gate us/frame)
bench: build
	$(BUILD)/gate_bench

clean:
	rm -rf $(BUILD)
