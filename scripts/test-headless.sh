#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf -- "$test_dir"' EXIT

sources=(
  tests/MaterialDBFake.cpp
  src/WorldEngine.cpp
  src/WorldEnvironment.cpp
  src/WorldPhysics.cpp
  src/WorldArt.cpp
  src/Biology.cpp
  src/Chunk.cpp
)

cd "$project_dir"
for name in WorldSmokeTest NatureTest ChemistryTest PhysicsTest FlowAimUnitsTest StructuralLaboratoryTest StructuralDynamicsTest SimulationSettingsTest; do
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itests/fakes -Isrc \
      "tests/${name}.cpp" "${sources[@]}" -o "$test_dir/$name"
  "$test_dir/$name"
done

g++ -std=c++17 -O2 -Itests/fakes -Isrc \
    tests/Benchmark.cpp "${sources[@]}" -o "$test_dir/Benchmark"
"$test_dir/Benchmark" 512 512 1
"$test_dir/Benchmark" 1024 768 0
