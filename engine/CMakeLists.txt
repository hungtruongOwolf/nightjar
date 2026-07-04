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
