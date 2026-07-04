# Nightjar engine — portable C++ core (no platform dependencies).
# The iOS/macOS shells link against this; it never links back to them.
cmake_minimum_required(VERSION 3.20)
project(nightjar_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# Core library — grows one module per commit.
add_library(nightjar
  src/pgm.cpp
  src/file_replay_source.cpp
  src/telemetry.cpp
  src/gate_steps.cpp
  src/motion_gate.cpp
  src/frame_ops.cpp
  src/best_frame_selector.cpp
  src/rule.cpp
  src/rule_engine.cpp
  src/alert_sink.cpp
  src/scripted_vlm_worker.cpp
  src/pipeline.cpp
)
target_include_directories(nightjar PUBLIC include)

# Tests: plain executables registered with CTest (zero external deps so the
# repo builds offline — matters for a $0 project a stranger will clone).
enable_testing()
add_subdirectory(tests)

# Micro-benchmarks (not tests) — build with -DNIGHTJAR_BENCH=ON.
option(NIGHTJAR_BENCH "Build micro-benchmarks" ON)
if(NIGHTJAR_BENCH)
  add_executable(gate_bench bench/gate_bench.cpp)
  target_link_libraries(gate_bench PRIVATE nightjar)
endif()

# Replay demo CLI (the `make demo` entry point).
add_executable(nightjar_demo tools/nightjar_demo.cpp)
target_link_libraries(nightjar_demo PRIVATE nightjar)

# Real llama.cpp-backed VLM worker — optional (keeps the core offline-buildable).
# Enable with -DNIGHTJAR_VLM=ON; expects llama.cpp headers/libs (brew).
option(NIGHTJAR_VLM "Build the mtmd/llama.cpp VLM worker" OFF)
if(NIGHTJAR_VLM)
  if(NOT LLAMA_CPP_ROOT)
    execute_process(COMMAND brew --prefix llama.cpp OUTPUT_VARIABLE LLAMA_CPP_ROOT
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  endif()
  message(STATUS "llama.cpp root: ${LLAMA_CPP_ROOT}")
  add_library(nightjar_vlm vlm/mtmd_vlm_worker.cpp)
  target_include_directories(nightjar_vlm PUBLIC vlm ${LLAMA_CPP_ROOT}/include /opt/homebrew/opt/ggml/include)
  target_link_directories(nightjar_vlm PUBLIC /opt/homebrew/opt/ggml/lib)
  target_link_directories(nightjar_vlm PUBLIC ${LLAMA_CPP_ROOT}/lib)
  target_link_libraries(nightjar_vlm PUBLIC nightjar llama mtmd)

  add_executable(vlm_probe vlm/vlm_probe.cpp)
  target_link_libraries(vlm_probe PRIVATE nightjar_vlm)
endif()

if(NIGHTJAR_VLM)
  add_executable(nightjar_replay_vlm vlm/nightjar_replay_vlm.cpp)
  target_link_libraries(nightjar_replay_vlm PRIVATE nightjar_vlm)
endif()
