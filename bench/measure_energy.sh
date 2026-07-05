#!/usr/bin/env bash
# measure_energy.sh — perf-per-watt on Apple Silicon via powermetrics idle-delta.
# Measures quiescent package power, then package power while a workload runs, and
# reports the net active power + energy. Needs sudo (powermetrics).
#
#   sudo bench/measure_energy.sh "<workload command>"
# e.g.
#   sudo bench/measure_energy.sh "engine/build/nightjar_replay_vlm clip model.gguf mmproj.gguf 30"
#
# Feed the reported J/inference back into nightjar_ablation --j-per-infer.

set -euo pipefail
WORKLOAD="${1:?usage: sudo measure_energy.sh \"<command>\"}"
INTERVAL_MS=200
IDLE_SAMPLES=50   # ~10s idle baseline
OUT="$(mktemp -d)/pm.txt"

# powermetrics reports "CPU Power" / "GPU Power" in mW under the cpu_power sampler.
avg_package_mw() {  # reads a powermetrics capture on stdin
  awk '/(CPU|GPU) Power:/ { for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/){s+=$i; break} } /Combined Power/ {c+=$3; n++} END { if(n>0) print c/n; else print s }'
}

echo "measuring idle baseline (~$((IDLE_SAMPLES*INTERVAL_MS/1000))s, stay quiet)..."
powermetrics --samplers cpu_power -i "$INTERVAL_MS" -n "$IDLE_SAMPLES" 2>/dev/null > "$OUT.idle"
IDLE_MW=$(grep -E "Combined Power" "$OUT.idle" | awk '{s+=$3;n++} END{if(n)print s/n; else print 0}')
echo "idle combined power: ${IDLE_MW} mW"

echo "running workload while sampling..."
powermetrics --samplers cpu_power -i "$INTERVAL_MS" 2>/dev/null > "$OUT.load" &
PM_PID=$!
START=$(date +%s.%N)
bash -c "$WORKLOAD" > "$OUT.workout" 2>&1 || true
END=$(date +%s.%N)
kill "$PM_PID" 2>/dev/null || true
wait "$PM_PID" 2>/dev/null || true

LOAD_MW=$(grep -E "Combined Power" "$OUT.load" | awk '{s+=$3;n++} END{if(n)print s/n; else print 0}')
DUR=$(echo "$END - $START" | bc)
NET_MW=$(echo "$LOAD_MW - $IDLE_MW" | bc)
NET_J=$(echo "scale=2; $NET_MW/1000 * $DUR" | bc)

echo "----"
echo "load combined power:  ${LOAD_MW} mW"
echo "net active power:     ${NET_MW} mW"
echo "duration:             ${DUR} s"
echo "net energy:           ${NET_J} J"
echo "workload output tail:"; tail -5 "$OUT.workout"
echo "----"
echo "If the workload printed 'vlm_inferences=N', J/inference = net_energy / N."
