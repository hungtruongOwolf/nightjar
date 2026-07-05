#!/usr/bin/env bash
# KT1, VLM latency kill-test (design doc §8).
# Benchmarks SmolVLM-500M-Instruct GGUF through llama-mtmd-cli, CPU-only,
# over a directory of test images, and reports p50/p90/p99 wall-clock per
# inference plus llama.cpp's own prompt-eval/eval timings.
#
# v0.3 scope: 2 quant configs (Q4_0 vs Q4_K_M) x 2 grammars (full JSON vs
# compact y/n), CPU-only (-ngl 0). Metal / res-336 runs open only if both
# quants fail the gate (M2 Max early signal: CPU-only p50 <= 1.5s).
#
# Usage:  bench/kt1.sh <images_dir> [runs_per_image]
# Output: bench/out/kt1-<timestamp>/  (raw logs + summary.md)

set -euo pipefail

IMAGES_DIR="${1:?usage: bench/kt1.sh <images_dir> [runs_per_image]}"
RUNS="${2:-5}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODELS_DIR="$REPO_ROOT/models"
OUT_DIR="$REPO_ROOT/bench/out/kt1-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$MODELS_DIR" "$OUT_DIR"

HF_BASE="https://huggingface.co/ggml-org/SmolVLM-500M-Instruct-GGUF/resolve/main"
MMPROJ="mmproj-SmolVLM-500M-Instruct-f16.gguf"
QUANTS=("SmolVLM-500M-Instruct-Q4_0.gguf" "SmolVLM-500M-Instruct-Q4_K_M.gguf")
GRAMMARS=("$REPO_ROOT/grammar/facts.gbnf" "$REPO_ROOT/grammar/facts_compact.gbnf")

PROMPT_JSON='Look at the image. Report which of these are visible: person, vehicle, animal, package. Answer with the JSON object only.'
PROMPT_COMPACT='Look at the image. Four questions, answer each with y or n, in this exact order, no spaces: person visible? vehicle visible? animal visible? package visible?'

command -v llama-mtmd-cli >/dev/null || {
  echo "llama-mtmd-cli not found, install with: brew install llama.cpp" >&2; exit 1; }

fetch() { # fetch <filename>
  local f="$MODELS_DIR/$1"
  [[ -f "$f" ]] && return 0
  echo "downloading $1 ..."
  curl -fL --progress-bar -o "$f.part" "$HF_BASE/$1" && mv "$f.part" "$f"
}

fetch "$MMPROJ"
for q in "${QUANTS[@]}"; do fetch "$q"; done

# ISA line (KT6 companion) goes into every report.
{ echo "## Environment"
  echo '```'
  sysctl -n machdep.cpu.brand_string
  sysctl hw.optional 2>/dev/null | grep -iE "dotprod|i8mm|sme|neon" || true
  llama-mtmd-cli --version 2>&1 | head -1 || true
  echo "commit: $(git -C "$REPO_ROOT" rev-parse --short HEAD)"
  echo '```' ; } > "$OUT_DIR/summary.md"

percentile() { # percentile <p> ; reads sorted ms values on stdin
  awk -v p="$1" '{a[NR]=$1} END{ if(NR==0){print "n/a"; exit}
    i=int(p/100*(NR-1))+1; printf "%.0f", a[i] }'
}

shopt -s nullglob
IMAGES=("$IMAGES_DIR"/*.{jpg,jpeg,png})
[[ ${#IMAGES[@]} -gt 0 ]] || { echo "no images in $IMAGES_DIR" >&2; exit 1; }
echo "images: ${#IMAGES[@]} × runs: $RUNS"

for q in "${QUANTS[@]}"; do
  for gi in 0 1; do
    g="${GRAMMARS[$gi]}"
    [[ $gi -eq 0 ]] && prompt="$PROMPT_JSON" || prompt="$PROMPT_COMPACT"
    cfg="$(basename "$q" .gguf | sed 's/SmolVLM-500M-Instruct-//')-$(basename "$g" .gbnf)"
    log="$OUT_DIR/$cfg.log"; times="$OUT_DIR/$cfg.ms"
    echo "=== $cfg ==="
    for img in "${IMAGES[@]}"; do
      for ((r=1; r<=RUNS; r++)); do
        t0=$(python3 -c 'import time; print(int(time.time()*1000))')
        llama-mtmd-cli -m "$MODELS_DIR/$q" --mmproj "$MODELS_DIR/$MMPROJ" \
          --image "$img" -p "$prompt" --grammar-file "$g" \
          --temp 0 -n 64 -ngl 0 >>"$log" 2>&1
        t1=$(python3 -c 'import time; print(int(time.time()*1000))')
        echo $((t1 - t0)) >> "$times"
      done
    done
    sort -n "$times" > "$times.sorted"
    p50=$(percentile 50 < "$times.sorted"); p90=$(percentile 90 < "$times.sorted")
    p99=$(percentile 99 < "$times.sorted")
    echo "| $cfg | $p50 | $p90 | $p99 |" >> "$OUT_DIR/results.rows"
    echo "  wall-clock ms: p50=$p50 p90=$p90 p99=$p99  (includes model load, see log timings for encode/prefill/decode split)"
  done
done

{ echo; echo "## Results (wall-clock ms per invocation, includes process+model load)"
  echo "| config | p50 | p90 | p99 |"; echo "|---|---|---|---|"
  cat "$OUT_DIR/results.rows"
  echo
  echo "NOTE: llama-mtmd-cli reloads the model each call; the in-app number"
  echo "will be lower (single warm context). Extract encode/prefill/decode"
  echo "from the logs' llama_perf lines for the per-stage split."
} >> "$OUT_DIR/summary.md"

echo; echo "summary: $OUT_DIR/summary.md"
